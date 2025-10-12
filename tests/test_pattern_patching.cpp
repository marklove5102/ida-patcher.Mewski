#include <stdexcept>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "pattern.hpp"

/**
 * @brief Tests for apply_pattern_patch function
 *
 * Validates pattern-based patching functionality including exact replacements,
 * wildcard preservation, and error handling.
 */

TEST_CASE("apply_pattern_patch - exact replacement", "[pattern][patch]") {
  SECTION("single byte replacement") {
    std::uint8_t data[] = {0xAA, 0xBB, 0xCC};
    auto pattern = parse_pattern("DD");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0xDD);
    REQUIRE(data[1] == 0xBB);
    REQUIRE(data[2] == 0xCC);
  }

  SECTION("multi-byte replacement") {
    std::uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    auto pattern = parse_pattern("11 22 33");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0x11);
    REQUIRE(data[1] == 0x22);
    REQUIRE(data[2] == 0x33);
    REQUIRE(data[3] == 0xDD);
  }

  SECTION("full buffer replacement") {
    std::uint8_t data[] = {0xAA, 0xBB, 0xCC};
    auto pattern = parse_pattern("11 22 33");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0x11);
    REQUIRE(data[1] == 0x22);
    REQUIRE(data[2] == 0x33);
  }
}

TEST_CASE("apply_pattern_patch - wildcard preservation", "[pattern][patch][wildcard]") {
  SECTION("full byte wildcard preserves original") {
    std::uint8_t data[] = {0xAA, 0xBB, 0xCC};
    auto pattern = parse_pattern("??");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0xAA);
    REQUIRE(data[1] == 0xBB);
    REQUIRE(data[2] == 0xCC);
  }

  SECTION("high nibble wildcard preserves high nibble") {
    std::uint8_t data[] = {0xAB, 0xCD, 0xEF};
    auto pattern = parse_pattern("?5");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0xA5);
    REQUIRE(data[1] == 0xCD);
    REQUIRE(data[2] == 0xEF);
  }

  SECTION("low nibble wildcard preserves low nibble") {
    std::uint8_t data[] = {0xAB, 0xCD, 0xEF};
    auto pattern = parse_pattern("C?");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0xCB);
    REQUIRE(data[1] == 0xCD);
    REQUIRE(data[2] == 0xEF);
  }

  SECTION("mixed wildcards and values") {
    std::uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    auto pattern = parse_pattern("11 ?? 33 ??");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0x11);
    REQUIRE(data[1] == 0xBB);
    REQUIRE(data[2] == 0x33);
    REQUIRE(data[3] == 0xDD);
  }

  SECTION("alternating nibble wildcards") {
    std::uint8_t data[] = {0xAB, 0xCD, 0xEF, 0x12};
    auto pattern = parse_pattern("1? ?2 3? ?4");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0x1B);
    REQUIRE(data[1] == 0xC2);
    REQUIRE(data[2] == 0x3F);
    REQUIRE(data[3] == 0x14);
  }
}

TEST_CASE("apply_pattern_patch - error handling", "[pattern][patch][error]") {
  SECTION("null data pointer throws") {
    auto pattern = parse_pattern("AA BB");

    REQUIRE_THROWS_AS(apply_pattern_patch(nullptr, 10, pattern), std::invalid_argument);
  }

  SECTION("pattern larger than data throws") {
    std::uint8_t data[] = {0xAA, 0xBB};
    auto pattern = parse_pattern("11 22 33 44");

    REQUIRE_THROWS_AS(apply_pattern_patch(data, sizeof(data), pattern), std::out_of_range);
  }

  SECTION("empty pattern is no-op") {
    std::uint8_t data[] = {0xAA, 0xBB, 0xCC};
    std::vector<pattern_byte_t> empty_pattern;

    apply_pattern_patch(data, sizeof(data), empty_pattern);

    REQUIRE(data[0] == 0xAA);
    REQUIRE(data[1] == 0xBB);
    REQUIRE(data[2] == 0xCC);
  }

  SECTION("zero data size with non-empty pattern throws") {
    std::uint8_t data[] = {0xAA};
    auto pattern = parse_pattern("BB");

    REQUIRE_THROWS_AS(apply_pattern_patch(data, 0, pattern), std::out_of_range);
  }
}

TEST_CASE("apply_pattern_patch - edge cases", "[pattern][patch][edge]") {
  SECTION("single byte at start") {
    std::uint8_t data[] = {0xAA, 0xBB, 0xCC};
    auto pattern = parse_pattern("FF");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0xFF);
    REQUIRE(data[1] == 0xBB);
    REQUIRE(data[2] == 0xCC);
  }

  SECTION("all zeros to all Fs") {
    std::uint8_t data[] = {0x00, 0x00, 0x00};
    auto pattern = parse_pattern("FF FF FF");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0xFF);
    REQUIRE(data[1] == 0xFF);
    REQUIRE(data[2] == 0xFF);
  }

  SECTION("all Fs to all zeros") {
    std::uint8_t data[] = {0xFF, 0xFF, 0xFF};
    auto pattern = parse_pattern("00 00 00");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0x00);
    REQUIRE(data[1] == 0x00);
    REQUIRE(data[2] == 0x00);
  }

  SECTION("pattern equals data size") {
    std::uint8_t data[] = {0xAA, 0xBB, 0xCC};
    auto pattern = parse_pattern("11 22 33");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0x11);
    REQUIRE(data[1] == 0x22);
    REQUIRE(data[2] == 0x33);
  }
}

TEST_CASE("apply_pattern_patch - real-world scenarios", "[pattern][patch][real-world]") {
  SECTION("NOP out instructions") {
    std::uint8_t data[] = {
      0x74, 0x05,                    // jz short +5
      0xE8, 0x12, 0x34, 0x56, 0x78   // call offset
    };
    auto pattern = parse_pattern("90 90 90 90 90 90 90");

    apply_pattern_patch(data, sizeof(data), pattern);

    for (std::size_t i = 0; i < sizeof(data); ++i) {
      REQUIRE(data[i] == 0x90);
    }
  }

  SECTION("patch conditional jump to unconditional") {
    std::uint8_t data[] = {0x74, 0x05};  // jz short +5
    auto pattern = parse_pattern("EB ??");  // jmp short (preserve offset)

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0xEB);
    REQUIRE(data[1] == 0x05);
  }

  SECTION("patch return value") {
    std::uint8_t data[] = {
      0xB8, 0x00, 0x00, 0x00, 0x00,  // mov eax, 0
      0xC3                           // ret
    };
    auto pattern = parse_pattern("B8 01 00 00 00");  // mov eax, 1

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0xB8);
    REQUIRE(data[1] == 0x01);
    REQUIRE(data[2] == 0x00);
    REQUIRE(data[3] == 0x00);
    REQUIRE(data[4] == 0x00);
    REQUIRE(data[5] == 0xC3);
  }

  SECTION("preserve call target while changing instruction") {
    std::uint8_t data[] = {
      0xFF, 0x15, 0x12, 0x34, 0x56, 0x78  // call qword ptr [rip+offset]
    };
    auto pattern = parse_pattern("90 90 ?? ?? ?? ??");  // NOP first 2 bytes, preserve offset

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0x90);
    REQUIRE(data[1] == 0x90);
    REQUIRE(data[2] == 0x12);
    REQUIRE(data[3] == 0x34);
    REQUIRE(data[4] == 0x56);
    REQUIRE(data[5] == 0x78);
  }
}

TEST_CASE("apply_pattern_patch - DEADBEEF example", "[pattern][patch][example]") {
  SECTION("DEADBEEF to DEADCODE") {
    std::uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    auto pattern = parse_pattern("DE AD C0 DE");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0xDE);
    REQUIRE(data[1] == 0xAD);
    REQUIRE(data[2] == 0xC0);
    REQUIRE(data[3] == 0xDE);
  }

  SECTION("DEADBEEF with wildcard preservation") {
    std::uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x12, 0x34};
    auto pattern = parse_pattern("DE AD ?? ?? C0 DE");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0xDE);
    REQUIRE(data[1] == 0xAD);
    REQUIRE(data[2] == 0xBE);
    REQUIRE(data[3] == 0xEF);
    REQUIRE(data[4] == 0xC0);
    REQUIRE(data[5] == 0xDE);
  }
}

TEST_CASE("apply_pattern_patch - nibble-level operations", "[pattern][patch][nibble]") {
  SECTION("modify only high nibbles") {
    std::uint8_t data[] = {0xAB, 0xCD, 0xEF};
    auto pattern = parse_pattern("1? 2? 3?");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0x1B);
    REQUIRE(data[1] == 0x2D);
    REQUIRE(data[2] == 0x3F);
  }

  SECTION("modify only low nibbles") {
    std::uint8_t data[] = {0xAB, 0xCD, 0xEF};
    auto pattern = parse_pattern("?1 ?2 ?3");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0xA1);
    REQUIRE(data[1] == 0xC2);
    REQUIRE(data[2] == 0xE3);
  }

  SECTION("swap nibbles conceptually") {
    std::uint8_t data[] = {0xAB};

    // First extract low nibble to temp, then write high nibble to low position
    // This test just verifies nibble-level control
    auto pattern = parse_pattern("BA");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0xBA);
  }

  SECTION("complex nibble pattern") {
    std::uint8_t data[] = {0x12, 0x34, 0x56, 0x78};
    auto pattern = parse_pattern("?0 A? ?F F?");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0x10);
    REQUIRE(data[1] == 0xA4);
    REQUIRE(data[2] == 0x5F);
    REQUIRE(data[3] == 0xF8);
  }
}

TEST_CASE("apply_pattern_patch - large buffers", "[pattern][patch][large]") {
  SECTION("64 byte pattern (AVX-512 boundary)") {
    std::vector<std::uint8_t> data(64, 0xAA);
    std::string pattern_str;
    for (int i = 0; i < 64; ++i) {
      if (i > 0) {
        pattern_str += " ";
      }
      pattern_str += "BB";
    }

    auto pattern = parse_pattern(pattern_str);

    apply_pattern_patch(data.data(), data.size(), pattern);

    for (std::size_t i = 0; i < data.size(); ++i) {
      REQUIRE(data[i] == 0xBB);
    }
  }

  SECTION("partial patch of large buffer") {
    std::vector<std::uint8_t> data(1024, 0x00);
    auto pattern = parse_pattern("AA BB CC DD");

    apply_pattern_patch(data.data(), data.size(), pattern);

    REQUIRE(data[0] == 0xAA);
    REQUIRE(data[1] == 0xBB);
    REQUIRE(data[2] == 0xCC);
    REQUIRE(data[3] == 0xDD);

    for (std::size_t i = 4; i < data.size(); ++i) {
      REQUIRE(data[i] == 0x00);
    }
  }
}
