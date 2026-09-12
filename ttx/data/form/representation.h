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

#define TTX_FORM_PRIMITIVE ((U8)0)
#define TTX_FORM_RANGE ((U8)1)
#define TTX_FORM_MAPPING ((U8)2)
#define TTX_FORM_INTERVAL ((U8)0)
#define TTX_FORM_REPEAT ((U8)1)
#define TTX_FORM_HEADER ((U8)2)

// Type is an inline description, not a separately allocated object. Primitive
// runs carry their byte stride. A Range refers to a prepared pattern by index,
// and Mapping refers to its two signature forms. All indices belong to the
// same publication, so moving the two arrays never requires pointer fixups.
typedef struct ttx_representation_type {
  U8 kind;
  U8 value;
  U8 byte_order;
  union {
    Count stride;
    struct { Count form; Count stride; } range;
    struct { Count input; Count output; } mapping;
  } data;
#ifdef __cplusplus
  enum class Kind : U8 {
    Primitive = TTX_FORM_PRIMITIVE,
    Range = TTX_FORM_RANGE,
    Mapping = TTX_FORM_MAPPING,
  };
  using Value = ttx_schema::Value;
  using ByteOrder = ttx_schema::ByteOrder;
  constexpr ttx_representation_type() : kind(TTX_FORM_PRIMITIVE), value(0), byte_order(0), data() {}
  constexpr auto get_kind() const -> Kind { return static_cast<Kind>(kind); }
  constexpr auto get_value() const -> Value { return static_cast<Value>(value); }
  constexpr auto get_byte_order() const -> ByteOrder { return static_cast<ByteOrder>(byte_order); }
  constexpr auto get_extent() const -> Count;
#endif
} ttx_representation_type;

// Each form's transfer entries are contiguous and ordered by byte offset.
// count is the cumulative primitive count through this entry. Subtracting the
// preceding count gives this run's length, while a binary search can locate a
// logical position without storing both its start and length in every entry.
typedef struct ttx_representation_element {
  ttx_representation_type type;
  Count offset;
  Count count;
#ifdef __cplusplus
  constexpr ttx_representation_element(
      ttx_representation_type type = ttx_representation_type(),
      Count offset = 0, Count count = 0)
      : type(type), offset(offset), count(count) {}
#endif
} ttx_representation_element;

// A publication has one metadata array. Interval and Repeat records describe
// composite boundaries in logical primitive coordinates. Form headers describe
// slices of the two arrays and their enclosing byte geometry. A header is not
// visited as a boundary, and each form's boundary slice contains only Interval
// and Repeat records. Repeated patterns and callable signatures reuse headers
// by index rather than owning further arrays or Representation objects.
typedef struct ttx_representation_composite {
  U8 kind;
  union {
    struct { Count first; Count end; Count repeats; Count stride; Count count; } interval;
    struct { Count form; Count first; Count repeats; Count stride; Count count; U8 root; } repeat;
    struct { Count extent; Count alignment; Count elements_first; Count elements_end;
             Count composites_first; Count composites_end; } form;
  } data;
#ifdef __cplusplus
  enum class Kind : U8 { Interval = TTX_FORM_INTERVAL, Repeat = TTX_FORM_REPEAT, Form = TTX_FORM_HEADER };
  constexpr ttx_representation_composite(Kind kind = Kind::Interval)
      : kind(static_cast<U8>(kind)), data() {}
  constexpr auto get_kind() const -> Kind { return static_cast<Kind>(kind); }
  constexpr auto get_count() const -> Count {
    return get_kind() == Kind::Interval ? data.interval.count : data.repeat.count;
  }
#endif
} ttx_representation_composite;

struct ttx_representation;

// A primitive answer borrows its inline Type and the publication that gives
// meaning to any signature indices. Coordinates describe the selected instance,
// independently of the record used to compress its repetitions.
typedef struct ttx_representation_position {
  const struct ttx_representation* owner;
  const ttx_representation_type* type;
  Count offset;
  Count index;
#ifdef __cplusplus
  constexpr ttx_representation_position(
      const ttx_representation* owner = nullptr,
      const ttx_representation_type* type = nullptr,
      Count offset = 0, Count index = 0)
      : owner(owner), type(type), offset(offset), index(index) {}
  constexpr auto compatible(const ttx_representation_position& other) const -> Bool;
#endif
} ttx_representation_position;

// Representation borrows exactly two linear arrays and selects a form header.
// Scalars, repetitions, boundaries and signature forms are records in those
// arrays. There are no per element owners or separately allocated subpatterns.
// The output owner retains both buffers, while source Schemas can disappear
// after compilation. The whole form interval is implicit for every form.
typedef struct ttx_representation {
  const ttx_representation_element* elements;
  const ttx_representation_composite* composites;
  Count form;
#ifdef __cplusplus
  using Type = ttx_representation_type;
  using Element = ttx_representation_element;
  using Composite = ttx_representation_composite;
  using Position = ttx_representation_position;
  using Value = ttx_schema::Value;
  using ByteOrder = ttx_schema::ByteOrder;
  constexpr ttx_representation(const Element* elements, const Composite* composites, Count form)
      : elements(elements), composites(composites), form(form) {}
  constexpr auto get_extent() const -> Count { return composites[form].data.form.extent; }
  constexpr auto get_alignment() const -> Count { return composites[form].data.form.alignment; }
  constexpr auto get_elements() const -> Perimortem::Core::View::Vector<Element> {
    const auto& header = composites[form].data.form;
    return Perimortem::Core::View::Vector<Element>(
        elements ? elements + header.elements_first : nullptr,
        header.elements_end - header.elements_first);
  }
  constexpr auto get_composites() const -> Perimortem::Core::View::Vector<Composite> {
    const auto& header = composites[form].data.form;
    return Perimortem::Core::View::Vector<Composite>(composites + header.composites_first,
        header.composites_end - header.composites_first);
  }
  constexpr auto get_count() const -> Count {
    const auto entries = get_elements();
    return entries.get_size() ? entries[entries.get_size() - 1].count : 0;
  }
  constexpr auto get_composite_count() const -> Count {
    const auto entries = get_composites();
    return entries.get_size() ? entries[entries.get_size() - 1].get_count() : 0;
  }
  constexpr auto get_form(Count index) const -> ttx_representation {
    return ttx_representation(elements, composites, index);
  }
  constexpr auto get_abi() const -> const ttx_representation& { return *this; }
  static auto compile(const ttx_schema& schema, Perimortem::Memory::Allocator::Arena& arena)
      -> Perimortem::Utility::Result<const ttx_representation&, Ttx::Data::Status>;
  constexpr auto compatible(const ttx_representation& other) const -> Bool;
  constexpr auto compatible_type(const Type& a, const ttx_representation& other, const Type& b) const -> Bool;
  auto at(Count index) const -> Perimortem::Utility::Result<Position, Ttx::Data::Status>;
  auto next(Count offset) const -> Perimortem::Utility::Result<Position, Ttx::Data::Status>;
 private:
  static constexpr auto compare_elements(ttx_representation a, Count a_first, Count a_offset,
      ttx_representation b, Count b_first, Count b_offset, Count count) -> Bool;
  static constexpr auto compare_composites(ttx_representation a, Count a_first, Count a_offset,
      ttx_representation b, Count b_first, Count b_offset, Count count,
      Bool a_root = False, Bool b_root = False) -> Bool;
#endif
} ttx_representation;

typedef struct ttx_representation_allocator {
  void* source;
  void* (*allocate)(void* source, Count bytes, Count alignment);
} ttx_representation_allocator;

PERIMORTEM_C ttx_data_status ttx_representation_compile(
    const ttx_schema* schema, ttx_representation_allocator allocator,
    const ttx_representation** result);
PERIMORTEM_C U8 ttx_representation_compatible(
    const ttx_representation* source, const ttx_representation* destination);
PERIMORTEM_C ttx_data_status ttx_representation_at(
    const ttx_representation* source, Count index, ttx_representation_position* result);
PERIMORTEM_C ttx_data_status ttx_representation_next(
    const ttx_representation* source, Count offset, ttx_representation_position* result);

#endif
