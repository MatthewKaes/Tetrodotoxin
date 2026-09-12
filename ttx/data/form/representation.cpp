// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/form/representation.hpp"

using namespace Ttx::Data::Form;

auto ttx_representation_compatible(
    const Representation* source,
    const Representation* destination) -> U8 {
  return source && destination && source->compatible(*destination);
}

auto ttx_representation_at(
    const Representation* source,
    Count index,
    Representation::Position* result) -> ttx_data_status {
  if (!source || !result) {
    return TTX_DATA_INVALID;
  }

  if (index >= source->count) {
    return TTX_DATA_BOUNDS;
  }

  const Count requested = index;
  Count offset = 0;
  while (source->get_kind() == Representation::Kind::Sequence) {
    const auto entries = source->get_elements();
    Count first = index;
    if (entries.get_size() != source->count) {
      first = 0;
      Count end = entries.get_size();
      while (first + 1 < end) {
        const Count middle = first + (end - first) / 2;
        if (entries[middle].index <= index) {
          first = middle;
        } else {
          end = middle;
        }
      }
    }

    const auto& entry = entries[first];
    index -= entry.index;
    const Count width = entry.representation->count;
    offset += entry.offset + (index / width) * entry.stride;
    index %= width;
    source = entry.representation;
  }

  *result = Representation::Position(source, offset, requested);
  return TTX_DATA_SUCCESS;
}

// The array is sorted by occupied byte regions. Binary search selects a region,
// then repetition arithmetic selects its instance. Recursion follows compact
// repetition patterns only, never authored Composite or Group nesting.
static auto next_value(
    const Representation& source,
    Count offset,
    Representation::Position& result) -> Bool {
  if (offset >= source.extent || !source.count) {
    return False;
  }

  if (source.get_kind() != Representation::Kind::Sequence) {
    if (offset) {
      return False;
    }

    result = Representation::Position(&source);
    return True;
  }

  const auto entries = source.get_elements();
  Count first = 0;
  Count end = entries.get_size();
  while (first < end) {
    const Count middle = first + (end - first) / 2;
    const auto& entry = entries[middle];
    const Count limit = entry.offset + (entry.count - 1) * entry.stride +
                        entry.representation->extent;
    if (limit <= offset) {
      first = middle + 1;
    } else {
      end = middle;
    }
  }

  for (; first < entries.get_size(); ++first) {
    const auto& entry = entries[first];
    const Count local = offset > entry.offset ? offset - entry.offset : 0;
    Count repetition = local / entry.stride;
    if (repetition >= entry.count) {
      continue;
    }

    if (next_value(
            *entry.representation, local - repetition * entry.stride, result)) {
      result.offset += entry.offset + repetition * entry.stride;
      result.index += entry.index + repetition * entry.representation->count;
      return True;
    }

    if (++repetition < entry.count &&
        next_value(*entry.representation, 0, result)) {
      result.offset += entry.offset + repetition * entry.stride;
      result.index += entry.index + repetition * entry.representation->count;
      return True;
    }
  }

  return False;
}

auto ttx_representation_next(
    const Representation* source,
    Count offset,
    Representation::Position* result) -> ttx_data_status {
  if (!source || !result) {
    return TTX_DATA_INVALID;
  }

  return next_value(*source, offset, *result) ? TTX_DATA_SUCCESS
                                              : TTX_DATA_BOUNDS;
}
