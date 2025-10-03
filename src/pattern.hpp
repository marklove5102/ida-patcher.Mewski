#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief Represents a single byte in a pattern with optional wildcards per nibble
 *
 * Each byte consists of two nibbles (high and low), where each nibble can either
 * contain specific data (0-15) or be a wildcard that matches any value.
 */
struct pattern_byte_t {
  struct pattern_nibble_t {
    std::uint8_t data;  ///< The nibble value (0-15)
    bool wildcard;      ///< True if this nibble is a wildcard ('?')
  } nibble[2];          ///< [0] = high nibble, [1] = low nibble
};

/**
 * @brief Parses a hexadecimal pattern string into structured pattern bytes
 *
 * Converts a string pattern like "A1 B? ?C ??" into a vector of pattern_byte_t.
 * - Hex digits (0-9, A-F, a-f) represent specific nibble values
 * - '?' represents a wildcard nibble that matches any value
 * - Whitespace is ignored
 *
 * @param pattern The pattern string to parse (e.g., "48 8B ?5 ?? C3")
 * @return Vector of pattern bytes with wildcard information
 * @throws std::runtime_error if pattern contains invalid characters or incomplete bytes
 */
std::vector<pattern_byte_t> parse_pattern(const std::string& pattern);

/**
 * @brief Checks if a single byte matches a pattern byte
 *
 * Compares both nibbles of the byte against the pattern byte, respecting wildcards.
 * A nibble matches if the pattern nibble is a wildcard or if the values are equal.
 *
 * @param byte The byte value to test
 * @param pattern_byte The pattern byte to match against
 * @return true if both nibbles match (considering wildcards), false otherwise
 */
bool match_pattern_byte(std::uint8_t byte, const pattern_byte_t& pattern_byte);

/**
 * @brief Searches for all occurrences of a pattern in a data buffer
 *
 * Uses AVX-512 SIMD acceleration when available and pattern is suitable (≤64 bytes,
 * no mixed wildcards like "?F" or "A?"). Falls back to scalar byte-by-byte matching
 * for complex patterns or when SIMD is unavailable.
 *
 * @param data Pointer to the data buffer to search
 * @param data_size Size of the data buffer in bytes
 * @param pattern The parsed pattern to search for
 * @return Vector of byte offsets where the pattern was found (may be empty)
 */
std::vector<std::size_t> find_pattern(
  const std::uint8_t* data, std::size_t data_size, const std::vector<pattern_byte_t>& pattern
);

/**
 * @brief Applies a pattern-based patch to a data buffer
 *
 * Writes bytes to the buffer according to the replace pattern:
 * - Non-wildcard nibbles are written with their specified values
 * - Wildcard nibbles ('?') preserve the original data at that position
 *
 * This allows selective patching like "A? B?" to change only specific nibbles
 * while preserving others.
 *
 * @param data Pointer to the data buffer to modify
 * @param data_size Size of the data buffer in bytes
 * @param replace_pattern The pattern specifying which bytes/nibbles to replace
 * @throws std::invalid_argument if data pointer is null
 * @throws std::out_of_range if patch size exceeds data size
 * @throws std::runtime_error if pattern contains invalid nibble data (>15)
 */
void apply_pattern_patch(
  std::uint8_t* data, std::size_t data_size, const std::vector<pattern_byte_t>& replace_pattern
);
