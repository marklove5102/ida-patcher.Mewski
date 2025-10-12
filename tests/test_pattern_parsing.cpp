#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

#include "pattern.hpp"

/**
 * @brief Tests for parse_pattern function
 *
 * Validates pattern string parsing into structured pattern_byte_t format.
 * Tests cover: basic parsing, wildcards, whitespace handling, and error cases.
 */

TEST_CASE("parse_pattern - basic hex patterns", "[pattern][parsing]") {
  SECTION("single byte pattern") {
    auto result = parse_pattern("AB");
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].nibble[0].data == 0xA);
    REQUIRE(result[0].nibble[0].wildcard == false);
    REQUIRE(result[0].nibble[1].data == 0xB);
    REQUIRE(result[0].nibble[1].wildcard == false);
  }

  SECTION("multi-byte pattern") {
    auto result = parse_pattern("48 8B 05");
    REQUIRE(result.size() == 3);
    REQUIRE(result[0].nibble[0].data == 0x4);
    REQUIRE(result[0].nibble[1].data == 0x8);
    REQUIRE(result[1].nibble[0].data == 0x8);
    REQUIRE(result[1].nibble[1].data == 0xB);
    REQUIRE(result[2].nibble[0].data == 0x0);
    REQUIRE(result[2].nibble[1].data == 0x5);
  }

  SECTION("lowercase hex digits") {
    auto result = parse_pattern("ab cd ef");
    REQUIRE(result.size() == 3);
    REQUIRE(result[0].nibble[0].data == 0xA);
    REQUIRE(result[0].nibble[1].data == 0xB);
    REQUIRE(result[1].nibble[0].data == 0xC);
    REQUIRE(result[1].nibble[1].data == 0xD);
    REQUIRE(result[2].nibble[0].data == 0xE);
    REQUIRE(result[2].nibble[1].data == 0xF);
  }

  SECTION("mixed case hex digits") {
    auto result = parse_pattern("aB Cd");
    REQUIRE(result.size() == 2);
    REQUIRE(result[0].nibble[0].data == 0xA);
    REQUIRE(result[0].nibble[1].data == 0xB);
    REQUIRE(result[1].nibble[0].data == 0xC);
    REQUIRE(result[1].nibble[1].data == 0xD);
  }
}

TEST_CASE("parse_pattern - wildcard patterns", "[pattern][parsing][wildcard]") {
  SECTION("full byte wildcard") {
    auto result = parse_pattern("??");
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].nibble[0].wildcard == true);
    REQUIRE(result[0].nibble[1].wildcard == true);
  }

  SECTION("high nibble wildcard") {
    auto result = parse_pattern("?B");
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].nibble[0].wildcard == true);
    REQUIRE(result[0].nibble[1].wildcard == false);
    REQUIRE(result[0].nibble[1].data == 0xB);
  }

  SECTION("low nibble wildcard") {
    auto result = parse_pattern("A?");
    REQUIRE(result.size() == 1);
    REQUIRE(result[0].nibble[0].wildcard == false);
    REQUIRE(result[0].nibble[0].data == 0xA);
    REQUIRE(result[0].nibble[1].wildcard == true);
  }

  SECTION("mixed pattern with wildcards") {
    auto result = parse_pattern("48 ?? A? ?B");
    REQUIRE(result.size() == 4);

    // First byte: 48
    REQUIRE(result[0].nibble[0].data == 0x4);
    REQUIRE(result[0].nibble[1].data == 0x8);
    REQUIRE_FALSE(result[0].nibble[0].wildcard);
    REQUIRE_FALSE(result[0].nibble[1].wildcard);

    // Second byte: ??
    REQUIRE(result[1].nibble[0].wildcard == true);
    REQUIRE(result[1].nibble[1].wildcard == true);

    // Third byte: A?
    REQUIRE(result[2].nibble[0].data == 0xA);
    REQUIRE_FALSE(result[2].nibble[0].wildcard);
    REQUIRE(result[2].nibble[1].wildcard == true);

    // Fourth byte: ?B
    REQUIRE(result[3].nibble[0].wildcard == true);
    REQUIRE(result[3].nibble[1].data == 0xB);
    REQUIRE_FALSE(result[3].nibble[1].wildcard);
  }
}

TEST_CASE("parse_pattern - whitespace handling", "[pattern][parsing][whitespace]") {
  SECTION("spaces between bytes") {
    auto result = parse_pattern("AA BB CC");
    REQUIRE(result.size() == 3);
  }

  SECTION("multiple spaces") {
    auto result = parse_pattern("AA   BB");
    REQUIRE(result.size() == 2);
  }

  SECTION("tabs between bytes") {
    auto result = parse_pattern("AA\tBB");
    REQUIRE(result.size() == 2);
  }

  SECTION("newlines in pattern") {
    auto result = parse_pattern("AA\nBB");
    REQUIRE(result.size() == 2);
  }

  SECTION("no spaces") {
    auto result = parse_pattern("AABB");
    REQUIRE(result.size() == 2);
    REQUIRE(result[0].nibble[0].data == 0xA);
    REQUIRE(result[0].nibble[1].data == 0xA);
    REQUIRE(result[1].nibble[0].data == 0xB);
    REQUIRE(result[1].nibble[1].data == 0xB);
  }

  SECTION("leading and trailing whitespace") {
    auto result = parse_pattern("  AA BB  ");
    REQUIRE(result.size() == 2);
  }
}

TEST_CASE("parse_pattern - error cases", "[pattern][parsing][error]") {
  SECTION("empty string") {
    auto result = parse_pattern("");
    REQUIRE(result.empty());
  }

  SECTION("incomplete byte") {
    REQUIRE_THROWS_AS(parse_pattern("A"), std::runtime_error);
  }

  SECTION("invalid character") {
    REQUIRE_THROWS_AS(parse_pattern("GG"), std::runtime_error);
  }

  SECTION("special characters") {
    REQUIRE_THROWS_AS(parse_pattern("AA @ BB"), std::runtime_error);
  }

  SECTION("incomplete byte at end") {
    REQUIRE_THROWS_AS(parse_pattern("AA BB C"), std::runtime_error);
  }
}

TEST_CASE("parse_pattern - real-world patterns", "[pattern][parsing][real-world]") {
  SECTION("DEADBEEF pattern") {
    auto result = parse_pattern("DE AD BE EF");
    REQUIRE(result.size() == 4);
    REQUIRE(result[0].nibble[0].data == 0xD);
    REQUIRE(result[0].nibble[1].data == 0xE);
    REQUIRE(result[1].nibble[0].data == 0xA);
    REQUIRE(result[1].nibble[1].data == 0xD);
    REQUIRE(result[2].nibble[0].data == 0xB);
    REQUIRE(result[2].nibble[1].data == 0xE);
    REQUIRE(result[3].nibble[0].data == 0xE);
    REQUIRE(result[3].nibble[1].data == 0xF);
  }

  SECTION("x86 instruction pattern") {
    auto result = parse_pattern("48 8B 05 ?? ?? ?? ??");
    REQUIRE(result.size() == 7);
    // MOV prefix
    REQUIRE(result[0].nibble[0].data == 0x4);
    REQUIRE(result[0].nibble[1].data == 0x8);
    // Wildcards for offset
    for (int i = 3; i < 7; ++i) {
      REQUIRE(result[i].nibble[0].wildcard == true);
      REQUIRE(result[i].nibble[1].wildcard == true);
    }
  }

  SECTION("jump instruction with wildcard offset") {
    auto result = parse_pattern("E9 ?? ?? ?? ??");
    REQUIRE(result.size() == 5);
    REQUIRE(result[0].nibble[0].data == 0xE);
    REQUIRE(result[0].nibble[1].data == 0x9);
    for (int i = 1; i < 5; ++i) {
      REQUIRE(result[i].nibble[0].wildcard == true);
      REQUIRE(result[i].nibble[1].wildcard == true);
    }
  }
}

TEST_CASE("parse_pattern - edge cases", "[pattern][parsing][edge]") {
  SECTION("all zeros") {
    auto result = parse_pattern("00 00 00");
    REQUIRE(result.size() == 3);
    for (const auto& byte : result) {
      REQUIRE(byte.nibble[0].data == 0x0);
      REQUIRE(byte.nibble[1].data == 0x0);
    }
  }

  SECTION("all Fs") {
    auto result = parse_pattern("FF FF FF");
    REQUIRE(result.size() == 3);
    for (const auto& byte : result) {
      REQUIRE(byte.nibble[0].data == 0xF);
      REQUIRE(byte.nibble[1].data == 0xF);
    }
  }

  SECTION("alternating wildcards and values") {
    auto result = parse_pattern("?? AA ?? BB");
    REQUIRE(result.size() == 4);
    REQUIRE(result[0].nibble[0].wildcard == true);
    REQUIRE(result[0].nibble[1].wildcard == true);
    REQUIRE(result[1].nibble[0].data == 0xA);
    REQUIRE(result[1].nibble[1].data == 0xA);
    REQUIRE(result[2].nibble[0].wildcard == true);
    REQUIRE(result[2].nibble[1].wildcard == true);
    REQUIRE(result[3].nibble[0].data == 0xB);
    REQUIRE(result[3].nibble[1].data == 0xB);
  }

  SECTION("64 byte pattern (AVX-512 boundary)") {
    std::string pattern;
    for (int i = 0; i < 64; ++i) {
      if (i > 0) {
        pattern += " ";
      }
      pattern += "AA";
    }
    auto result = parse_pattern(pattern);
    REQUIRE(result.size() == 64);
  }

  SECTION("65 byte pattern (exceeds AVX-512 register)") {
    std::string pattern;
    for (int i = 0; i < 65; ++i) {
      if (i > 0) {
        pattern += " ";
      }
      pattern += "BB";
    }
    auto result = parse_pattern(pattern);
    REQUIRE(result.size() == 65);
  }
}
