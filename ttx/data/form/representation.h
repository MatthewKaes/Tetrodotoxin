// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_FORM_REPRESENTATION_H
#define TTX_DATA_FORM_REPRESENTATION_H

#include "ttx/data/form/schema.h"

#ifdef __cplusplus
#include "perimortem/memory/allocator/arena.hpp"
#include "perimortem/utility/result.hpp"
#include "ttx/data/status.hpp"
#endif

// Navigation returns a primitive occurrence by value. Its byte coordinates
// belong to the containing object, while type and byte order describe the
// observation at that coordinate. No pointer to a reconstructed child object
// needs to survive the lookup.
typedef struct ttx_representation_position {
  Count offset;
  ttx_schema_value type;
  U8 byte_order;
#ifdef __cplusplus
  using Value = ttx_schema::Value;
  using ByteOrder = ttx_schema::ByteOrder;

  constexpr ttx_representation_position(
      Count offset = 0, Value type = Value::U8,
      ByteOrder byte_order = ByteOrder::Little)
      : offset(offset), type(static_cast<U8>(type)),
        byte_order(static_cast<U8>(byte_order)) {}

  constexpr auto get_value() const -> Value { return static_cast<Value>(type); }
  constexpr auto get_byte_order() const -> ByteOrder {
    return static_cast<ByteOrder>(byte_order);
  }
  constexpr auto get_extent() const -> Count;
  constexpr auto compatible(const ttx_representation_position& other) const -> Bool {
    return type == other.type && byte_order == other.byte_order;
  }
#endif
} ttx_representation_position;

// A Representation borrows a canonical descriptor buffer. Compilation has
// already established its geometry, normalization and reference invariants.
// An independent provider can publish the same format without using our
// compiler, provided it establishes those same invariants before exposing it.
//
// The owner retains the bytes for every consumer of this view. Schema and
// compiler storage can disappear independently. Relocating the bytes needs no
// reference fixups because every child reference is an absolute block index.
typedef struct ttx_representation {
  const U8* data;
  Count size;
#ifdef __cplusplus
  using Position = ttx_representation_position;
  using Value = ttx_schema::Value;
  using ByteOrder = ttx_schema::ByteOrder;

  constexpr ttx_representation(const U8* data = nullptr, Count size = 0)
      : data(data), size(size) {}
  constexpr auto get_bytes() const -> Perimortem::Core::View::Bytes {
    return Perimortem::Core::View::Bytes(data, size);
  }
  constexpr auto get_depth() const -> U8 { return data[0] & 15; }
  constexpr auto get_extent() const -> Count;
  constexpr auto get_alignment() const -> Count;
  constexpr auto get_abi() const -> const ttx_representation& { return *this; }
  constexpr auto compatible(const ttx_representation& other) const -> Bool;

  static auto compile(
      const ttx_schema& schema, Perimortem::Memory::Allocator::Arena& arena)
      -> Perimortem::Utility::Result<const ttx_representation&, Ttx::Data::Status>;

  // A byte coordinate selects the first primitive whose start is at or after
  // it. Padding is skipped, and a coordinate inside a primitive advances to
  // the following primitive rather than returning a partial value. Bounds
  // means there is no such start. Use visit when consuming a whole record so
  // each successive observation does not repeat this search from the root.
  auto next(Count offset) const
      -> Perimortem::Utility::Result<Position, Ttx::Data::Status>;

  // Whole operations consume primitive occurrences in byte order. Repeated
  // composites revisit their shared body for each instance, but successive
  // observations never restart the search at the root. Returning a failure
  // stops the walk immediately. Only the active composite path occupies stack
  // space, even when the descriptor represents a very large repeated object.
  template <typename Consumer>
  constexpr auto visit(Consumer consumer) const -> Ttx::Data::Status;

  // Prepared selections can ask for a sorted set of exact byte coordinates.
  // This walk skips the unrequested instances of a range arithmetically, so
  // selecting its final element does not observe all preceding elements.
  // Coordinates must be unique and increasing. A coordinate in padding or
  // inside a primitive returns Bounds rather than rounding to another value.
  template <typename Consumer>
  constexpr auto visit(
      Perimortem::Core::View::Vector<Count> coordinates,
      Consumer consumer) const -> Ttx::Data::Status;
#endif
} ttx_representation;

// Runtime compilation uses temporary owner storage and asks for one final
// allocation after preparation succeeds. The supplied owner retains that
// allocation, including the view and its adjacent encoded bytes.
typedef struct ttx_representation_allocator {
  void* source;
  void* (*allocate)(void* source, Count bytes, Count alignment);
} ttx_representation_allocator;

PERIMORTEM_C ttx_data_status ttx_representation_compile(
    const ttx_schema* schema, ttx_representation_allocator allocator,
    const ttx_representation** result);
PERIMORTEM_C U8 ttx_representation_compatible(
    const ttx_representation* source, const ttx_representation* destination);
PERIMORTEM_C ttx_data_status ttx_representation_next(
    const ttx_representation* source, Count offset, ttx_representation_position* result);

// C operations need the same streaming walk as native consumers. The callback
// receives each primitive's physical coordinate and type while the walker
// retains its current path on the stack. Returning a failure stops before the
// next occurrence. No callback state or position pointer survives the call, so
// consuming a record needs neither an allocated iterator nor repeated searches.
typedef struct ttx_representation_visitor {
  void* source;
  ttx_data_status (*visit)(void* source, ttx_representation_position position);
} ttx_representation_visitor;

PERIMORTEM_C ttx_data_status ttx_representation_visit(
    const ttx_representation* source, ttx_representation_visitor visitor);

// The coordinates are a borrowed, strictly increasing selection of primitive
// starts. Unselected repetitions consume no callbacks or expanded inventory.
PERIMORTEM_C ttx_data_status ttx_representation_visit_selected(
    const ttx_representation* source, const Count* coordinates, Count count,
    ttx_representation_visitor visitor);

#endif
