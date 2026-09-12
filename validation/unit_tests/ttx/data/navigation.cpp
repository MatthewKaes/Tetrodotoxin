// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_test.hpp"
#include "validation/unit_tests/ttx/data/preparation.hpp"

#include "ttx/data/form/representation.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;
using Ttx::Data::Form::Schema;
using Validation::DataTests::Preparation;

static Validation::Harness TtxNavigation = {
  .name = "TTX::Data::Form::Navigation"_view};
static constexpr auto integer = Schema::primitive(Schema::Value::U32);
static constexpr auto small = Schema::primitive(Schema::Value::U16);

// The last element of a large range is described by its repetition and stride.
// Neither the schema nor this request needs a buffer of a billion values.
PERIMORTEM_UNIT_TEST(TtxNavigation, root_range_index) {
  Preparation prepare;
  const auto range = Schema::range(integer, 1000000000, 4, 4000000000, 4);
  const auto& prepared = prepare(range);

  prepared.at(999999999).visit(
      [&](const Representation::Position& answer) {
        EXPECT(answer.representation->compatible(prepare(integer)));
        EXPECT_EQ(answer.offset, Count(3999999996));
        EXPECT_EQ(answer.index, Count(999999999));
      },
      [&](Status) { EXPECT(false); });

  prepared.at(1000000000)
      .visit(
          [&](auto) { EXPECT(false); },
          [&](Status status) { EXPECT(status == Status::Bounds); });
}

// Composite boundaries do not hide primitives from transfer indexing. The
// second field remains logical position one while its enclosing interval is
// recorded independently from the sixteen bytes of leading padding.
PERIMORTEM_UNIT_TEST(TtxNavigation, composite_index) {
  Preparation prepare;
  const Schema::Position members[] = {{small, 0}, {integer, 8}};
  const auto record = Schema::composite({members, 2}, 16, 4);
  const Schema::Position entries[] = {{record, 16}};
  const auto root = Schema::composite({entries, 1}, 32, 4);
  const auto& prepared = prepare(root);

  prepared.at(1).visit(
      [&](const Representation::Position& answer) {
        EXPECT(answer.representation->compatible(prepare(integer)));
        EXPECT_EQ(answer.offset, Count(24));
        EXPECT_EQ(answer.index, Count(1));
      },
      [&](Status) { EXPECT(false); });
}

// Flattened logical positions still respect each record's physical stride.
// Alternating primitive widths and interior padding distinguish a positional
// lookup from multiplying every index by the first field's byte width.
PERIMORTEM_UNIT_TEST(TtxNavigation, padded_range_index) {
  Preparation prepare;
  const Schema::Position fields[] = {{small, 0}, {integer, 8}};
  const auto record = Schema::composite({fields, 2}, 16, 4);
  const auto records = Schema::range(record, 16, 16, 256, 4);
  const auto& prepared = prepare(records);

  for (Count i = 0; i < prepared.count; ++i) {
    const auto* expected = i % 2 ? &integer : &small;
    const Count offset = (i / 2) * 16 + (i % 2 ? 8 : 0);
    prepared.at(i).visit(
        [&](const Representation::Position& answer) {
          EXPECT(answer.representation->compatible(prepare(*expected)));
          EXPECT_EQ(answer.offset, offset);
          EXPECT_EQ(answer.index, i);
        },
        [&](Status) { EXPECT(false); });
  }
}

// Physical traversal finds an occupied start at or after the requested byte.
// Checking every byte distinguishes a valid value from its interior bytes and
// surrounding padding. Exact selection policies can compare the returned
// coordinate with their request, as Swizzle admission does.
PERIMORTEM_UNIT_TEST(TtxNavigation, physical_coordinates) {
  Preparation prepare;
  const Schema::Position fields[] = {{small, 0}, {integer, 8}};
  const auto record = Schema::composite({fields, 2}, 16, 4);
  const auto records = Schema::range(record, 4, 16, 64, 4);
  const auto& prepared = prepare(records);

  for (Count offset = 0; offset <= records.extent; ++offset) {
    const Count local = offset % 16;
    const Count expected = offset - local + (!local ? 0 : local <= 8 ? 8 : 16);
    prepared.next(offset).visit(
        [&](const Representation::Position& answer) {
          EXPECT(expected < 64);
          EXPECT_EQ(answer.offset, expected);
        },
        [&](Status status) {
          EXPECT(expected >= 64);
          EXPECT(status == Status::Bounds);
        });
  }
}

// Empty has no occupied position. Invalid C arguments are rejected at the
// entry, while callers holding a Representation can use the typed operations.
PERIMORTEM_UNIT_TEST(TtxNavigation, empty_and_invalid) {
  Preparation prepare;
  const auto empty = Schema::composite({}, 0);
  const auto& prepared = prepare(empty);

  prepared.at(0).visit(
      [&](auto) { EXPECT(false); },
      [&](Status status) { EXPECT(status == Status::Bounds); });

  Representation::Position answer;
  EXPECT(ttx_representation_at(nullptr, 0, &answer) == TTX_DATA_INVALID);
  EXPECT(ttx_representation_next(&prepared, 0, nullptr) == TTX_DATA_INVALID);
}

// Repeated transparent nesting must accumulate every offset while selecting
// the original leaf. This exercises the same descent as a shallow Composite,
// with enough levels to expose accidental dependence on an expanded layout.
PERIMORTEM_UNIT_TEST(TtxNavigation, deep_layout_index) {
  Preparation prepare;
  Schema layers[256];
  Schema::Position entries[256];
  for (Count i = 0; i < 256; ++i) {
    entries[i] = {i ? &layers[i - 1] : &integer, 4};
    layers[i] = Schema::composite({&entries[i], 1}, (i + 2) * 4, 4);
  }

  const auto& root = layers[255];
  const auto& prepared = prepare(root);
  prepared.at(0).visit(
      [&](const Representation::Position& answer) {
        EXPECT(answer.representation->compatible(prepare(integer)));
        EXPECT_EQ(answer.offset, Count(1024));
      },
      [&](Status) { EXPECT(false); });
}
