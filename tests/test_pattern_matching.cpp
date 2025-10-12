#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

#include <vector>
#include <cstring>
#include <algorithm>

#include "pattern.hpp"

/**
 * @brief Tests for pattern matching functions
 *
 * Validates both AVX-512 SIMD and scalar pattern matching implementations.
 * Tests cover: byte matching, pattern finding, edge cases, and performance scenarios.
 */

TEST_CASE("match_pattern_byte - exact matches", "[pattern][matching][byte]") {
  SECTION("exact byte match") {
    pattern_byte_t pattern = {};
    pattern.nibble[0].data = 0xA;
    pattern.nibble[0].wildcard = false;
    pattern.nibble[1].data = 0xB;
    pattern.nibble[1].wildcard = false;

    REQUIRE(match_pattern_byte(0xAB, pattern) == true);
    REQUIRE(match_pattern_byte(0xAC, pattern) == false);
    REQUIRE(match_pattern_byte(0xBB, pattern) == false);
  }

  SECTION("all zeros") {
    pattern_byte_t pattern = {};
    pattern.nibble[0].data = 0x0;
    pattern.nibble[0].wildcard = false;
    pattern.nibble[1].data = 0x0;
    pattern.nibble[1].wildcard = false;

    REQUIRE(match_pattern_byte(0x00, pattern) == true);
    REQUIRE(match_pattern_byte(0x01, pattern) == false);
  }

  SECTION("all Fs") {
    pattern_byte_t pattern = {};
    pattern.nibble[0].data = 0xF;
    pattern.nibble[0].wildcard = false;
    pattern.nibble[1].data = 0xF;
    pattern.nibble[1].wildcard = false;

    REQUIRE(match_pattern_byte(0xFF, pattern) == true);
    REQUIRE(match_pattern_byte(0xFE, pattern) == false);
  }
}

TEST_CASE("match_pattern_byte - wildcard matching", "[pattern][matching][byte][wildcard]") {
  SECTION("full byte wildcard") {
    pattern_byte_t pattern = {};
    pattern.nibble[0].wildcard = true;
    pattern.nibble[1].wildcard = true;

    REQUIRE(match_pattern_byte(0x00, pattern) == true);
    REQUIRE(match_pattern_byte(0xAB, pattern) == true);
    REQUIRE(match_pattern_byte(0xFF, pattern) == true);
  }

  SECTION("high nibble wildcard") {
    pattern_byte_t pattern = {};
    pattern.nibble[0].wildcard = true;
    pattern.nibble[1].data = 0x5;
    pattern.nibble[1].wildcard = false;

    REQUIRE(match_pattern_byte(0x05, pattern) == true);
    REQUIRE(match_pattern_byte(0xA5, pattern) == true);
    REQUIRE(match_pattern_byte(0xF5, pattern) == true);
    REQUIRE(match_pattern_byte(0xA6, pattern) == false);
  }

  SECTION("low nibble wildcard") {
    pattern_byte_t pattern = {};
    pattern.nibble[0].data = 0xC;
    pattern.nibble[0].wildcard = false;
    pattern.nibble[1].wildcard = true;

    REQUIRE(match_pattern_byte(0xC0, pattern) == true);
    REQUIRE(match_pattern_byte(0xCA, pattern) == true);
    REQUIRE(match_pattern_byte(0xCF, pattern) == true);
    REQUIRE(match_pattern_byte(0xD0, pattern) == false);
  }
}

TEST_CASE("find_pattern - single occurrence", "[pattern][matching][find]") {
  SECTION("exact match at start") {
    std::uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00};
    auto pattern = parse_pattern("DE AD BE EF");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0] == 0);
  }

  SECTION("exact match at end") {
    std::uint8_t data[] = {0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF};
    auto pattern = parse_pattern("DE AD BE EF");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0] == 2);
  }

  SECTION("exact match in middle") {
    std::uint8_t data[] = {0x00, 0x00, 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00};
    auto pattern = parse_pattern("DE AD BE EF");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0] == 2);
  }
}

TEST_CASE("find_pattern - multiple occurrences", "[pattern][matching][find][multiple]") {
  SECTION("two consecutive matches") {
    std::uint8_t data[] = {0xAA, 0xBB, 0xAA, 0xBB, 0x00};
    auto pattern = parse_pattern("AA BB");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.size() == 2);
    REQUIRE(matches[0] == 0);
    REQUIRE(matches[1] == 2);
  }

  SECTION("overlapping pattern matches") {
    std::uint8_t data[] = {0xAA, 0xAA, 0xAA, 0x00};
    auto pattern = parse_pattern("AA AA");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.size() == 2);
    REQUIRE(matches[0] == 0);
    REQUIRE(matches[1] == 1);
  }

  SECTION("three separated matches") {
    std::uint8_t data[] = {0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00};
    auto pattern = parse_pattern("AA");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.size() == 3);
    REQUIRE(matches[0] == 0);
    REQUIRE(matches[1] == 2);
    REQUIRE(matches[2] == 4);
  }
}

TEST_CASE("find_pattern - wildcard patterns", "[pattern][matching][find][wildcard]") {
  SECTION("full byte wildcard") {
    std::uint8_t data[] = {0xAA, 0x12, 0xBB, 0xAA, 0x34, 0xBB};
    auto pattern = parse_pattern("AA ?? BB");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.size() == 2);
    REQUIRE(matches[0] == 0);
    REQUIRE(matches[1] == 3);
  }

  SECTION("high nibble wildcard") {
    std::uint8_t data[] = {0xA5, 0xB5, 0xC5, 0xD6};
    auto pattern = parse_pattern("?5");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.size() == 3);
    REQUIRE(matches[0] == 0);
    REQUIRE(matches[1] == 1);
    REQUIRE(matches[2] == 2);
  }

  SECTION("low nibble wildcard") {
    std::uint8_t data[] = {0xA0, 0xA1, 0xA2, 0xB0};
    auto pattern = parse_pattern("A?");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.size() == 3);
    REQUIRE(matches[0] == 0);
    REQUIRE(matches[1] == 1);
    REQUIRE(matches[2] == 2);
  }

  SECTION("multiple wildcards") {
    std::uint8_t data[] = {0x48, 0x8B, 0x05, 0x12, 0x34, 0x56, 0x78, 0x90};
    auto pattern = parse_pattern("48 8B 05 ?? ?? ?? ??");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0] == 0);
  }
}

TEST_CASE("find_pattern - no matches", "[pattern][matching][find][negative]") {
  SECTION("pattern not present") {
    std::uint8_t data[] = {0x00, 0x11, 0x22, 0x33};
    auto pattern = parse_pattern("AA BB");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.empty());
  }

  SECTION("partial match only") {
    std::uint8_t data[] = {0xAA, 0xBB, 0xCC};
    auto pattern = parse_pattern("AA BB DD");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.empty());
  }
}

TEST_CASE("find_pattern - edge cases", "[pattern][matching][find][edge]") {
  SECTION("empty data") {
    auto pattern = parse_pattern("AA");
    auto matches = find_pattern(nullptr, 0, pattern);

    REQUIRE(matches.empty());
  }

  SECTION("null data pointer") {
    auto pattern = parse_pattern("AA");
    auto matches = find_pattern(nullptr, 100, pattern);

    REQUIRE(matches.empty());
  }

  SECTION("pattern larger than data") {
    std::uint8_t data[] = {0xAA, 0xBB};
    auto pattern = parse_pattern("AA BB CC DD");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.empty());
  }

  SECTION("pattern equals data size") {
    std::uint8_t data[] = {0xAA, 0xBB, 0xCC};
    auto pattern = parse_pattern("AA BB CC");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0] == 0);
  }

  SECTION("single byte data and pattern") {
    std::uint8_t data[] = {0xAA};
    auto pattern = parse_pattern("AA");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0] == 0);
  }
}

TEST_CASE("find_pattern - SIMD boundary conditions", "[pattern][matching][find][simd]") {
  SECTION("exactly 64 bytes pattern (AVX-512 register size)") {
    std::vector<std::uint8_t> data(128, 0xAA);
    std::string pattern_str;
    for (int i = 0; i < 64; ++i) {
      if (i > 0) {
        pattern_str += " ";
      }
      pattern_str += "AA";
    }
    auto pattern = parse_pattern(pattern_str);
    auto matches = find_pattern(data.data(), data.size(), pattern);

    REQUIRE(matches.size() == 65);
  }

  SECTION("65 bytes pattern (exceeds AVX-512 register)") {
    std::vector<std::uint8_t> data(128, 0xBB);
    std::string pattern_str;
    for (int i = 0; i < 65; ++i) {
      if (i > 0) {
        pattern_str += " ";
      }
      pattern_str += "BB";
    }
    auto pattern = parse_pattern(pattern_str);
    auto matches = find_pattern(data.data(), data.size(), pattern);

    REQUIRE(matches.size() == 64);
  }

  SECTION("pattern at 64-byte boundary") {
    std::vector<std::uint8_t> data(128, 0x00);
    data[63] = 0xDE;
    data[64] = 0xAD;
    data[65] = 0xBE;
    data[66] = 0xEF;

    auto pattern = parse_pattern("DE AD BE EF");
    auto matches = find_pattern(data.data(), data.size(), pattern);

    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0] == 63);
  }
}

TEST_CASE("find_pattern - large data sets", "[pattern][matching][find][large]") {
  SECTION("1KB data with single match") {
    std::vector<std::uint8_t> data(1024, 0x00);
    data[512] = 0xDE;
    data[513] = 0xAD;
    data[514] = 0xBE;
    data[515] = 0xEF;

    auto pattern = parse_pattern("DE AD BE EF");
    auto matches = find_pattern(data.data(), data.size(), pattern);

    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0] == 512);
  }

  SECTION("4KB data with multiple matches") {
    std::vector<std::uint8_t> data(4096, 0x00);
    data[100] = 0xAA;
    data[1000] = 0xAA;
    data[2000] = 0xAA;
    data[3000] = 0xAA;

    auto pattern = parse_pattern("AA");
    auto matches = find_pattern(data.data(), data.size(), pattern);

    REQUIRE(matches.size() == 4);
    REQUIRE(matches[0] == 100);
    REQUIRE(matches[1] == 1000);
    REQUIRE(matches[2] == 2000);
    REQUIRE(matches[3] == 3000);
  }
}

TEST_CASE("find_pattern - SIMD vs scalar consistency", "[pattern][matching][find][simd][scalar]") {
  SECTION("SIMD-compatible pattern (full-byte wildcards)") {
    std::vector<std::uint8_t> data(256, 0x00);
    data[10] = 0x48;
    data[11] = 0x8B;
    data[12] = 0xAA;
    data[13] = 0xBB;
    data[100] = 0x48;
    data[101] = 0x8B;
    data[102] = 0xCC;
    data[103] = 0xDD;

    auto pattern = parse_pattern("48 8B ?? ??");
    auto matches = find_pattern(data.data(), data.size(), pattern);

    REQUIRE(matches.size() == 2);
    REQUIRE(matches[0] == 10);
    REQUIRE(matches[1] == 100);
  }

  SECTION("non-SIMD pattern (mixed wildcards)") {
    std::vector<std::uint8_t> data(256, 0x00);
    data[10] = 0xA5;
    data[11] = 0xBB;
    data[100] = 0xC5;
    data[101] = 0xBB;

    auto pattern = parse_pattern("?5 BB");
    auto matches = find_pattern(data.data(), data.size(), pattern);

    REQUIRE(matches.size() == 2);
    REQUIRE(matches[0] == 10);
    REQUIRE(matches[1] == 100);
  }
}

TEST_CASE("find_pattern - real-world scenarios", "[pattern][matching][find][real-world]") {
  SECTION("x86-64 function prologue") {
    std::uint8_t data[] = {
      0x48, 0x89, 0x5C, 0x24, 0x08,  // mov [rsp+8], rbx
      0x48, 0x89, 0x6C, 0x24, 0x10,  // mov [rsp+10h], rbp
      0x48, 0x89, 0x74, 0x24, 0x18,  // mov [rsp+18h], rsi
      0x57,                          // push rdi
      0x48, 0x83, 0xEC, 0x20         // sub rsp, 20h
    };

    auto pattern = parse_pattern("48 89 5C 24 08");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.size() == 1);
    REQUIRE(matches[0] == 0);
  }

  SECTION("JMP instruction with wildcard offset") {
    std::uint8_t data[] = {
      0xE9,
      0x12,
      0x34,
      0x56,
      0x78,  // jmp offset
      0x90,  // nop
      0xE9,
      0xAA,
      0xBB,
      0xCC,
      0xDD  // another jmp
    };

    auto pattern = parse_pattern("E9 ?? ?? ?? ??");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.size() == 2);
    REQUIRE(matches[0] == 0);
    REQUIRE(matches[1] == 6);
  }

  SECTION("call instruction pattern") {
    std::uint8_t data[] = {
      0xFF,
      0x15,
      0x12,
      0x34,
      0x56,
      0x78,  // call qword ptr [rip+offset]
      0x90,
      0x90,  // nops
      0xFF,
      0x15,
      0xAA,
      0xBB,
      0xCC,
      0xDD  // another call
    };

    auto pattern = parse_pattern("FF 15 ?? ?? ?? ??");
    auto matches = find_pattern(data, sizeof(data), pattern);

    REQUIRE(matches.size() == 2);
    REQUIRE(matches[0] == 0);
    REQUIRE(matches[1] == 8);
  }
}
