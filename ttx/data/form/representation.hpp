// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/form/encoding.hpp"
#include "ttx/data/form/representation.h"

namespace Ttx::Data::Form {

// The native surface adds observations to the actual C carrier. Both sides
// borrow the same bytes and establish agreement through their canonical form.
using Representation = ttx_representation;

}  // namespace Ttx::Data::Form

constexpr auto ttx_representation_position::get_extent() const -> Count {
  return Ttx::Data::Form::Schema::get_width(get_value());
}

constexpr auto ttx_representation::get_extent() const -> Count {
  using Ttx::Data::Form::Encoding;
  const U8 depth = get_depth();
  const auto block = Encoding::Block::read(get_bytes(), 0, depth);
  return Encoding::header(block, depth).extent;
}

constexpr auto ttx_representation::get_alignment() const -> Count {
  using Ttx::Data::Form::Encoding;
  const U8 depth = get_depth();
  const auto block = Encoding::Block::read(get_bytes(), 0, depth);
  return Encoding::header(block, depth).alignment;
}

constexpr auto ttx_representation::compatible(
    const ttx_representation& other) const -> Bool {
  if (size != other.size) {
    return False;
  }

  if (data == other.data) {
    return True;
  }

  return Perimortem::Core::Data::compare(data, other.data, size);
}

template <typename Consumer>
constexpr auto ttx_representation::visit(Consumer consumer) const
    -> Ttx::Data::Status {
  using Ttx::Data::Status;
  using Ttx::Data::Form::Encoding;
  const U8 depth = get_depth();
  const auto walk = [&](this auto&& self, Count body, Count offset) -> Status {
    const auto header = Encoding::header(
        Encoding::Block::read(get_bytes(), body, depth), depth);
    for (Count i = 0; i < header.count; ++i) {
      const auto entry = Encoding::element(
          Encoding::Block::read(get_bytes(), body + i + 1, depth), depth);
      for (Count repetition = 0; repetition < entry.count; ++repetition) {
        const Count start = offset + entry.offset + repetition * entry.stride;
        Status status;
        if (entry.composite) {
          status = self(entry.type, start);
        } else {
          const Position position(
              start, Value(entry.type & 63),
              (entry.type & 64) ? ByteOrder::Big : ByteOrder::Little);
          status = consumer(position);
        }

        if (status != Status::Success) {
          return status;
        }
      }
    }

    return Status::Success;
  };
  return walk(0, 0);
}

template <typename Consumer>
constexpr auto ttx_representation::visit(
    Perimortem::Core::View::Vector<Count> coordinates,
    Consumer consumer) const -> Ttx::Data::Status {
  using Ttx::Data::Status;
  using Ttx::Data::Form::Encoding;
  const U8 depth = get_depth();
  Count selected = 0;

  const auto walk = [&](this auto&& self, Count body, Count offset) -> Status {
    const auto header = Encoding::header(
        Encoding::Block::read(get_bytes(), body, depth), depth);
    for (Count i = 0; i < header.count && selected < coordinates.get_size();
         ++i) {
      const auto entry = Encoding::element(
          Encoding::Block::read(get_bytes(), body + i + 1, depth), depth);
      const Count first = offset + entry.offset;
      Count width;
      if (entry.composite) {
        const auto child = Encoding::header(
            Encoding::Block::read(get_bytes(), entry.type, depth), depth);
        width = child.extent;
      } else {
        width = ttx_schema::get_width(Value(entry.type & 63));
      }

      const Count end = first + (entry.count - 1) * entry.stride + width;

      // The next requested coordinate selects an instance directly. Keeping
      // the descriptor here lets later requests in this same run reuse it.
      while (selected < coordinates.get_size() && coordinates[selected] < end) {
        const Count requested = coordinates[selected];
        if (requested < first) {
          return Status::Bounds;
        }

        const Count instance = (requested - first) / entry.stride;
        const Count start = first + instance * entry.stride;
        Status status;
        if (entry.composite) {
          if (requested >= start + width) {
            return Status::Bounds;
          }

          const Count before = selected;
          status = self(entry.type, start);
          if (selected == before && status == Status::Success) {
            return Status::Bounds;
          }
        } else {
          if (requested != start) {
            return Status::Bounds;
          }

          const Position position(
              start, Value(entry.type & 63),
              (entry.type & 64) ? ByteOrder::Big : ByteOrder::Little);
          status = consumer(position);
          ++selected;
        }

        if (status != Status::Success) {
          return status;
        }
      }
    }

    return Status::Success;
  };
  const auto status = walk(0, 0);
  if (status != Status::Success) {
    return status;
  }

  return selected == coordinates.get_size() ? Status::Success : Status::Bounds;
}
