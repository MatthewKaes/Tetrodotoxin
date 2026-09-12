// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/form/schema.hpp"

#include "validation/unit_test.hpp"
#include "validation/unit_tests/ttx/data/preparation.hpp"

#include <stddef.h>

using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;
using Ttx::Data::Form::Schema;
using Validation::DataTests::Preparation;

static Validation::Harness TtxSchema = {.name = "TTX::Data::Form::Schema"_view};
static constexpr auto integer = Schema::primitive(Schema::Value::U32);

// Offsets describe a real C record, including the space after its last field.
// Data does not recompute that layout from the type list. The same published
// description can be used by thunks that never store a Record at all.
PERIMORTEM_UNIT_TEST(TtxSchema, native_record_geometry) {
  Preparation prepare;
  struct Record {
    U16 tag;
    R64 energy;
    U32 frame;
  };

  const auto tag = Schema::primitive(Schema::Value::U16);
  const auto real = Schema::primitive(Schema::Value::R64);
  const Schema::Position fields[] = {
    {tag, offsetof(Record, tag)},
    {real, offsetof(Record, energy)},
    {integer, offsetof(Record, frame)},
  };
  const auto record =
      Schema::composite({fields, 3}, sizeof(Record), alignof(Record));
  ASSERT(prepare.validate(record) == Status::Success);

  EXPECT_EQ(record.get_extent(), Count(sizeof(Record)));
  prepare(record).at(2).visit(
      [&](const Representation::Position& value) {
        EXPECT_EQ(value.offset, Count(offsetof(Record, frame)));
      },
      [&](Status) { EXPECT(false); });

  auto truncated = record;
  truncated.extent = offsetof(Record, frame) + sizeof(U32);
  EXPECT_NOT(prepare(record).compatible(prepare(truncated)));
}

// Both encodings describe four adjacent U32s. One stores four entries and the
// other stores one repeated entry. Different root and leaf addresses establish
// that pointer identity is only a fast path, never required for agreement.
PERIMORTEM_UNIT_TEST(TtxSchema, composite_and_range) {
  Preparation prepare;
  auto foreign_integer = integer;
  const Schema::Position entries[] = {
    {integer, 0},
    {integer, 4},
    {integer, 8},
    {integer, 12},
  };
  const auto flat = Schema::composite({entries, 4}, 16, 4);
  const auto range = Schema::range(foreign_integer, 4, 4, 16, 4);
  ASSERT(prepare.validate(flat) == Status::Success);
  ASSERT(prepare.validate(range) == Status::Success);

  EXPECT(prepare(flat).compatible(prepare(range)));
  EXPECT(prepare(range).compatible(prepare(flat)));

  auto changed = foreign_integer;
  changed.data.value.type = static_cast<U8>(Schema::Value::R32);
  const auto incompatible = Schema::range(changed, 4, 4, 16, 4);
  EXPECT_NOT(prepare(flat).compatible(prepare(incompatible)));
}

// The nested Composite contributes [0, 2) while transfer sees five primitive
// positions. Its second member has logical index one, independently of the
// retained boundary and of the compressed transfer entry that contains it.
PERIMORTEM_UNIT_TEST(TtxSchema, nested_group_lookup) {
  Preparation prepare;
  const Schema::Position pair[] = {{integer, 0}, {integer, 4}};
  const auto group = Schema::composite({pair, 2}, 8, 4);
  const auto range = Schema::range(integer, 2, 4, 8, 4);
  const Schema::Position entries[] = {{group, 0}, {range, 8}, {integer, 16}};
  const auto outer = Schema::composite({entries, 3}, 20, 4);
  ASSERT(prepare.validate(outer) == Status::Success);

  prepare(outer).at(1).visit(
      [&](const Representation::Position& entry) {
        EXPECT_EQ(entry.offset, Count(4));
        EXPECT(entry.representation->compatible(prepare(integer)));
      },
      [&](Status) { EXPECT(false); });

  prepare(outer).at(3).visit(
      [&](const Representation::Position& entry) {
        EXPECT_EQ(entry.offset, Count(12));
      },
      [&](Status) { EXPECT(false); });

  const auto ungrouped = Schema::range(integer, 5, 4, 20, 4);
  EXPECT_NOT(prepare(outer).compatible(prepare(ungrouped)));
}

// A billion observations still have two compact descriptions. Admission and
// comparison visit the element contract rather than expanding the range. No
// billion element buffer is needed to negotiate the promise.
PERIMORTEM_UNIT_TEST(TtxSchema, compact_large_range) {
  Preparation prepare;
  const auto other = Schema::primitive(Schema::Value::U32);
  const auto a = Schema::range(integer, 1000000000, 4, 4000000000, 4);
  const auto b = Schema::range(other, 1000000000, 4, 4000000000, 4);
  ASSERT(prepare.validate(a) == Status::Success);
  ASSERT(prepare.validate(b) == Status::Success);

  EXPECT(prepare(a).compatible(prepare(b)));
  EXPECT(prepare(a).compatible(prepare(a)));

  prepare(a).at(999999999).visit(
      [&](const Representation::Position& entry) {
        EXPECT_EQ(entry.offset, Count(3999999996));
      },
      [&](Status) { EXPECT(false); });
}

// A callable's carrier is explicitly declared. Empty input and output do not
// imply a zero sized function pointer, and an instance thunk includes its self
// pointer in the input contract rather than hiding it in an ABI convention.
PERIMORTEM_UNIT_TEST(TtxSchema, callable_schema) {
  Preparation prepare;
  const auto empty = Schema::composite({}, 0);
  const auto pointer = Schema::primitive(Schema::Value::Pointer);
  const Schema::Position inputs[] = {{pointer, 0}};
  const auto instance = Schema::composite({inputs, 1}, 8, 8);

  const auto free = Schema::mapping(empty, empty);
  const auto thunk = Schema::mapping(instance, empty);
  EXPECT(prepare.validate(free) == Status::Success);
  EXPECT(prepare.validate(thunk) == Status::Success);

  EXPECT_EQ(free.get_extent(), Count(8));
  EXPECT_NOT(prepare(free).compatible(prepare(thunk)));
}

// Admission catches incorrect fixed facts once. Runtime operations require an
// admitted immutable descriptor so they never repeat this walk for each GEP.
PERIMORTEM_UNIT_TEST(TtxSchema, schema_validation) {
  Preparation prepare;
  Schema::Position entries[] = {{integer, 0}, {integer, 2}};
  auto shape = Schema::composite({entries, 2}, 8, 4);
  EXPECT(prepare.validate(shape) == Status::Invalid);

  entries[1].offset = 4;
  EXPECT(prepare.validate(shape) == Status::Success);

  entries[0].schema = &shape;
  EXPECT(prepare.validate(shape) == Status::Invalid);

  const auto huge = Schema::range(integer, Count(-1), 4, Count(-1), 4);
  EXPECT(prepare.validate(huge) == Status::Overflow);
}

// Factoring a range into pairs must not turn contract comparison into a walk
// over all billion values. These schemas have the same byte sequence but use
// different repetition factors and independently constructed leaf records.
PERIMORTEM_UNIT_TEST(TtxSchema, factored_ranges) {
  Preparation prepare;
  const auto other = Schema::primitive(Schema::Value::U32);
  const auto pair = Schema::range(integer, 2, 4, 8, 4);
  const auto nested = Schema::range(pair, 500000000, 8, 4000000000, 4);
  const auto flat = Schema::range(other, 1000000000, 4, 4000000000, 4);
  EXPECT(prepare.validate(nested) == Status::Success);
  EXPECT(prepare.validate(flat) == Status::Success);

  EXPECT(prepare(nested).compatible(prepare(flat)));
  EXPECT(prepare(flat).compatible(prepare(nested)));
}

// A wide composite stores each child's logical start. Comparison visits each
// entry once, and a random indexed lookup seeks directly into that inventory.
// This guards against repeated measurement of the whole tree during agreement.
PERIMORTEM_UNIT_TEST(TtxSchema, wide_composite) {
  Preparation prepare;
  const auto other = Schema::primitive(Schema::Value::U32);
  Schema::Position a[512], b[512];
  for (Count i = 0; i < 512; ++i) {
    a[i] = {integer, i * 4};
    b[i] = {other, i * 4};
  }

  const auto left = Schema::composite({a, 512}, 2048, 4);
  const auto right = Schema::composite({b, 512}, 2048, 4);
  EXPECT(prepare.validate(left) == Status::Success);
  EXPECT(prepare.validate(right) == Status::Success);

  EXPECT(prepare(left).compatible(prepare(right)));
  prepare(right).at(511).visit(
      [&](const Representation::Position& position) {
        EXPECT_EQ(position.offset, Count(2044));
      },
      [&](Status) { EXPECT(false); });
}

// Every source spelling of no values compiles to the same Empty form. Neither
// an unused Range element nor another layer of grouping creates a position.
// The full matrix catches agreement that works only through a third shape.
PERIMORTEM_UNIT_TEST(TtxSchema, empty_equivalence) {
  Preparation prepare;
  const auto real = Schema::primitive(Schema::Value::R32);
  const Schema forms[] = {
    Schema::range(integer, 0, 4, 0, 4),
    Schema::composite({}, 0, 4),
    Schema::range(real, 0, 4, 0, 4),
  };
  for (const auto& source : forms) {
    ASSERT(prepare.validate(source) == Status::Success);
    for (const auto& destination : forms) {
      EXPECT(prepare(source).compatible(prepare(destination)));
    }
  }

  const auto group = Schema::composite({}, 0, 4);
  EXPECT(prepare(group).compatible(prepare(forms[0])));
  EXPECT(prepare(forms[0]).compatible(prepare(group)));
}

// The gap between two records can belong to a child's trailing padding or to
// its parent's placement. Preserve the two record intervals in every spelling
// so this tests padding equivalence independently of boundary agreement.
PERIMORTEM_UNIT_TEST(TtxSchema, padding_equivalence) {
  Preparation prepare;
  const Schema::Position child[] = {{integer, 0}};
  const auto padded = Schema::composite({child, 1}, 8, 4);
  const auto compact = Schema::composite({child, 1}, 4, 4);
  const Schema::Position nested[] = {{padded, 0}, {compact, 8}};
  const Schema::Position flat[] = {{compact, 0}, {compact, 8}};
  const Schema forms[] = {
    Schema::composite({nested, 2}, 12, 4),
    Schema::composite({flat, 2}, 12, 4),
    Schema::range(compact, 2, 8, 12, 4),
  };
  for (const auto& source : forms) {
    ASSERT(prepare.validate(source) == Status::Success);
    for (const auto& destination : forms) {
      EXPECT(prepare(source).compatible(prepare(destination)));
    }
  }

  const Schema::Position shifted[] = {{compact, 4}, {compact, 8}};
  const auto different = Schema::composite({shifted, 2}, 12, 4);
  ASSERT(prepare.validate(different) == Status::Success);
  EXPECT_NOT(prepare(forms[0]).compatible(prepare(different)));
  EXPECT_NOT(prepare(different).compatible(prepare(forms[0])));
}

// Different Range element extents can describe the same occupied values when
// the stride supplies the same gap. Agreement compares that repeated pattern
// once, even with a billion repetitions of the padded element.
PERIMORTEM_UNIT_TEST(TtxSchema, repeated_padding) {
  Preparation prepare;
  const Schema::Position child[] = {{integer, 0}};
  const auto padded = Schema::composite({child, 1}, 8, 4);
  const auto compact = Schema::composite({child, 1}, 4, 4);
  const auto a = Schema::range(padded, 1000000000, 8, 8000000000, 4);
  const auto b = Schema::range(compact, 1000000000, 8, 8000000000, 4);
  ASSERT(prepare.validate(a) == Status::Success);
  ASSERT(prepare.validate(b) == Status::Success);
  EXPECT(prepare(a).compatible(prepare(b)));
  EXPECT(prepare(b).compatible(prepare(a)));
}

// Transparent nesting can surround a Group without changing its boundary.
// Opening that Group during a partial comparison would mistake its first
// member for the Group itself and could accept an actually ungrouped layout.
PERIMORTEM_UNIT_TEST(TtxSchema, nested_group_match) {
  Preparation prepare;
  const Schema::Position pair[] = {{integer, 0}, {integer, 4}};
  const auto group = Schema::composite({pair, 2}, 8, 4);
  const Schema::Position fields[] = {{group, 0}, {integer, 8}};
  const auto flat = Schema::composite({fields, 2}, 12, 4);
  const Schema::Position wrapper[] = {{flat, 0}};
  const auto nested = Schema::composite({wrapper, 1}, 12, 4);
  ASSERT(prepare.validate(nested) == Status::Success);
  EXPECT(prepare(nested).compatible(prepare(flat)));
  EXPECT(prepare(flat).compatible(prepare(nested)));

  const auto ungrouped = Schema::range(integer, 3, 4, 12, 4);
  EXPECT_NOT(prepare(nested).compatible(prepare(ungrouped)));
  EXPECT_NOT(prepare(ungrouped).compatible(prepare(nested)));
}
