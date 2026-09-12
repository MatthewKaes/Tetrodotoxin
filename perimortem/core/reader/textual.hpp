// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/bytes.hpp"

namespace Perimortem::Core::Reader {

// Reads human readable values from a text byte buffer. Reads are greedy so
// numeric values must be whitespace separated to be read appropriately.
//
// Real, Flag, Unsigned, and Signed reads automatically skip leading whitespace.
//
// An overflow or parse failure sets the reader to an invalid state and
// subsequent reads return zero initialized values without advancing the
// cursor.
class Textual {
 public:
  constexpr Textual(View::Bytes source) : source(source) {}
  constexpr Textual(const Textual& rhs)
      : source(rhs.source), cursor(rhs.cursor) {}

  // Sets the location of the read cursor.
  //
  // An out of range location invalidates the reader by setting the position to
  // the maximum Count value. Converting negative one to Count offers callers a
  // convenient spelling for that invalid state.
  constexpr auto set_location(Count location) -> void {
    cursor = location <= source.get_size() ? location : Count(-1);
  }
  constexpr auto get_location() const -> Count { return cursor; }

  auto read_byte() -> U8;
  auto read_flag() -> Bool;
  // Reads one unsigned value using a radix from 2 through 16.
  auto read_unsigned(U8 radix = 10) -> U64;
  auto read_signed() -> S64;
  auto read_r32() -> R32;
  auto read_r64() -> R64;

  constexpr auto get_size() const -> Count { return source.get_size(); }
  constexpr auto is_valid() const -> Bool { return cursor != Count(-1); }
  constexpr auto has_content() const -> Bool {
    return cursor < source.get_size();
  }

  constexpr auto reset() -> void { cursor = 0; }

 private:
  View::Bytes source;
  Count cursor = 0;
};

}  // namespace Perimortem::Core::Reader
