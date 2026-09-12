// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/static/bytes.hpp"
#include "perimortem/core/data.hpp"

#include "perimortem/system/uuid.h"

namespace Perimortem::System {

// A 128 bit UUID value with canonical hexadecimal serialization. The C carrier
// is its sole storage, so crossing an ABI boundary does not rebuild a separate
// pair of words. Comparisons and hashing remain independent from text format.
// Construction accepts either 32 hexadecimal digits or the 36 byte dashed
// spelling. Generation provides random version 4 identifiers and time ordered
// version 7 identifiers.
class Uuid {
  static constexpr auto ascii_to_nibble(U8 byte) -> U64 {
    switch (byte) {
    case '0' ... '9':
      return byte - '0';
    case 'a' ... 'f':
      return byte - 'a' + 10;
    case 'A' ... 'F':
      return byte - 'A' + 10;
    default:
      return 0;
    }
  }

 public:
  constexpr Uuid() = default;

  constexpr Uuid(const Uuid&) = default;

  explicit constexpr Uuid(perimortem_uuid value) : value(value) {}

  // Reads undashed hexadecimal in network order, highest word first.
  explicit constexpr Uuid(const Core::Static::Bytes<32>& source) {
    if consteval {
      for (Count i = 0; i < 16; i++) {
        value.high |= ascii_to_nibble(source[i]) << (60 - i * 4);
      }

      for (Count i = 0; i < 16; i++) {
        value.low |= ascii_to_nibble(source[i + 16]) << (60 - i * 4);
      }
    } else {
      deserialize(source);
    }
  }

  // Stores the high word followed by the low word.
  explicit constexpr Uuid(U64 high, U64 low) : value{high, low} {}

  explicit constexpr Uuid(const Core::Static::Bytes<36>& source) {
    if consteval {
      for (Count i = 0; i < 8; i++) {
        value.high |= ascii_to_nibble(source[i]) << (60 - i * 4);
      }

      for (Count i = 0; i < 4; i++) {
        value.high |= ascii_to_nibble(source[i + 9]) << (28 - i * 4);
      }

      for (Count i = 0; i < 4; i++) {
        value.high |= ascii_to_nibble(source[i + 14]) << (12 - i * 4);
      }

      for (Count i = 0; i < 4; i++) {
        value.low |= ascii_to_nibble(source[i + 19]) << (60 - i * 4);
      }

      for (Count i = 0; i < 12; i++) {
        value.low |= ascii_to_nibble(source[i + 24]) << (44 - i * 4);
      }
    } else {
      deserialize(source);
    }
  }

  constexpr operator perimortem_uuid() const { return value; }

  constexpr auto operator==(const Uuid& rhs) const -> Bool {
    return value.high == rhs.value.high && value.low == rhs.value.low;
  }

  constexpr auto operator!=(const Uuid& rhs) const -> Bool {
    return value.high != rhs.value.high || value.low != rhs.value.low;
  }

  constexpr auto operator<(const Uuid& rhs) const -> Bool {
    if (value.high == rhs.value.high) {
      return value.low < rhs.value.low;
    }

    return value.high < rhs.value.high;
  }

  constexpr auto get_value() const -> perimortem_uuid { return value; }

  constexpr auto is_set() const -> Bool {
    return value.high != 0 || value.low != 0;
  }

  auto deserialize(const Core::Static::Bytes<36>& uuid_string) -> Uuid&;
  auto deserialize(const Core::Static::Bytes<32>& nibble_string) -> Uuid&;
  auto serialize() const -> const Core::Static::Bytes<36>;

  static auto generate_v4() -> Uuid;

  // The timestamp orders identifiers from different Unix milliseconds while
  // the suffix distinguishes independent generators. Random suffixes do not
  // order calls within one millisecond, and a clock adjustment may move the
  // next timestamp backward. Callers needing a strict sequence must establish
  // that order separately from UUID generation.
  static auto generate_v7() -> Uuid;

 private:
  perimortem_uuid value = {};
};

static_assert(sizeof(Uuid) == sizeof(perimortem_uuid));
static_assert(alignof(Uuid) == alignof(perimortem_uuid));
static_assert(__is_standard_layout(Uuid));
static_assert(__is_trivially_copyable(Uuid));

}  // namespace Perimortem::System
