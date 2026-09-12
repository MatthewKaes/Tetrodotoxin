// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/bytes.hpp"
#include "perimortem/core/view/vector.hpp"
#include "perimortem/core/access/bytes.hpp"
#include "perimortem/core/data.hpp"

namespace Perimortem::Core::Writer {

// Writes typed values into a dense byte buffer.
//
// Each write emits exactly the bytes for the provided value at the current
// cursor. The writer never inserts alignment padding, so its output can be used
// directly for packed protocol data and worker handoff payloads.
//
// On overflow the writer enters an invalid state and subsequent writes are safe
// but treated as undefined behavior.
template <Data::ByteOrder stream_endian>
class Binary {
 public:
  constexpr Binary(Core::Access::Bytes source) : source(source) {};
  constexpr Binary(const Binary& rhs) : source(rhs.source) {};

  // Sets the location of the read/write cursor.
  // If the index is out of range then the cursor is put to the end of the
  // buffer.
  constexpr auto set_pointer(Count location) -> void {
    cursor = location < source.get_size() ? location : source.get_size();
  }

  constexpr auto operator<<(const U8 value) -> Binary& {
    return write_unsigned(value);
  }

  constexpr auto operator<<(const U16 value) -> Binary& {
    return write_unsigned(value);
  }

  constexpr auto operator<<(const U32 value) -> Binary& {
    return write_unsigned(value);
  }

  constexpr auto operator<<(const U64 value) -> Binary& {
    return write_unsigned(value);
  }

  constexpr auto operator<<(const S8 value) -> Binary& {
    return write_unsigned(U8(value));
  }

  constexpr auto operator<<(const S16 value) -> Binary& {
    return write_unsigned(U16(value));
  }

  constexpr auto operator<<(const S32 value) -> Binary& {
    return write_unsigned(U32(value));
  }

  constexpr auto operator<<(const S64 value) -> Binary& {
    return write_unsigned(U64(value));
  }

  constexpr auto operator<<(const R32 value) -> Binary& {
    return write_unsigned(__builtin_bit_cast(U32, value));
  }

  constexpr auto operator<<(const R64 value) -> Binary& {
    return write_unsigned(__builtin_bit_cast(U64, value));
  }

  constexpr auto operator<<(const View::Bytes blob) -> Binary& {
    return write_vector(blob);
  }

  constexpr auto operator<<(const View::Vector<U8> blob) -> Binary& {
    return write_vector(blob);
  }

  constexpr auto operator<<(const View::Vector<U16> blob) -> Binary& {
    return write_vector(blob);
  }

  constexpr auto operator<<(const View::Vector<U32> blob) -> Binary& {
    return write_vector(blob);
  }

  constexpr auto operator<<(const View::Vector<U64> blob) -> Binary& {
    return write_vector(blob);
  }

  constexpr auto operator<<(const View::Vector<S8> blob) -> Binary& {
    return write_vector(blob);
  }

  constexpr auto operator<<(const View::Vector<S16> blob) -> Binary& {
    return write_vector(blob);
  }

  constexpr auto operator<<(const View::Vector<S32> blob) -> Binary& {
    return write_vector(blob);
  }

  constexpr auto operator<<(const View::Vector<S64> blob) -> Binary& {
    return write_vector(blob);
  }

  constexpr auto get_size() const -> Count { return source.get_size(); }
  constexpr auto get_location() const -> Count { return cursor; }
  constexpr auto is_valid() const -> Bool { return valid_state; }
  constexpr operator View::Bytes() const {
    return View::Bytes(source.get_data(), cursor);
  }

 private:
  // Constant evaluation cannot copy an integer through a byte pointer. Build
  // its bytes explicitly there, while runtime writes retain the native copy.
  template <typename T>
  constexpr auto write_unsigned(T value) -> Binary& {
    if (sizeof(T) > source.get_size() - cursor) [[unlikely]] {
      valid_state = False;
      return *this;
    }

    if consteval {
      for (Count i = 0; i < sizeof(T); ++i) {
        const Count shift = stream_endian == Data::ByteOrder::Little
                                ? i * 8
                                : (sizeof(T) - i - 1) * 8;
        source.get_data()[cursor + i] = U8(value >> shift);
      }
    } else {
      value =
          Data::ensure_endian<Data::ByteOrder::Native, stream_endian>(value);
      Data::copy(source.get_data() + cursor, value);
    }

    cursor += sizeof(T);
    return *this;
  }

  template <typename T>
  constexpr auto write_vector(T blob) -> Binary& {
    using element_type = typename T::data_type;
    if (blob.get_size() > (source.get_size() - cursor) / sizeof(element_type))
        [[unlikely]] {
      valid_state = False;
      return *this;
    }

    // Keep one bulk copy when the values already have the stream's byte order.
    // Constant evaluation shares the scalar writes because it cannot inspect a
    // typed array through a byte pointer.
    if !consteval {
      if constexpr (
          sizeof(element_type) == 1 ||
          stream_endian == Data::ByteOrder::Native) {
        if (blob.get_size()) {
          Data::copy(
              source.get_data() + cursor, blob.get_data(), blob.get_size());
        }

        cursor += sizeof(element_type) * blob.get_size();
        return *this;
      }
    }

    for (Count index = 0; index < blob.get_size(); index++) {
      *this << blob.get_data()[index];
    }

    return *this;
  }

  Access::Bytes source;
  Count cursor = 0;
  Bool valid_state = True;
};

}  // namespace Perimortem::Core::Writer
