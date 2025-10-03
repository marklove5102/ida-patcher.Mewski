#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <stdexcept>

// Include the pattern header
#include "pattern.hpp"

class PatternTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test data
        test_data = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10,
                     0x48, 0x89, 0x74, 0x24, 0x18, 0x57, 0x48, 0x83, 0xEC, 0x20,
                     0x41, 0x8B, 0xF8, 0x48, 0x8B, 0xDA, 0x48, 0x8B, 0xF1, 0xE8};
    }

    std::vector<std::uint8_t> test_data;
};

// Test parse_pattern function
TEST_F(PatternTest, ParsePatternBasic) {
    // Test basic hex pattern
    std::string pattern = "48 89 5C";
    auto result = parse_pattern(pattern);
    
    ASSERT_EQ(result.size(), 3);
    
    // Check first byte (0x48)
    EXPECT_EQ(result[0].nibble[0].data, 4);
    EXPECT_FALSE(result[0].nibble[0].wildcard);
    EXPECT_EQ(result[0].nibble[1].data, 8);
    EXPECT_FALSE(result[0].nibble[1].wildcard);
    
    // Check second byte (0x89)
    EXPECT_EQ(result[1].nibble[0].data, 8);
    EXPECT_FALSE(result[1].nibble[0].wildcard);
    EXPECT_EQ(result[1].nibble[1].data, 9);
    EXPECT_FALSE(result[1].nibble[1].wildcard);
    
    // Check third byte (0x5C)
    EXPECT_EQ(result[2].nibble[0].data, 5);
    EXPECT_FALSE(result[2].nibble[0].wildcard);
    EXPECT_EQ(result[2].nibble[1].data, 12);
    EXPECT_FALSE(result[2].nibble[1].wildcard);
}

TEST_F(PatternTest, ParsePatternWithWildcards) {
    // Test pattern with wildcards
    std::string pattern = "48 ?? 5C ?4";
    auto result = parse_pattern(pattern);
    
    ASSERT_EQ(result.size(), 4);
    
    // Check first byte (0x48) - no wildcards
    EXPECT_EQ(result[0].nibble[0].data, 4);
    EXPECT_FALSE(result[0].nibble[0].wildcard);
    EXPECT_EQ(result[0].nibble[1].data, 8);
    EXPECT_FALSE(result[0].nibble[1].wildcard);
    
    // Check second byte (??) - full wildcard
    EXPECT_TRUE(result[1].nibble[0].wildcard);
    EXPECT_TRUE(result[1].nibble[1].wildcard);
    
    // Check third byte (0x5C) - no wildcards
    EXPECT_EQ(result[2].nibble[0].data, 5);
    EXPECT_FALSE(result[2].nibble[0].wildcard);
    EXPECT_EQ(result[2].nibble[1].data, 12);
    EXPECT_FALSE(result[2].nibble[1].wildcard);
    
    // Check fourth byte (?4) - partial wildcard
    EXPECT_TRUE(result[3].nibble[0].wildcard);
    EXPECT_EQ(result[3].nibble[1].data, 4);
    EXPECT_FALSE(result[3].nibble[1].wildcard);
}

TEST_F(PatternTest, ParsePatternLowercase) {
    // Test lowercase hex
    std::string pattern = "48 89 ab cd ef";
    auto result = parse_pattern(pattern);
    
    ASSERT_EQ(result.size(), 5);
    
    // Check 0xab
    EXPECT_EQ(result[2].nibble[0].data, 10);
    EXPECT_EQ(result[2].nibble[1].data, 11);
    
    // Check 0xcd
    EXPECT_EQ(result[3].nibble[0].data, 12);
    EXPECT_EQ(result[3].nibble[1].data, 13);
    
    // Check 0xef
    EXPECT_EQ(result[4].nibble[0].data, 14);
    EXPECT_EQ(result[4].nibble[1].data, 15);
}

TEST_F(PatternTest, ParsePatternNoSpaces) {
    // Test pattern without spaces
    std::string pattern = "48895C";
    auto result = parse_pattern(pattern);
    
    ASSERT_EQ(result.size(), 3);
    
    // Should parse the same as "48 89 5C"
    EXPECT_EQ(result[0].nibble[0].data, 4);
    EXPECT_EQ(result[0].nibble[1].data, 8);
    EXPECT_EQ(result[1].nibble[0].data, 8);
    EXPECT_EQ(result[1].nibble[1].data, 9);
    EXPECT_EQ(result[2].nibble[0].data, 5);
    EXPECT_EQ(result[2].nibble[1].data, 12);
}

TEST_F(PatternTest, ParsePatternEmpty) {
    std::string pattern = "";
    auto result = parse_pattern(pattern);
    EXPECT_TRUE(result.empty());
}

TEST_F(PatternTest, ParsePatternOnlySpaces) {
    std::string pattern = "   \t\n  ";
    auto result = parse_pattern(pattern);
    EXPECT_TRUE(result.empty());
}

TEST_F(PatternTest, ParsePatternInvalidCharacter) {
    std::string pattern = "48 89 XY";
    EXPECT_THROW(parse_pattern(pattern), std::runtime_error);
}

TEST_F(PatternTest, ParsePatternIncompletePattern) {
    std::string pattern = "48 89 5";
    EXPECT_THROW(parse_pattern(pattern), std::runtime_error);
}

// Test match_pattern_byte function
TEST_F(PatternTest, MatchPatternByteExact) {
    pattern_byte_t pattern_byte;
    pattern_byte.nibble[0] = {4, false};  // high nibble = 4
    pattern_byte.nibble[1] = {8, false};  // low nibble = 8
    
    EXPECT_TRUE(match_pattern_byte(0x48, pattern_byte));
    EXPECT_FALSE(match_pattern_byte(0x47, pattern_byte));
    EXPECT_FALSE(match_pattern_byte(0x58, pattern_byte));
}

TEST_F(PatternTest, MatchPatternByteFullWildcard) {
    pattern_byte_t pattern_byte;
    pattern_byte.nibble[0] = {0, true};   // high nibble wildcard
    pattern_byte.nibble[1] = {0, true};   // low nibble wildcard
    
    EXPECT_TRUE(match_pattern_byte(0x48, pattern_byte));
    EXPECT_TRUE(match_pattern_byte(0xFF, pattern_byte));
    EXPECT_TRUE(match_pattern_byte(0x00, pattern_byte));
}

TEST_F(PatternTest, MatchPatternBytePartialWildcard) {
    pattern_byte_t pattern_byte;
    pattern_byte.nibble[0] = {0, true};   // high nibble wildcard
    pattern_byte.nibble[1] = {8, false};  // low nibble = 8
    
    EXPECT_TRUE(match_pattern_byte(0x48, pattern_byte));
    EXPECT_TRUE(match_pattern_byte(0x58, pattern_byte));
    EXPECT_TRUE(match_pattern_byte(0xF8, pattern_byte));
    EXPECT_FALSE(match_pattern_byte(0x47, pattern_byte));
    EXPECT_FALSE(match_pattern_byte(0x49, pattern_byte));
}

// Test find_pattern function
TEST_F(PatternTest, FindPatternSingle) {
    std::string pattern_str = "48 89 5C";
    auto pattern = parse_pattern(pattern_str);
    
    auto matches = find_pattern(test_data.data(), test_data.size(), pattern);
    
    ASSERT_EQ(matches.size(), 1);
    EXPECT_EQ(matches[0], 0);  // Should find at the beginning
}

TEST_F(PatternTest, FindPatternMultiple) {
    std::string pattern_str = "48";
    auto pattern = parse_pattern(pattern_str);
    
    auto matches = find_pattern(test_data.data(), test_data.size(), pattern);
    
    // Should find multiple occurrences of 0x48
    EXPECT_GT(matches.size(), 1);
    
    // Check that all matches are actually 0x48
    for (auto match : matches) {
        EXPECT_EQ(test_data[match], 0x48);
    }
}

TEST_F(PatternTest, FindPatternWithWildcard) {
    std::string pattern_str = "48 ?? 5C";
    auto pattern = parse_pattern(pattern_str);
    
    auto matches = find_pattern(test_data.data(), test_data.size(), pattern);
    
    // Should find pattern where middle byte can be anything
    ASSERT_GT(matches.size(), 0);
    
    // Verify the match
    for (auto match : matches) {
        EXPECT_EQ(test_data[match], 0x48);
        EXPECT_EQ(test_data[match + 2], 0x5C);
        // Middle byte can be anything
    }
}

TEST_F(PatternTest, FindPatternNotFound) {
    std::string pattern_str = "FF FF FF";
    auto pattern = parse_pattern(pattern_str);
    
    auto matches = find_pattern(test_data.data(), test_data.size(), pattern);
    
    EXPECT_TRUE(matches.empty());
}

TEST_F(PatternTest, FindPatternNullData) {
    std::string pattern_str = "48 89";
    auto pattern = parse_pattern(pattern_str);
    
    auto matches = find_pattern(nullptr, 100, pattern);
    
    EXPECT_TRUE(matches.empty());
}

TEST_F(PatternTest, FindPatternEmptyPattern) {
    std::vector<pattern_byte_t> empty_pattern;
    
    auto matches = find_pattern(test_data.data(), test_data.size(), empty_pattern);
    
    EXPECT_TRUE(matches.empty());
}

TEST_F(PatternTest, FindPatternTooLarge) {
    std::string pattern_str = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 41 8B F8 48 8B DA 48 8B F1 E8 FF";
    auto pattern = parse_pattern(pattern_str);
    
    auto matches = find_pattern(test_data.data(), test_data.size(), pattern);
    
    EXPECT_TRUE(matches.empty());  // Pattern is larger than data
}

// Test apply_pattern_patch function
TEST_F(PatternTest, ApplyPatternPatchBasic) {
    std::vector<std::uint8_t> data = {0x48, 0x89, 0x5C, 0x24};
    std::string patch_str = "90 90 90 90";  // NOP instructions
    auto patch = parse_pattern(patch_str);
    
    apply_pattern_patch(data.data(), data.size(), patch);
    
    EXPECT_EQ(data[0], 0x90);
    EXPECT_EQ(data[1], 0x90);
    EXPECT_EQ(data[2], 0x90);
    EXPECT_EQ(data[3], 0x90);
}

TEST_F(PatternTest, ApplyPatternPatchWithWildcards) {
    std::vector<std::uint8_t> data = {0x48, 0x89, 0x5C, 0x24};
    std::string patch_str = "90 ?? 90 ?4";  // Patch with wildcards
    auto patch = parse_pattern(patch_str);
    
    apply_pattern_patch(data.data(), data.size(), patch);
    
    EXPECT_EQ(data[0], 0x90);     // Changed
    EXPECT_EQ(data[1], 0x89);     // Preserved (wildcard)
    EXPECT_EQ(data[2], 0x90);     // Changed
    EXPECT_EQ(data[3], 0x24);     // Preserved high nibble, changed low nibble to 4
}

TEST_F(PatternTest, ApplyPatternPatchPartialWildcard) {
    std::vector<std::uint8_t> data = {0xAB};
    std::string patch_str = "?5";  // Keep high nibble, set low nibble to 5
    auto patch = parse_pattern(patch_str);
    
    apply_pattern_patch(data.data(), data.size(), patch);
    
    EXPECT_EQ(data[0], 0xA5);  // High nibble preserved (A), low nibble changed to 5
}

TEST_F(PatternTest, ApplyPatternPatchNullData) {
    std::string patch_str = "90 90";
    auto patch = parse_pattern(patch_str);
    
    EXPECT_THROW(apply_pattern_patch(nullptr, 2, patch), std::invalid_argument);
}

TEST_F(PatternTest, ApplyPatternPatchTooLarge) {
    std::vector<std::uint8_t> data = {0x48, 0x89};
    std::string patch_str = "90 90 90";  // Patch larger than data
    auto patch = parse_pattern(patch_str);
    
    EXPECT_THROW(apply_pattern_patch(data.data(), data.size(), patch), std::out_of_range);
}

TEST_F(PatternTest, ApplyPatternPatchEmpty) {
    std::vector<std::uint8_t> data = {0x48, 0x89};
    std::vector<pattern_byte_t> empty_patch;
    
    // Should not throw and should not modify data
    EXPECT_NO_THROW(apply_pattern_patch(data.data(), data.size(), empty_patch));
    EXPECT_EQ(data[0], 0x48);
    EXPECT_EQ(data[1], 0x89);
}

// Edge case tests
TEST_F(PatternTest, EdgeCaseVeryLongPattern) {
    // Test with a pattern longer than 64 bytes (SIMD limit)
    std::string long_pattern;
    for (int i = 0; i < 70; i++) {
        long_pattern += "48 ";
    }
    
    auto pattern = parse_pattern(long_pattern);
    EXPECT_EQ(pattern.size(), 70);
    
    // Create matching data
    std::vector<std::uint8_t> long_data(70, 0x48);
    
    auto matches = find_pattern(long_data.data(), long_data.size(), pattern);
    ASSERT_EQ(matches.size(), 1);
    EXPECT_EQ(matches[0], 0);
}

TEST_F(PatternTest, EdgeCaseSingleByte) {
    std::string pattern_str = "48";
    auto pattern = parse_pattern(pattern_str);
    
    std::vector<std::uint8_t> data = {0x48};
    auto matches = find_pattern(data.data(), data.size(), pattern);
    
    ASSERT_EQ(matches.size(), 1);
    EXPECT_EQ(matches[0], 0);
}

TEST_F(PatternTest, EdgeCaseAlternatingWildcards) {
    std::string pattern_str = "4? ?8 5? ?C";
    auto pattern = parse_pattern(pattern_str);
    
    std::vector<std::uint8_t> data = {0x4F, 0x18, 0x5A, 0x2C};
    auto matches = find_pattern(data.data(), data.size(), pattern);
    
    ASSERT_EQ(matches.size(), 1);
    EXPECT_EQ(matches[0], 0);
}

// Test error recovery
TEST_F(PatternTest, ErrorRecoveryInvalidNibbleData) {
    // This tests internal error handling in apply_pattern_patch
    std::vector<std::uint8_t> data = {0x48, 0x89};
    
    // Create a pattern with invalid nibble data manually
    std::vector<pattern_byte_t> invalid_pattern(1);
    invalid_pattern[0].nibble[0] = {20, false};  // Invalid nibble (> 15)
    invalid_pattern[0].nibble[1] = {8, false};
    
    // Should handle the error gracefully
    EXPECT_THROW(apply_pattern_patch(data.data(), data.size(), invalid_pattern), std::runtime_error);
}
