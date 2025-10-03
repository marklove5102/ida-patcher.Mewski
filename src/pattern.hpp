#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct pattern_byte_t {
  struct pattern_nibble_t {
    std::uint8_t data;
    bool wildcard;
  } nibble[2];
};

std::vector<pattern_byte_t> parse_pattern(const std::string& pattern);
bool match_pattern_byte(std::uint8_t byte, const pattern_byte_t& pattern_byte);

std::vector<std::size_t> find_pattern(
  const std::uint8_t* data, std::size_t data_size, const std::vector<pattern_byte_t>& pattern
);

void apply_pattern_patch(
  std::uint8_t* data, std::size_t data_size, const std::vector<pattern_byte_t>& replace_pattern
);
