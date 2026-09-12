// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/bytes.hpp"

namespace Perimortem::Core::Reader {

// Reads self describing values from a serial bytes following the Perimortem
// Serial format.
//
// Values are encoded as 1 type byte along with there minimum byte form and are
// always returned as 64 bit values regardless of how they were encoded.
//
// Blobs are encoded as a Value + a string of raw bytes. The Value's type byte
// contains an additional flag encoding it's a blob.
//
// Negative Values are encoded as their two's complement for easier compression
// with their type byte marked with the negate flag.
//
// On any error the reader enters an invalid state and all subsequent reads
// return zero initialized values without advancing the cursor.
class Serial {
 public:
  // Stores a single read from a Serial stream.
  // Useful for schemaless data where the next value could be any type.
  class Value {
   public:
    constexpr Value() : type_info(0), value(0) {}
    constexpr Value(S64 value) : type_info(0x8000000000000000), value(value) {}
    constexpr Value(View::Bytes view)
        : type_info(view.get_size()), blob(view.get_data()) {}

    constexpr operator View::Bytes() { return get_view(); }
    constexpr operator S64() { return get_value(); }

    constexpr auto get_view() -> View::Bytes {
      if (type_info & 0x8000000000000000) {
        return View::Bytes();
      }

      return View::Bytes(blob, type_info);
    }

    constexpr auto get_value() -> S64 {
      if (type_info & 0x8000000000000000) {
        return value;
      }

      return S64();
    }

    constexpr auto is_blob() -> Bool {
      return !(type_info & 0x8000000000000000);
    }

   private:
    // Value keeps its blob/value discriminator in type_info so the complete
    // schemaless read remains 16 bytes. Static::Union would require a separate
    // tag and grow this hot return type to 24 bytes.
    U64 type_info;
    union {
      const U8* blob;
      S64 value;
    };
  };

  constexpr Serial(View::Bytes source) : source(source) {}
  constexpr Serial(const Serial& rhs)
      : source(rhs.source), cursor(rhs.cursor) {}

  // Sets the location of the read cursor.
  //
  // An out of range location invalidates the reader by setting the position to
  // the maximum Count value. Converting negative one to Count offers callers a
  // convenient spelling for that invalid state.
  constexpr auto set_location(Count location) -> void { cursor = location; }
  constexpr auto get_location() const -> Count { return cursor; }

  // Reads the next value in the stream if available.
  auto read() -> Value;

  // Reads the next value in the stream and explicitly parses it as a signed
  // value. If the value is actually a blob then 0 is returned.
  auto read_value() -> S64;

  // Reads the next value in the stream and explicitly parses it as a signed
  // value. If the value is actually a regular value than an empty view is
  // returned.
  auto read_blob() -> View::Bytes;

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
