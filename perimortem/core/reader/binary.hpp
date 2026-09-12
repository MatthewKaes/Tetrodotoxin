// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/bytes.hpp"
#include "perimortem/core/data.hpp"

namespace Perimortem::Core::Reader {

// Reads typed values from a dense byte buffer.
//
// Each read consumes exactly the bytes for the requested type from the current
// cursor. The reader never inserts alignment padding, so it can decode packed
// protocol data and subviews that start at arbitrary byte offsets.
//
// On overflow or any failed read the reader enters an invalid state and all
// subsequent reads return zero initialized values without advancing the cursor.
template <Data::ByteOrder stream_endian>
class Binary {
 public:
  constexpr Binary(View::Bytes source) : source(source) {}
  constexpr Binary(const Binary& rhs)
      : source(rhs.source), cursor(rhs.cursor) {}

  // Sets the location of the read cursor.
  //
  // An out of range location invalidates the reader by setting the position to
  // the maximum Count value. Converting negative one to Count offers callers a
  // convenient spelling for that invalid state.
  constexpr auto set_location(Count location) -> void { cursor = location; }
  constexpr auto get_location() const -> Count { return cursor; }

  auto read_u8() -> U8;
  auto read_u16() -> U16;
  auto read_u32() -> U32;
  auto read_u64() -> U64;
  auto read_s8() -> S8;
  auto read_s16() -> S16;
  auto read_s32() -> S32;
  auto read_s64() -> S64;
  auto read_r32() -> R32;
  auto read_r64() -> R64;
  auto read_bytes(Count count) -> View::Bytes;

  constexpr auto get_size() const -> Count { return source.get_size(); }
  constexpr auto has_content() const -> Bool {
    return cursor < source.get_size();
  }

  constexpr auto reset() -> void { cursor = 0; }

 private:
  View::Bytes source;
  Count cursor = 0;
};

}  // namespace Perimortem::Core::Reader
