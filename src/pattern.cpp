#include "pattern.hpp"

#include <stdexcept>
#include <cstring>

#include <immintrin.h>

/**
 * @brief Internal structure for SIMD-optimized pattern matching
 *
 * Stores precomputed data for AVX-512 pattern matching, including the pattern bytes,
 * mask bytes, and SIMD registers loaded with the pattern data.
 */
struct pattern_simd_t {
  std::vector<std::uint8_t> bytes;       ///< Pattern bytes (wildcards as 0x00)
  std::vector<std::uint8_t> mask_bytes;  ///< Mask bytes (0xFF = check, 0x00 = wildcard)
  std::size_t length = 0;                ///< Pattern length in bytes
  __m512i simd_pattern = {};             ///< AVX-512 register with pattern data
  __m512i simd_mask = {};                ///< AVX-512 register with mask data
  bool can_use_full_simd = false;        ///< True if pattern is SIMD-compatible
};

/**
 * @brief Detects CPU support for AVX-512F instructions
 *
 * Uses CPUID to check if the processor supports AVX-512 Foundation (AVX-512F).
 * Result is cached after first call for performance.
 *
 * @return true if AVX-512F is supported, false otherwise
 */
static bool has_avx512f() {
  // Cache the result to avoid repeated CPUID calls
  static bool checked = false;
  static bool supported = false;

  if (!checked) {
    int cpuInfo[4];
    // Get maximum supported CPUID function
    __cpuid(cpuInfo, 0);
    int max_function_id = cpuInfo[0];

    // Extended features are available via function 7
    if (max_function_id >= 7) {
      __cpuidex(cpuInfo, 7, 0);
      // Check AVX-512F bit (bit 16 in EBX register)
      supported = (cpuInfo[1] & (1 << 16)) != 0;
    }
    checked = true;
  }

  return supported;
}

/**
 * @brief Parses a hexadecimal pattern string into structured pattern bytes
 *
 * Implementation details:
 * - Processes pattern string character by character
 * - Skips whitespace (space, tab, newline, carriage return)
 * - Converts hex digits to nibble values (0-15)
 * - Treats '?' as wildcard nibbles
 * - Each byte requires exactly 2 characters (hex digits or wildcards)
 *
 * @param pattern The pattern string to parse
 * @return Vector of pattern_byte_t structures
 * @throws std::runtime_error if pattern is malformed
 */
std::vector<pattern_byte_t> parse_pattern(const std::string& pattern) {
  std::vector<pattern_byte_t> pattern_bytes;
  size_t i = 0;

  while (i < pattern.length()) {
    // Skip all whitespace
    if (pattern[i] == ' ' || pattern[i] == '\t' || pattern[i] == '\n' || pattern[i] == '\r') {
      ++i;
      continue;
    }

    // Parse one byte (two nibbles: high then low)
    pattern_byte_t byte = {};
    for (auto& j : byte.nibble) {
      if (i >= pattern.length()) {
        throw std::runtime_error("Incomplete byte in pattern");
      }

      if (pattern[i] == '?') {
        // Wildcard nibble - matches any value
        j.wildcard = true;
        j.data = 0;
        ++i;
      } else if (isxdigit(pattern[i])) {
        // Hex digit - matches specific value
        j.wildcard = false;
        // Convert hex char to nibble value: '0'-'9' -> 0-9, 'a'-'f' -> 10-15
        j.data = static_cast<std::uint8_t>(
          std::isdigit(pattern[i]) ? pattern[i] - '0' : std::tolower(pattern[i]) - 'a' + 10
        );
        ++i;
      } else {
        throw std::runtime_error("Invalid character in pattern");
      }
    }

    pattern_bytes.push_back(byte);
  }

  return pattern_bytes;
}

/**
 * @brief Tests if a byte matches a pattern byte
 *
 * Splits the byte into high and low nibbles and checks each against the
 * corresponding pattern nibble. A nibble matches if it's a wildcard or
 * if the values are equal.
 *
 * @param byte The byte value to test
 * @param pattern_byte The pattern byte to match against
 * @return true if both nibbles match, false otherwise
 */
bool match_pattern_byte(std::uint8_t byte, const pattern_byte_t& pattern_byte) {
  // Extract nibbles from byte
  std::uint8_t high_nibble = (byte >> 4) & 0x0F;  // Upper 4 bits
  std::uint8_t low_nibble = byte & 0x0F;          // Lower 4 bits

  // Check if each nibble matches (wildcard or exact match)
  bool high_match = pattern_byte.nibble[0].wildcard || (high_nibble == pattern_byte.nibble[0].data);
  bool low_match = pattern_byte.nibble[1].wildcard || (low_nibble == pattern_byte.nibble[1].data);

  return high_match && low_match;
}

/**
 * @brief Prepares a pattern for SIMD-accelerated matching
 *
 * Analyzes the pattern to determine if it's suitable for AVX-512 SIMD matching:
 * - Pattern must be ≤64 bytes (AVX-512 register size)
 * - Only full-byte wildcards ("??") or full-byte matches ("AB") are SIMD-compatible
 * - Mixed wildcards like "?F" or "A?" require scalar fallback
 *
 * If compatible, converts the pattern into SIMD registers where:
 * - simd_pattern contains the byte values to match
 * - simd_mask contains 0xFF for bytes to check, 0x00 for wildcards
 *
 * @param pattern The parsed pattern to prepare
 * @return pattern_simd_t structure with SIMD data and compatibility flag
 */
static pattern_simd_t prepare_simd_pattern(const std::vector<pattern_byte_t>& pattern) {
  pattern_simd_t result;
  result.length = pattern.size();

  // Only enable SIMD if CPU supports AVX-512F and pattern fits in one register (64 bytes)
  result.can_use_full_simd = has_avx512f() && !pattern.empty() && pattern.size() <= 64;

  if (result.can_use_full_simd) {
    result.bytes.resize(pattern.size());
    result.mask_bytes.resize(pattern.size());

    // Analyze each byte to determine if pattern is SIMD-compatible
    for (size_t i = 0; i < pattern.size(); ++i) {
      const auto& pb = pattern[i];

      bool both_wildcard = pb.nibble[0].wildcard && pb.nibble[1].wildcard;
      if (both_wildcard) {
        // Full byte wildcard (??): mask = 0x00 means ignore this byte
        result.bytes[i] = 0x00;
        result.mask_bytes[i] = 0x00;
      } else if (!pb.nibble[0].wildcard && !pb.nibble[1].wildcard) {
        // Both nibbles specified (AB): combine into full byte, mask = 0xFF means check all bits
        result.bytes[i] = (pb.nibble[0].data << 4) | pb.nibble[1].data;
        result.mask_bytes[i] = 0xFF;
      } else {
        // Mixed wildcard (?F or A?): can't efficiently handle with simple SIMD mask
        result.can_use_full_simd = false;
        break;
      }
    }

    if (result.can_use_full_simd && result.length <= 64) {
      // Create aligned buffers for loading into AVX-512 registers
      alignas(64) std::uint8_t pattern_buffer[64] = {0};
      alignas(64) std::uint8_t mask_buffer[64] = {0};

      // Copy pattern data into aligned buffers
      std::memcpy(pattern_buffer, result.bytes.data(), result.length);
      std::memcpy(mask_buffer, result.mask_bytes.data(), result.length);

      // Load into SIMD registers for fast parallel comparison
      result.simd_pattern = _mm512_load_si512((__m512i*)pattern_buffer);
      result.simd_mask = _mm512_load_si512((__m512i*)mask_buffer);
    }
  }

  return result;
}

/**
 * @brief Performs SIMD pattern matching at a specific position
 *
 * Uses AVX-512 instructions to compare 64 bytes at once:
 * 1. Load 64 bytes from data into SIMD register
 * 2. XOR with pattern register (differences become non-zero)
 * 3. AND with mask register (ignore wildcard positions)
 * 4. Compare result to zero (all checked bytes must match)
 * 5. Apply length mask to only check pattern.length bytes
 *
 * @param data Pointer to data position to check (must have ≥64 bytes available)
 * @param pattern The prepared SIMD pattern structure
 * @return true if pattern matches at this position, false otherwise
 */
static inline bool match_at_position_avx512(
  const std::uint8_t* data, const pattern_simd_t& pattern
) {
  if (!pattern.can_use_full_simd || !has_avx512f()) {
    return false;
  }

  // Load 64 bytes from data (unaligned load is fine, just slightly slower)
  __m512i data_vec = _mm512_loadu_si512((__m512i*)data);
  
  // XOR data with pattern - matching bytes become 0, different bytes become non-zero
  __m512i xor_result = _mm512_xor_si512(data_vec, pattern.simd_pattern);
  
  // Apply mask - zeros out wildcard positions (0x00 mask) so they don't affect comparison
  __m512i masked_diff = _mm512_and_si512(xor_result, pattern.simd_mask);
  
  // Compare with zero - returns bitmask where 1 = byte matched (was zero after mask)
  __mmask64 cmp_mask = _mm512_cmpeq_epi8_mask(masked_diff, _mm512_setzero_si512());

  // Create mask for actual pattern length (we may not use all 64 bytes)
  uint64_t length_mask =
    (pattern.length == 64) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << pattern.length) - 1);

  // Pattern matches if all relevant bytes matched
  return (cmp_mask & length_mask) == length_mask;
}

/**
 * @brief Searches for all occurrences of a pattern in a data buffer
 *
 * Uses a hybrid approach for optimal performance:
 * 1. For patterns ≤64 bytes with full-byte wildcards on AVX-512 CPUs:
 *    - Uses SIMD matching for positions where 64 bytes can be safely read
 *    - Falls back to scalar matching for final bytes if needed
 * 2. For all other cases:
 *    - Uses scalar byte-by-byte matching with early exit on mismatch
 *
 * @param data Pointer to the data buffer to search
 * @param data_size Size of the data buffer in bytes
 * @param pattern The parsed pattern to search for
 * @return Vector of byte offsets where pattern was found
 */
std::vector<std::size_t> find_pattern(
  const std::uint8_t* data, std::size_t data_size, const std::vector<pattern_byte_t>& pattern
) {
  std::vector<std::size_t> matches;

  // Early exit for invalid inputs
  if (data == nullptr || pattern.empty() || data_size == 0 || data_size < pattern.size()) {
    return matches;
  }

  // Last position where pattern could possibly start
  const std::size_t end_pos = data_size - pattern.size();

  // Attempt SIMD acceleration for suitable patterns on capable CPUs
  if (pattern.size() <= 64 && has_avx512f()) {
    pattern_simd_t simd_pattern = prepare_simd_pattern(pattern);

    if (simd_pattern.can_use_full_simd && data_size >= 64) {
      // SIMD path: we can safely read 64 bytes up to this position
      const std::size_t simd_end_pos = (data_size >= 64) ? data_size - 64 : 0;

      // Process all positions where we can safely load 64 bytes using SIMD
      for (std::size_t i = 0; i <= end_pos && i <= simd_end_pos; ++i) {
        if (match_at_position_avx512(data + i, simd_pattern)) {
          matches.push_back(i);
        }
      }

      // Handle tail positions with scalar matching (if pattern extends beyond safe SIMD region)
      const std::size_t scalar_start = simd_end_pos + 1;
      if (scalar_start <= end_pos) {
        for (std::size_t i = scalar_start; i <= end_pos; ++i) {
          bool match = true;
          // Check each byte in pattern
          for (std::size_t j = 0; j < pattern.size(); ++j) {
            if (!match_pattern_byte(data[i + j], pattern[j])) {
              match = false;
              break;  // Early exit on first mismatch
            }
          }
          if (match) {
            matches.push_back(i);
          }
        }
      }

      return matches;
    }
  }

  // Scalar fallback: byte-by-byte matching for all patterns that can't use SIMD
  for (std::size_t i = 0; i <= end_pos; ++i) {
    bool match = true;
    // Check each byte in pattern
    for (std::size_t j = 0; j < pattern.size(); ++j) {
      if (!match_pattern_byte(data[i + j], pattern[j])) {
        match = false;
        break;  // Early exit on first mismatch
      }
    }
    if (match) {
      matches.push_back(i);
    }
  }

  return matches;
}

/**
 * @brief Applies a pattern-based patch to a data buffer
 *
 * Iterates through each byte in the replacement pattern and constructs the new
 * byte value:
 * - For non-wildcard nibbles: uses the specified value from the pattern
 * - For wildcard nibbles: preserves the original data
 *
 * This allows selective modification of bytes. For example:
 * - "A?" preserves the low nibble while setting high nibble to A
 * - "?B" preserves the high nibble while setting low nibble to B
 * - "AB" replaces the entire byte with 0xAB
 * - "??" leaves the byte unchanged (effectively a no-op)
 *
 * @param data Pointer to the data buffer to modify
 * @param data_size Size of the data buffer in bytes
 * @param replace_pattern The pattern specifying which bytes/nibbles to replace
 * @throws std::invalid_argument if data pointer is null
 * @throws std::out_of_range if patch size exceeds data size
 * @throws std::runtime_error if nibble data exceeds valid range (0-15)
 */
void apply_pattern_patch(
  std::uint8_t* data, std::size_t data_size, const std::vector<pattern_byte_t>& replace_pattern
) {
  if (data == nullptr) {
    throw std::invalid_argument("Data pointer is null");
  }

  if (replace_pattern.empty()) {
    return;  // Nothing to patch
  }

  if (replace_pattern.size() > data_size) {
    throw std::out_of_range("Patch size exceeds data size");
  }

  // Process each byte in the replacement pattern
  for (std::size_t i = 0; i < replace_pattern.size(); ++i) {
    std::uint8_t new_byte = 0;

    // Build byte from two nibbles: j=0 is high nibble, j=1 is low nibble
    for (int j = 0; j < 2; ++j) {
      if (!replace_pattern[i].nibble[j].wildcard) {
        // Non-wildcard: use the replacement value
        
        // Validate nibble data is in valid range (0-15)
        if (replace_pattern[i].nibble[j].data > 15) {
          throw std::runtime_error("Invalid nibble data: value exceeds 15");
        }

        // Shift nibble to correct position: high nibble << 4, low nibble << 0
        new_byte |= (replace_pattern[i].nibble[j].data << ((1 - j) * 4));
      } else {
        // Wildcard: preserve the original nibble value
        // 0xF0 for high nibble (j=0), 0x0F for low nibble (j=1)
        new_byte |= (data[i] & (0xF0 >> (j * 4)));
      }
    }
    
    // Write the constructed byte back to data
    data[i] = new_byte;
  }
}
