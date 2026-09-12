// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/bytes.hpp"
#include "perimortem/core/data.hpp"
#include "perimortem/core/option.hpp"

namespace Perimortem::Core::Reader {

// Reads typed values from a dense byte buffer.
//
// Each read consumes exactly the bytes for the requested type from the current
// cursor. The reader never inserts alignment padding, so it can decode packed
// protocol data and subviews that start at arbitrary byte offsets.
//
// A zero value or an empty byte view can be valid input, so each read returns
// Option to distinguish those values from missing input. A failed read leaves
// the cursor unchanged, allowing the caller to try a smaller read without
// losing any of the remaining bytes.
template <Data::ByteOrder stream_endian>
class Binary {
 public:
  constexpr Binary(View::Bytes source) : source(source) {}
  constexpr Binary(const Binary& rhs)
      : source(rhs.source), cursor(rhs.cursor) {}

  // Sets the location of the read cursor.
  //
  // An out of range location causes reads to return empty until reset.
  constexpr auto set_location(Count location) -> void { cursor = location; }
  constexpr auto get_location() const -> Count { return cursor; }

  constexpr auto read_u8() -> Option<U8> { return read_value<U8>(); }
  constexpr auto read_u16() -> Option<U16> { return read_value<U16>(); }
  constexpr auto read_u32() -> Option<U32> { return read_value<U32>(); }
  constexpr auto read_u64() -> Option<U64> { return read_value<U64>(); }
  constexpr auto read_s8() -> Option<S8> { return read_value<S8, U8>(); }
  constexpr auto read_s16() -> Option<S16> { return read_value<S16, U16>(); }
  constexpr auto read_s32() -> Option<S32> { return read_value<S32, U32>(); }
  constexpr auto read_s64() -> Option<S64> { return read_value<S64, U64>(); }
  constexpr auto read_r32() -> Option<R32> { return read_value<R32, U32>(); }

  constexpr auto read_r64() -> Option<R64> { return read_value<R64, U64>(); }

  constexpr auto read_bytes(Count count) -> Option<View::Bytes> {
    if (!check_buffer_overruns(count)) {
      return Option<View::Bytes>();
    }

    View::Bytes result = source.slice(cursor, count);
    cursor += count;
    return result;
  }

  constexpr auto get_size() const -> Count { return source.get_size(); }
  constexpr auto has_content() const -> Bool {
    return cursor < source.get_size();
  }

  constexpr auto reset() -> void { cursor = 0; }

 private:
  constexpr auto check_buffer_overruns(Count read_size) const -> Bool {
    return cursor <= source.get_size() &&
           read_size <= source.get_size() - cursor;
  }

  // Constant evaluation cannot load an integer through a byte pointer. Build
  // it from the stream's bytes there, while runtime reads retain memcpy for
  // packed input that may begin at an unaligned address.
  template <typename T, typename Storage = T>
  constexpr auto read_value() -> Option<T> {
    if (!check_buffer_overruns(sizeof(Storage))) {
      return Option<T>();
    }

    Storage value = 0;
    if consteval {
      for (Count i = 0; i < sizeof(Storage); ++i) {
        const Count shift = stream_endian == Data::ByteOrder::Little
                                ? i * 8
                                : (sizeof(Storage) - i - 1) * 8;
        value |= Storage(source.get_data()[cursor + i]) << shift;
      }
    } else {
      memcpy(&value, source.get_data() + cursor, sizeof(Storage));
      value =
          Data::ensure_endian<stream_endian, Data::ByteOrder::Native>(value);
    }

    cursor += sizeof(Storage);
    return __builtin_bit_cast(T, value);
  }

  View::Bytes source;
  Count cursor = 0;
};

}  // namespace Perimortem::Core::Reader
