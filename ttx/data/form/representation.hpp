// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/math.hpp"

#include "ttx/data/form/representation.h"

namespace Ttx::Data::Form {
using Representation = ttx_representation;
}

inline auto ttx_representation::compile(
    const ttx_schema& schema,
    Perimortem::Memory::Allocator::Arena& arena) -> Perimortem::Utility::
    Result<const ttx_representation&, Ttx::Data::Status> {
  const ttx_representation* result;
  const ttx_representation_allocator allocator = {
    &arena, [](void* owner, Count bytes, Count) -> void* {
      return static_cast<Perimortem::Memory::Allocator::Arena*>(owner)
          ->allocate(bytes)
          .get_data();
    }};
  const auto status = ttx_representation_compile(&schema, allocator, &result);
  if (status != TTX_DATA_SUCCESS) {
    return static_cast<Ttx::Data::Status>(status);
  }

  return *result;
}

// Each comparison advances through the published array. Entry boundaries may
// differ, so consume the common prefix of two progressions rather than compare
// their compressed record counts. Matching repeated patterns are checked once
// and skipped in bulk, independent of the number of repetitions.
constexpr auto ttx_representation::compare_elements(
    const ttx_representation& a,
    Count a_first,
    Count a_offset,
    const ttx_representation& b,
    Count b_first,
    Count b_offset,
    Count count) -> Bool {
  if (&a == &b && a_first == b_first) {
    return a_offset == b_offset;
  }

  if (a.get_kind() != Kind::Sequence && b.get_kind() != Kind::Sequence) {
    if (a_offset != b_offset || a.get_kind() != b.get_kind()) {
      return False;
    }

    if (a.get_kind() == Kind::Value) {
      return a.data.value.type == b.data.value.type &&
             a.data.value.byte_order == b.data.value.byte_order;
    }

    if (a.get_kind() == Kind::Mapping) {
      return a.data.mapping.input->compatible(*b.data.mapping.input) &&
             a.data.mapping.output->compatible(*b.data.mapping.output);
    }

    return True;
  }

  // The common case has corresponding entries already aligned. Compare that
  // array directly, leaving prefix matching for differently segmented runs.
  if (!a_first && !b_first && count == a.count && count == b.count &&
      a.get_kind() == Kind::Sequence && b.get_kind() == Kind::Sequence &&
      a.data.sequence.size == b.data.sequence.size) {
    Bool aligned = True;
    for (Count i = 0; i < a.data.sequence.size; ++i) {
      const auto& from = a.data.sequence.elements[i];
      const auto& to = b.data.sequence.elements[i];
      if (from.count != to.count ||
          from.representation->count != to.representation->count) {
        aligned = False;
        break;
      }

      if (from.count > 1 && from.stride != to.stride) {
        return False;
      }

      if (!compare_elements(
              *from.representation, 0, a_offset + from.offset,
              *to.representation, 0, b_offset + to.offset,
              from.representation->count)) {
        return False;
      }
    }

    if (aligned) {
      return True;
    }
  }

  Count ai = 0;
  Count bi = 0;
  while (count) {
    const auto select = [](const ttx_representation& owner, Count first,
                           Count& cursor) -> Element {
      if (owner.get_kind() != Kind::Sequence) {
        return Element(&owner, 0, 0, 1, owner.extent);
      }

      const auto entries = owner.get_elements();
      while (cursor + 1 < entries.get_size() &&
             entries[cursor + 1].index <= first) {
        ++cursor;
      }

      return entries[cursor];
    };
    const auto from = select(a, a_first, ai);
    const auto to = select(b, b_first, bi);
    const auto& left = *from.representation;
    const auto& right = *to.representation;
    const Count af = a_first - from.index;
    const Count bf = b_first - to.index;
    const Count ar = af % left.count;
    const Count br = bf % right.count;
    const Count ao = a_offset + from.offset + (af / left.count) * from.stride;
    const Count bo = b_offset + to.offset + (bf / right.count) * to.stride;
    Count available = Perimortem::Core::Math::min(
        count, Perimortem::Core::Math::min(
                   from.count * left.count - af, to.count * right.count - bf));
    Count consumed;

    if (left.get_kind() != Kind::Sequence &&
        right.get_kind() != Kind::Sequence) {
      if (ao != bo || left.get_kind() != right.get_kind()) {
        return False;
      }

      if (left.get_kind() == Kind::Value) {
        if (left.data.value.type != right.data.value.type ||
            left.data.value.byte_order != right.data.value.byte_order) {
          return False;
        }
      } else if (
          !left.data.mapping.input->compatible(*right.data.mapping.input) ||
          !left.data.mapping.output->compatible(*right.data.mapping.output)) {
        return False;
      }

      if (available > 1 && from.stride != to.stride) {
        return False;
      }

      consumed = available;
    } else if (
        !ar && !br && left.count == right.count && available >= left.count &&
        from.stride == to.stride) {
      if (!compare_elements(left, 0, ao, right, 0, bo, left.count)) {
        return False;
      }

      consumed = (available / left.count) * left.count;
    } else {
      consumed = Perimortem::Core::Math::min(
          available,
          Perimortem::Core::Math::min(left.count - ar, right.count - br));
      if (!compare_elements(left, ar, ao, right, br, bo, consumed)) {
        return False;
      }
    }

    a_first += consumed;
    b_first += consumed;
    count -= consumed;
  }

  return True;
}

// Boundary comparison uses the same prefix rule in logical coordinates.
// Its records carry no primitive types, byte padding, names or scope objects.
// A repeated record can therefore compare its boundary pattern once without
// enumerating every instance in a large array.
constexpr auto ttx_representation::compare_composites(
    const ttx_representation& a,
    Count a_first,
    Count a_offset,
    const ttx_representation& b,
    Count b_first,
    Count b_offset,
    Count count, Bool a_root, Bool b_root) -> Bool {
  if (&a == &b && a_first == b_first && a_root == b_root) {
    return a_offset == b_offset;
  }

  Count ai = 0;
  Count bi = 0;
  while (count) {
    const auto select = [](const ttx_representation& owner, Count first,
                           Count& cursor, Bool root) -> Composite {
      if (root && !first) {
        return Composite(0, owner.count);
      }

      const Count shift = root ? 1 : 0;
      while (cursor + 1 < owner.composite_size &&
             owner.composites[cursor + 1].ordinal <= first - shift) {
        ++cursor;
      }

      auto result = owner.composites[cursor];
      result.ordinal += shift;
      return result;
    };
    const auto from = select(a, a_first, ai, a_root);
    const auto to = select(b, b_first, bi, b_root);
    const Bool from_root = from.first != from.end;
    const Bool to_root = to.first != to.end;
    const Count aw = from.pattern
        ? from.pattern->composite_count + (from_root ? 1 : 0) : 1;
    const Count bw = to.pattern
        ? to.pattern->composite_count + (to_root ? 1 : 0) : 1;
    const Count af = a_first - from.ordinal;
    const Count bf = b_first - to.ordinal;
    const Count ar = af % aw;
    const Count br = bf % bw;
    const Count ao = a_offset + from.first + (af / aw) * from.stride;
    const Count bo = b_offset + to.first + (bf / bw) * to.stride;
    const Count available = Perimortem::Core::Math::min(
        count,
        Perimortem::Core::Math::min(from.count * aw - af, to.count * bw - bf));
    Count consumed;
    if (!from.pattern && !to.pattern) {
      if (ao != bo || from.end - from.first != to.end - to.first) {
        return False;
      }

      if (available > 1 && from.stride != to.stride) {
        return False;
      }

      consumed = available;
    } else if (
        from.pattern && to.pattern && !ar && !br && aw == bw &&
        available >= aw && from.stride == to.stride) {
      if (!compare_composites(*from.pattern, 0, ao, *to.pattern, 0, bo, aw,
                              from_root, to_root)) {
        return False;
      }

      consumed = (available / aw) * aw;
    } else {
      // A pattern boundary may meet a plain interval. Present that interval as
      // a one entry window without manufacturing a publication or allocation.
      const auto& left = from.pattern ? *from.pattern : a;
      const auto& right = to.pattern ? *to.pattern : b;
      const Count left_first = from.pattern ? ar : a_first;
      const Count right_first = to.pattern ? br : b_first;
      consumed = Perimortem::Core::Math::min(
          available, Perimortem::Core::Math::min(aw - ar, bw - br));
      if (!compare_composites(
              left, left_first, from.pattern ? ao : a_offset, right,
              right_first, to.pattern ? bo : b_offset, consumed,
              from.pattern ? from_root : a_root, to.pattern ? to_root : b_root)) {
        return False;
      }
    }

    a_first += consumed;
    b_first += consumed;
    count -= consumed;
  }

  return True;
}

constexpr auto ttx_representation::compatible(
    const ttx_representation& other) const -> Bool {
  if (this == &other) {
    return True;
  }

  if (extent != other.extent || alignment != other.alignment ||
      count != other.count || composite_count != other.composite_count) {
    return False;
  }

  return compare_elements(*this, 0, 0, other, 0, 0, count) &&
         compare_composites(*this, 0, 0, other, 0, 0, composite_count);
}

inline auto ttx_representation::at(Count index) const
    -> Perimortem::Utility::Result<Position, Ttx::Data::Status> {
  Position result;
  const auto status = ttx_representation_at(this, index, &result);
  if (status) {
    return static_cast<Ttx::Data::Status>(status);
  }

  return result;
}

inline auto ttx_representation::next(Count offset) const
    -> Perimortem::Utility::Result<Position, Ttx::Data::Status> {
  Position result;
  const auto status = ttx_representation_next(this, offset, &result);
  if (status) {
    return static_cast<Ttx::Data::Status>(status);
  }

  return result;
}
