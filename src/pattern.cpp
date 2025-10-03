#include "pattern.hpp"

#include <stdexcept>

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

std::vector<std::size_t> find_pattern(
  const std::uint8_t* data, std::size_t data_size, const std::vector<pattern_byte_t>& pattern
) {
  std::vector<std::size_t> matches;

  if (pattern.empty() || data_size < pattern.size()) {
    return matches;
  }

  for (std::size_t i = 0; i <= data_size - pattern.size(); ++i) {
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
    for (int j = 0; j < 2; ++j) {
      if (!replace_pattern[i].nibble[j].wildcard) {
        new_byte |= (replace_pattern[i].nibble[j].data << ((1 - j) * 4));
      } else {
        new_byte |= (data[i] & (0xF0 >> (j * 4)));
      }
    }
    data[i] = new_byte;
  }
}
