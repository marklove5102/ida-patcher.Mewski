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
  SECTION("replace all zeros with all ones") {
    std::uint8_t data[] = {0x00, 0x00, 0x00};
    auto pattern = parse_pattern("FF FF FF");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0xFF);
    REQUIRE(data[1] == 0xFF);
    REQUIRE(data[2] == 0xFF);
  }

  SECTION("replace all ones with all zeros") {
    std::uint8_t data[] = {0xFF, 0xFF, 0xFF};
    auto pattern = parse_pattern("00 00 00");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0x00);
    REQUIRE(data[1] == 0x00);
    REQUIRE(data[2] == 0x00);
  }

  SECTION("pattern size matches buffer size") {
    std::uint8_t data[] = {0xAA, 0xBB, 0xCC};
    auto pattern = parse_pattern("11 22 33");

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0x11);
    REQUIRE(data[1] == 0x22);
    REQUIRE(data[2] == 0x33);
  }
}

TEST_CASE("apply_pattern_patch - x86 instruction patching", "[pattern][patch][x86]") {
  SECTION("nop out instructions") {
    std::uint8_t data[] = {
      0x74,
      0x05,  // jz short +5
      0xE8,
      0x12,
      0x34,
      0x56,
      0x78  // call offset
    };
    auto pattern = parse_pattern("90 90 90 90 90 90 90");

    apply_pattern_patch(data, sizeof(data), pattern);

    for (std::size_t i = 0; i < sizeof(data); ++i) {
      REQUIRE(data[i] == 0x90);
    }
  }

  SECTION("patch conditional jump to unconditional") {
    std::uint8_t data[] = {0x74, 0x05};     // jz short +5
    auto pattern = parse_pattern("EB ??");  // jmp short (preserve offset)

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0xEB);
    REQUIRE(data[1] == 0x05);
  }

  SECTION("preserve offset with wildcard") {
    std::uint8_t data[] = {
      0xFF, 0x15, 0x12, 0x34, 0x56, 0x78  // call qword ptr [rip+offset]
    };
    auto pattern = parse_pattern("90 90 ?? ?? ?? ??");  // nop first 2 bytes, preserve offset

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0x90);
    REQUIRE(data[1] == 0x90);
    REQUIRE(data[2] == 0x12);
    REQUIRE(data[3] == 0x34);
    REQUIRE(data[4] == 0x56);
    REQUIRE(data[5] == 0x78);
  }
}

TEST_CASE("apply_pattern_patch - aarch64 instruction patching", "[pattern][patch][aarch64]") {
  SECTION("nop out instructions") {
    std::uint8_t data[] = {
      0x00,
      0x00,
      0x80,
      0xD2,  // mov x0, #0
      0x01,
      0x00,
      0x80,
      0xD2,  // mov x1, #0
      0xC0,
      0x03,
      0x5F,
      0xD6  // ret
    };
    auto pattern = parse_pattern("1F 20 03 D5 1F 20 03 D5 1F 20 03 D5");  // nop x3

    apply_pattern_patch(data, sizeof(data), pattern);

    for (std::size_t i = 0; i < sizeof(data); i += 4) {
      REQUIRE(data[i] == 0x1F);
      REQUIRE(data[i + 1] == 0x20);
      REQUIRE(data[i + 2] == 0x03);
      REQUIRE(data[i + 3] == 0xD5);
    }
  }

  SECTION("patch branch to nop preserving target") {
    std::uint8_t data[] = {
      0x03, 0x00, 0x00, 0x54  // b.eq <target>
    };
    auto pattern = parse_pattern("1F 20 03 D5");  // nop

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0x1F);
    REQUIRE(data[1] == 0x20);
    REQUIRE(data[2] == 0x03);
    REQUIRE(data[3] == 0xD5);
  }

  SECTION("preserve immediate value with wildcard") {
    std::uint8_t data[] = {
      0x00, 0x12, 0x80, 0xD2  // mov x0, #0x900
    };
    auto pattern = parse_pattern("00 ?? ?? D2");  // preserve immediate, keep instruction type

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0x00);
    REQUIRE(data[1] == 0x12);
    REQUIRE(data[2] == 0x80);
    REQUIRE(data[3] == 0xD2);
  }

  SECTION("modify register while preserving immediate") {
    std::uint8_t data[] = {
      0x00, 0x12, 0x80, 0xD2  // mov x0, #0x900
    };
    auto pattern = parse_pattern("01 ?? ?? ??");  // change x0 to x1, preserve rest

    apply_pattern_patch(data, sizeof(data), pattern);

    REQUIRE(data[0] == 0x01);
    REQUIRE(data[1] == 0x12);
    REQUIRE(data[2] == 0x80);
    REQUIRE(data[3] == 0xD2);
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
  SECTION("large pattern replacement") {
    std::vector<std::uint8_t> data(100, 0xAA);
    std::string pattern_str;
    for (int i = 0; i < 50; ++i) {
      if (i > 0) {
        pattern_str += " ";
      }
      pattern_str += "BB";
    }

    auto pattern = parse_pattern(pattern_str);
    apply_pattern_patch(data.data(), data.size(), pattern);

    for (std::size_t i = 0; i < 50; ++i) {
      REQUIRE(data[i] == 0xBB);
    }

    for (std::size_t i = 50; i < data.size(); ++i) {
      REQUIRE(data[i] == 0xAA);
    }
  }
}
