#include "pattern.hpp"

#include <immintrin.h>

#include <stdexcept>
#include <cstring>

struct pattern_simd_t {
  std::vector<std::uint8_t> bytes;
  std::vector<std::uint8_t> mask_bytes;
  std::size_t length = 0;
  __m512i simd_pattern = {};
  __m512i simd_mask = {};
  bool can_use_full_simd = false;
};

std::vector<pattern_byte_t> parse_pattern(const std::string& pattern) {
  std::vector<pattern_byte_t> pattern_bytes;
  size_t i = 0;

  while (i < pattern.length()) {
    if (pattern[i] == ' ') {
      ++i;
      continue;
    }

    pattern_byte_t byte = {};
    for (auto& j : byte.nibble) {
      if (i >= pattern.length()) {
        throw std::runtime_error("Incomplete byte in pattern");
      }

      if (pattern[i] == '?') {
        j.wildcard = true;
        j.data = 0;
        ++i;
      } else if (isxdigit(pattern[i])) {
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

bool match_pattern_byte(std::uint8_t byte, const pattern_byte_t& pattern_byte) {
  std::uint8_t high_nibble = (byte >> 4) & 0x0F;
  std::uint8_t low_nibble = byte & 0x0F;

  bool high_match = pattern_byte.nibble[0].wildcard || (high_nibble == pattern_byte.nibble[0].data);
  bool low_match = pattern_byte.nibble[1].wildcard || (low_nibble == pattern_byte.nibble[1].data);

  return high_match && low_match;
}

static pattern_simd_t prepare_simd_pattern(const std::vector<pattern_byte_t>& pattern) {
  pattern_simd_t result;
  result.length = pattern.size();
  result.can_use_full_simd = (pattern.size() <= 64);
  result.bytes.resize(pattern.size());
  result.mask_bytes.resize(pattern.size());

  for (size_t i = 0; i < pattern.size(); ++i) {
    const auto& pb = pattern[i];

    bool both_wildcard = pb.nibble[0].wildcard && pb.nibble[1].wildcard;
    if (both_wildcard) {
      // Full byte wildcard: don't check this byte at all
      result.bytes[i] = 0x00;
      result.mask_bytes[i] = 0x00;
    } else if (!pb.nibble[0].wildcard && !pb.nibble[1].wildcard) {
      // Both nibbles specified: combine into full byte and check everything
      result.bytes[i] = (pb.nibble[0].data << 4) | pb.nibble[1].data;
      result.mask_bytes[i] = 0xFF;
    } else {
      // Mixed wildcard (e.g., "?F" or "A?"): too complex for simple SIMD
      result.can_use_full_simd = false;
      result.bytes[i] = 0x00;
      result.mask_bytes[i] = 0x00;
    }
  }

  if (result.can_use_full_simd && result.length <= 64) {
    alignas(64) std::uint8_t pattern_buffer[64] = {0};
    alignas(64) std::uint8_t mask_buffer[64] = {0};

    std::memcpy(pattern_buffer, result.bytes.data(), result.length);
    std::memcpy(mask_buffer, result.mask_bytes.data(), result.length);

    result.simd_pattern = _mm512_load_si512((__m512i*)pattern_buffer);
    result.simd_mask = _mm512_load_si512((__m512i*)mask_buffer);
  }

  return result;
}

static inline bool match_at_position_avx512(
  const std::uint8_t* data, const pattern_simd_t& pattern
) {
  if (!pattern.can_use_full_simd) {
    return false;
  }

  __m512i data_vec = _mm512_loadu_si512((__m512i*)data);
  __m512i xor_result = _mm512_xor_si512(data_vec, pattern.simd_pattern);
  __m512i masked_diff = _mm512_and_si512(xor_result, pattern.simd_mask);
  __mmask64 cmp_mask = _mm512_cmpeq_epi8_mask(masked_diff, _mm512_setzero_si512());

  // Create mask for pattern length since we might not use all 64 bytes
  uint64_t length_mask =
    (pattern.length == 64) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << pattern.length) - 1);

  return (cmp_mask & length_mask) == length_mask;
}

std::vector<std::size_t> find_pattern(
  const std::uint8_t* data, std::size_t data_size, const std::vector<pattern_byte_t>& pattern
) {
  std::vector<std::size_t> matches;

  if (pattern.empty() || data_size < pattern.size()) {
    return matches;
  }

  const std::size_t end_pos = data_size - pattern.size();

  if (pattern.size() <= 64) {
    pattern_simd_t simd_pattern = prepare_simd_pattern(pattern);

    if (simd_pattern.can_use_full_simd) {
      for (std::size_t i = 0; i <= end_pos; ++i) {
        if (match_at_position_avx512(data + i, simd_pattern)) {
          matches.push_back(i);
        }
      }
      return matches;
    }
  }

  for (std::size_t i = 0; i <= end_pos; ++i) {
    bool match = true;
    for (std::size_t j = 0; j < pattern.size(); ++j) {
      if (!match_pattern_byte(data[i + j], pattern[j])) {
        match = false;
        break;
      }
    }
    if (match) {
      matches.push_back(i);
    }
  }

  return matches;
}

void apply_pattern_patch(
  std::uint8_t* data, std::size_t data_size, const std::vector<pattern_byte_t>& replace_pattern
) {
  if (replace_pattern.size() > data_size) {
    throw std::out_of_range("Patch location out of range");
  }

  for (std::size_t i = 0; i < replace_pattern.size(); ++i) {
    std::uint8_t new_byte = 0;
    // Build byte from two nibbles: j=0 is high nibble, j=1 is low nibble
    for (int j = 0; j < 2; ++j) {
      if (!replace_pattern[i].nibble[j].wildcard) {
        // Use replacement nibble: shift to correct position
        new_byte |= (replace_pattern[i].nibble[j].data << ((1 - j) * 4));
      } else {
        // Preserve original nibble: mask out the correct 4 bits
        new_byte |= (data[i] & (0xF0 >> (j * 4)));
      }
    }
    data[i] = new_byte;
  }
}
