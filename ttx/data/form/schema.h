// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_FORM_SCHEMA_H
#define TTX_DATA_FORM_SCHEMA_H

#include "ttx/data/status.h"

// Native pointer storage occupies eight bytes on the supported platforms.
// Data describes that storage without following the pointer or confirming a
// callable signature. Those meanings are supplied through Semantic contracts.
#ifdef __cplusplus
static_assert(sizeof(void*) == 8 && alignof(void*) == 8);
static_assert(sizeof(void (*)(void)) == 8 && alignof(void (*)(void)) == 8);
#else
_Static_assert(
    sizeof(void*) == 8 && _Alignof(void*) == 8,
    "TTX requires eight byte native pointers and alignment.");
_Static_assert(
    sizeof(void (*)(void)) == 8 && _Alignof(void (*)(void)) == 8,
    "TTX requires eight byte native function pointers and alignment.");
#endif

#ifdef __cplusplus
#include "perimortem/core/view/vector.hpp"

#endif

#define TTX_SCHEMA_VALUE ((U8)1)
#define TTX_SCHEMA_COMPOSITE ((U8)2)
#define TTX_SCHEMA_RANGE ((U8)4)

typedef U8 ttx_schema_value;
#define TTX_SCHEMA_U8 ((ttx_schema_value)1)
#define TTX_SCHEMA_U16 ((ttx_schema_value)2)
#define TTX_SCHEMA_U32 ((ttx_schema_value)3)
#define TTX_SCHEMA_U64 ((ttx_schema_value)4)
#define TTX_SCHEMA_S8 ((ttx_schema_value)5)
#define TTX_SCHEMA_S16 ((ttx_schema_value)6)
#define TTX_SCHEMA_S32 ((ttx_schema_value)7)
#define TTX_SCHEMA_S64 ((ttx_schema_value)8)
#define TTX_SCHEMA_R32 ((ttx_schema_value)9)
#define TTX_SCHEMA_R64 ((ttx_schema_value)10)
#define TTX_SCHEMA_POINTER ((ttx_schema_value)12)
#define TTX_SCHEMA_LITTLE_ENDIAN ((U8)0)
#define TTX_SCHEMA_BIG_ENDIAN ((U8)1)

struct ttx_schema;

// Red and green can use the same U32 schema but occupy different positions in
// a color. Their placement belongs to the parent's entries, which lets a child
// description be reused without changing its meaning. The author supplies byte
// offsets from the intended wire record. Reusing a child therefore preserves
// its description while each placement supplies its own physical coordinate.
typedef struct ttx_schema_position {
  const struct ttx_schema* schema;
  Count offset;
#ifdef __cplusplus
  constexpr ttx_schema_position(
      const ttx_schema* schema = nullptr,
      Count offset = 0)
      : schema(schema), offset(offset) {}

  constexpr ttx_schema_position(const ttx_schema& schema, Count offset)
      : schema(&schema), offset(offset) {}

  constexpr auto get_schema() const -> const ttx_schema* { return schema; }

  constexpr auto get_offset() const -> Count { return offset; }

#endif
} ttx_schema_position;

typedef struct ttx_schema_composite {
  const ttx_schema_position* positions;
  Count count;
} ttx_schema_composite;

typedef struct ttx_schema_range {
  const struct ttx_schema* element;
  Count count;
  Count stride;
#ifdef __cplusplus
  constexpr auto get_element() const -> const ttx_schema* { return element; }

  constexpr auto get_count() const -> Count { return count; }

  constexpr auto get_stride() const -> Count { return stride; }
#endif
} ttx_schema_range;

typedef struct ttx_schema_primitive {
  U8 type;
  U8 byte_order;
} ttx_schema_primitive;

// Providers need to describe their intended wire format without deciding how
// every consumer will navigate it. Schema supplies that source vocabulary,
// including explicit offsets, strides and padding. Compiling it establishes
// the concrete geometry and lookup structure that transports borrow through a
// Representation. The source can then be discarded independently of that
// result.
//
// Composite preserves a real object boundary around its physical elements.
// Range can then be used to describe repeated geometry compactly, so compiling
// a million identical scalar positions can be processed in a single command.
//
// Names and the choice of which source supplies an output belong to the
// semantic layer. Source descriptions may be runtime objects or C++ constants.
// They need only remain stable during compilation. The resulting publication
// has its own lifetime and contains no source identity used to justify
// compatibility.
typedef struct ttx_schema {
  Count extent;
  Count alignment;
  U8 kind;
  union {
    ttx_schema_primitive value;
    ttx_schema_composite composite;
    ttx_schema_range range;
  } data;

#ifdef __cplusplus
  enum class Kind : U8 {
    Value = TTX_SCHEMA_VALUE,
    Composite = TTX_SCHEMA_COMPOSITE,
    Range = TTX_SCHEMA_RANGE,
  };

  enum class Value : U8 {
    U8 = TTX_SCHEMA_U8,
    U16 = TTX_SCHEMA_U16,
    U32 = TTX_SCHEMA_U32,
    U64 = TTX_SCHEMA_U64,
    S8 = TTX_SCHEMA_S8,
    S16 = TTX_SCHEMA_S16,
    S32 = TTX_SCHEMA_S32,
    S64 = TTX_SCHEMA_S64,
    R32 = TTX_SCHEMA_R32,
    R64 = TTX_SCHEMA_R64,
    Pointer = TTX_SCHEMA_POINTER,
  };

  enum class ByteOrder : U8 {
    Little = TTX_SCHEMA_LITTLE_ENDIAN,
    Big = TTX_SCHEMA_BIG_ENDIAN
  };

  using Position = ttx_schema_position;
  using Composite = ttx_schema_composite;
  using Range = ttx_schema_range;

  static constexpr auto get_width(Value type) -> Count;

  static constexpr auto primitive(
      Value type,
      ByteOrder order = ByteOrder::Little) -> ttx_schema;
  static constexpr auto composite(
      Perimortem::Core::View::Vector<Position> positions,
      Count extent,
      Count alignment = 1) -> ttx_schema;
  static constexpr auto range(
      const ttx_schema& element,
      Count repeats,
      Count stride,
      Count extent,
      Count alignment = 1) -> ttx_schema;

  constexpr auto get_extent() const -> Count { return extent; }

  constexpr auto get_alignment() const -> Count { return alignment; }

  constexpr auto get_kind() const -> Kind { return static_cast<Kind>(kind); }

  // Selecting the kind establishes which part of the description we can
  // observe. These accessors preserve the source facts for compilation to
  // validate, including missing children and malformed position arrays.
  constexpr auto get_value() const -> Value {
    return static_cast<Value>(data.value.type);
  }

  constexpr auto get_byte_order() const -> ByteOrder {
    return static_cast<ByteOrder>(data.value.byte_order);
  }

  constexpr auto get_positions() const
      -> Perimortem::Core::View::Vector<Position> {
    return {data.composite.positions, data.composite.count};
  }

  constexpr auto get_range() const -> const Range& { return data.range; }

  constexpr auto get_abi() const -> const ttx_schema& { return *this; }

#endif
} ttx_schema;

#endif
