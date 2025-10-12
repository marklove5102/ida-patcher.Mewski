#include "pattern.hpp"

#include <cstring>
#include <stdexcept>

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
 * @brief Searches for all occurrences of a pattern in a data buffer
 *
 * Uses scalar byte-by-byte matching with early exit on mismatch for optimal
 * cross-platform compatibility.
 *
 * @param data Pointer to the data buffer to search
 * @param data_size Size of the data buffer in bytes
 * @param pattern The parsed pattern to search for
 * @return Vector of byte offsets where the pattern was found (may be empty)
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

  // Scalar byte-by-byte matching with early exit on mismatch
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
