// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/static/bytes.hpp"
#include "perimortem/core/data.hpp"

#include "ttx/data/protocol/fragment.hpp"
#include "ttx/data/form/storage.hpp"

namespace Ttx::Semantic::Operations {

// Copy and Swizzle choose different coordinates but need the same typed
// observation at each one. Fragment joins that admitted representation leaf to
// its C getter and realizes the returned value in the destination. The
// operation keeps its own cursor on the stack, so sharing these mechanics does
// not turn either operation's policy into part of the Data protocol.
class Fragment {
 public:
  // The consumer visits a typed result within this call. It is not a provider
  // completion and neither it nor its state is retained across the C boundary.
  template <typename Consumer>
  static auto read(
      Data::Protocol::Fragment::Access access,
      const Data::Form::Representation& type,
      Count position,
      Consumer consume) -> Data::Status {
    if (type.kind == TTX_SCHEMA_MAPPING) {
      return access.get_function(position).visit(
          [&](auto value) {
            consume(value);
            return Data::Status::Success;
          },
          [](Data::Status status) { return status; });
    }

    switch (type.data.value.type) {
#define READ(code, name)                      \
  case code:                                  \
    return access.get_##name(position).visit( \
        [&](auto value) {                     \
          consume(value);                     \
          return Data::Status::Success;       \
        },                                    \
        [](Data::Status status) { return status; });
      READ(TTX_SCHEMA_U8, u8)
      READ(TTX_SCHEMA_U16, u16)
      READ(TTX_SCHEMA_U32, u32)
      READ(TTX_SCHEMA_U64, u64)
      READ(TTX_SCHEMA_S8, s8)
      READ(TTX_SCHEMA_S16, s16)
      READ(TTX_SCHEMA_S32, s32)
      READ(TTX_SCHEMA_S64, s64)
      READ(TTX_SCHEMA_R32, r32)
      READ(TTX_SCHEMA_R64, r64)
      READ(TTX_SCHEMA_POINTER, pointer)
#undef READ
    default:
      // TODO: Decide whether this template calls an out of line fatal log.
      // Keeping Log in a .cpp avoids its header dependencies, but needs a
      // private diagnostic entry. Measure the valid dispatch path before
      // choosing that exception over the admitted representation precondition.
      __builtin_unreachable();
    }
  }

  // A getter returns a native value, while the target may use another byte
  // order. Realizing the value here leaves that storage choice with the output
  // operation. Copying its representation as bytes preserves floating point
  // and callable bits instead of accidentally applying a numeric conversion.
  template <typename T>
  static auto put(
      Data::Form::Storage target,
      const Data::Form::Representation::Position& position,
      T value) -> void {
    using Perimortem::Core::Data::ByteOrder;
    auto* output = target.get_bytes().get_data() + position.offset;
    const Bool reverse =
        position.representation->kind == TTX_SCHEMA_VALUE &&
        position.representation->data.value.byte_order !=
            (ByteOrder::Native == ByteOrder::Little ? TTX_SCHEMA_LITTLE_ENDIAN
                                                    : TTX_SCHEMA_BIG_ENDIAN);
    if (!reverse) {
      Perimortem::Core::Data::copy(output, &value, 1);
      return;
    }

    Perimortem::Core::Static::Bytes<sizeof(T)> bytes;
    Perimortem::Core::Data::copy(bytes.get_data(), &value, 1);
    for (Count i = 0; i < sizeof(T); ++i) {
      output[i] = bytes[sizeof(T) - i - 1];
    }
  }
};

}  // namespace Ttx::Semantic::Operations
