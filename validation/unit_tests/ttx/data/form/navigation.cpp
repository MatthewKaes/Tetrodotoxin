// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_test.hpp"
#include "validation/unit_tests/ttx/data/form/visitation.h"
#include "validation/unit_tests/ttx/data/form/preparation.hpp"

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

// A byte coordinate selects the last element of a large range directly from
// its stride. Neither this lookup nor the descriptor expands the other values.
PERIMORTEM_UNIT_TEST(TtxNavigation, range_coordinate) {
  Preparation prepare;
  const auto range = Schema::range(integer, 1000000000, 4, 4000000000, 4);
  const auto& prepared = prepare(range);

  prepared.next(3999999996)
      .visit(
          [&](const Representation::Position& answer) {
            EXPECT(answer.get_value() == integer.get_value());
            EXPECT_EQ(answer.offset, Count(3999999996));
          },
          [&](Status) { EXPECT(false); });

  prepared.next(4000000000)
      .visit(
          [&](auto) { EXPECT(false); },
          [&](Status status) { EXPECT(status == Status::Bounds); });
}

// A nested field is addressed in the enclosing wire object. Descent adds
// the child placement to its own member offset, preserving leading padding.
PERIMORTEM_UNIT_TEST(TtxNavigation, composite_coordinate) {
  Preparation prepare;
  const Schema::Position members[] = {{small, 0}, {integer, 8}};
  const auto record = Schema::composite({members, 2}, 16, 4);
  const Schema::Position entries[] = {{record, 16}};
  const auto root = Schema::composite({entries, 1}, 32, 4);
  const auto& prepared = prepare(root);

  prepared.next(24).visit(
      [&](const Representation::Position& answer) {
        EXPECT(answer.get_value() == integer.get_value());
        EXPECT_EQ(answer.offset, Count(24));
      },
      [&](Status) { EXPECT(false); });
}

// Whole traversal consumes each primitive occurrence in wire order. The
// visitor keeps only a local expected count, while Representation retains the
// active composite path and performs no repeated lookup of earlier fields.
PERIMORTEM_UNIT_TEST(TtxNavigation, padded_range_walk) {
  Preparation prepare;
  const Schema::Position fields[] = {{small, 0}, {integer, 8}};
  const auto record = Schema::composite({fields, 2}, 16, 4);
  const auto records = Schema::range(record, 16, 16, 256, 4);
  const auto& prepared = prepare(records);
  Count seen = 0;

  const auto status = prepared.visit([&](Representation::Position position) {
    const auto expected = seen % 2 ? integer.get_value() : small.get_value();
    EXPECT(position.get_value() == expected);
    EXPECT_EQ(position.offset, (seen / 2) * 16 + (seen % 2 ? 8 : 0));
    ++seen;
    return Status::Success;
  });

  EXPECT(status == Status::Success);
  EXPECT_EQ(seen, Count(32));
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

  prepared.next(0).visit(
      [&](auto) { EXPECT(false); },
      [&](Status status) { EXPECT(status == Status::Bounds); });

  Representation::Position answer;
  EXPECT(ttx_representation_next(nullptr, 0, &answer) == TTX_DATA_INVALID);
  EXPECT(ttx_representation_next(&prepared, 0, nullptr) == TTX_DATA_INVALID);
}

// Deep nesting still accumulates each placement along the selected path. No
// preceding subtree has to be counted to return an ABI byte coordinate.
PERIMORTEM_UNIT_TEST(TtxNavigation, deep_coordinate) {
  Preparation prepare;
  Schema layers[256];
  Schema::Position entries[256];
  for (Count i = 0; i < 256; ++i) {
    entries[i] = {i ? &layers[i - 1] : &integer, 4};
    layers[i] = Schema::composite({&entries[i], 1}, (i + 2) * 4, 4);
  }

  const auto& root = layers[255];
  const auto& prepared = prepare(root);
  prepared.next(0).visit(
      [&](const Representation::Position& answer) {
        EXPECT(answer.get_value() == integer.get_value());
        EXPECT_EQ(answer.offset, Count(1024));
      },
      [&](Status) { EXPECT(false); });
}

// Sharing keeps this binary tree small in the descriptor stream. A coordinate
// selects one path without counting either sibling's implied observations.
// The visitor separately proves that a consumer can stop after a short prefix
// instead of enumerating the entire advertised payload before its first call.
PERIMORTEM_UNIT_TEST(TtxNavigation, shared_body_access) {
  Preparation prepare;
  const auto byte = Schema::primitive(Schema::Value::U8);
  Schema bodies[24];
  Schema::Position fields[24][3];
  for (Count i = 0; i < 24; ++i) {
    const auto& child = i ? bodies[i - 1] : byte;
    const Count extent = child.get_extent();
    fields[i][0] = Schema::Position(child, 0);
    fields[i][1] = Schema::Position(byte, extent);
    fields[i][2] = Schema::Position(child, extent + 1);
    bodies[i] = Schema::composite(
        View::Vector<Schema::Position>(fields[i], 3), extent * 2 + 1);
  }

  const auto& representation = prepare(bodies[23]);
  const Count last = bodies[23].get_extent() - 1;
  ASSERT(representation.get_bytes().get_size() < 2048);
  representation.next(last).visit(
      [&](Representation::Position position) {
        EXPECT_EQ(position.offset, last);
        EXPECT(position.get_value() == Schema::Value::U8);
      },
      [&](Status) { EXPECT(false); });

  Count seen = 0;
  const auto status =
      representation.visit([&](Representation::Position position) {
        EXPECT_EQ(position.offset, seen);
        ++seen;
        return seen == 9 ? Status::Denied : Status::Success;
      });
  EXPECT(status == Status::Denied);
  EXPECT_EQ(seen, Count(9));
}

// The callback is compiled as C and borrows its state only for this call. A
// failure after two observations prevents a third, while a later call starts
// a fresh walk over the same borrowed descriptor.
PERIMORTEM_UNIT_TEST(TtxNavigation, c_streaming_visit) {
  Preparation prepare;
  const auto& representation = prepare(Schema::range(integer, 4, 4, 16, 4));
  visitation_probe probe = {.stop = 2};
  EXPECT(
      observe_representation(&representation, nullptr, 0, &probe) ==
      TTX_DATA_DENIED);
  EXPECT_EQ(probe.count, Count(2));
  EXPECT_EQ(probe.offsets[1], Count(4));

  probe.count = 0;
  probe.stop = 8;
  EXPECT(
      observe_representation(&representation, nullptr, 0, &probe) ==
      TTX_DATA_SUCCESS);
  EXPECT_EQ(probe.count, Count(4));
  EXPECT_EQ(probe.offsets[3], Count(12));
}

// Selecting the last value of a Range must skip its unrequested instances.
// The C callback sees one value, while malformed exact coordinates yield no
// callback. Neither case constructs an inventory proportional to the Range.
PERIMORTEM_UNIT_TEST(TtxNavigation, c_selected_visit) {
  Preparation prepare;
  const auto& representation =
      prepare(Schema::range(integer, 1000000000, 4, 4000000000, 4));
  const Count last[] = {3999999996};
  visitation_probe probe = {.stop = 8};
  EXPECT(
      observe_representation(&representation, last, 1, &probe) ==
      TTX_DATA_SUCCESS);
  EXPECT_EQ(probe.count, Count(1));
  EXPECT_EQ(probe.offsets[0], last[0]);

  const Count interior[] = {1};
  probe.count = 0;
  EXPECT(
      observe_representation(&representation, interior, 1, &probe) ==
      TTX_DATA_BOUNDS);
  EXPECT_EQ(probe.count, Count(0));

  const Count outside[] = {4000000000};
  EXPECT(
      observe_representation(&representation, outside, 1, &probe) ==
      TTX_DATA_BOUNDS);
  EXPECT_EQ(probe.count, Count(0));
}

// A selection crosses repeated real composites and skips their padding. Both
// selected occurrences use the same stored body, with different physical bases.
PERIMORTEM_UNIT_TEST(TtxNavigation, selected_composites) {
  Preparation prepare;
  const Schema::Position member(integer, 4);
  const auto record =
      Schema::composite(View::Vector<Schema::Position>(&member, 1), 12, 4);
  const auto& representation = prepare(Schema::range(record, 3, 16, 44, 4));
  const Count selected[] = {4, 36};
  visitation_probe probe = {.stop = 8};
  EXPECT(
      observe_representation(&representation, selected, 2, &probe) ==
      TTX_DATA_SUCCESS);
  EXPECT_EQ(probe.offsets[0], Count(4));
  EXPECT_EQ(probe.offsets[1], Count(36));

  const Count padding[] = {12};
  probe.count = 0;
  EXPECT(
      observe_representation(&representation, padding, 1, &probe) ==
      TTX_DATA_BOUNDS);
  EXPECT_EQ(probe.count, Count(0));
}
