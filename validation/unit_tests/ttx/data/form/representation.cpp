// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/form/representation.hpp"

#include "validation/unit_test.hpp"

#include "ttx/data/form/schema.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;
using Perimortem::Memory::Allocator::Arena;

static Validation::Harness TtxRepresentation = {
  .name = "TTX::Data::Form::Representation"_view};
static constexpr auto u32 = Schema::primitive(Schema::Value::U32);

// Both the source tree and its placement array disappear before lookup. The
// prepared result must carry its own facts, including indices derived from the
// source structure. Mutating the original leaf cannot change that publication.
PERIMORTEM_UNIT_TEST(TtxRepresentation, independent_lifetime) {
  Arena arena;
  const Representation* prepared = nullptr;
  {
    auto value = u32;
    const Schema::Position entries[] = {{value, 0}, {value, 8}};
    const auto schema = Schema::composite({entries, 2}, 12, 4);
    Representation::compile(schema, arena)
        .visit(
            [&](const Representation& result) { prepared = &result; },
            [&](Status) { EXPECT(false); });
    value.data.value.type = TTX_SCHEMA_R32;
  }

  ASSERT(prepared);
  prepared->next(8).visit(
      [&](Representation::Position entry) {
        EXPECT_EQ(entry.offset, Count(8));
        EXPECT(entry.get_value() == Schema::Value::U32);
      },
      [&](Status) { EXPECT(false); });
}

// Scalar ranges stay compact at any size. Only the repeated element and the
// range itself need publication storage, and the last position uses arithmetic.
PERIMORTEM_UNIT_TEST(TtxRepresentation, billion_positions) {
  Arena arena;
  const auto source = Schema::range(u32, 1000000000, 4, 4000000000, 4);
  Representation::compile(source, arena)
      .visit(
          [&](const Representation& value) {
            EXPECT_EQ(value.get_bytes().get_size(), Count(32));
            value.next(3999999996).visit(
                [&](Representation::Position entry) {
                  EXPECT_EQ(entry.offset, Count(3999999996));
                },
                [&](Status) { EXPECT(false); });
          },
          [&](Status) { EXPECT(false); });
}

// Alternating types retain separate descriptors. Logical lookup crosses the
// enclosing Composite and returns the selected primitive by value.
PERIMORTEM_UNIT_TEST(TtxRepresentation, heterogeneous_index) {
  Arena arena;
  const auto real = Schema::primitive(Schema::Value::R32);
  const Schema::Position entries[] = {{u32, 0}, {real, 8}};
  const auto inner = Schema::composite({entries, 2}, 12, 4);
  const Schema::Position wrapper[] = {{inner, 0}};
  const auto outer = Schema::composite({wrapper, 1}, 12, 4);
  Representation::compile(outer, arena)
      .visit(
          [&](const Representation& value) {
            value.next(8).visit(
                [&](Representation::Position entry) {
                  EXPECT_EQ(entry.offset, Count(8));
                  EXPECT(entry.get_value() == Schema::Value::R32);
                },
                [&](Status) { EXPECT(false); });
          },
          [&](Status) { EXPECT(false); });
}

// Failure leaves the publication slot untouched. A caller can discard its
// preparation arena without having lent an incomplete result to consumers.
PERIMORTEM_UNIT_TEST(TtxRepresentation, invalid_publication) {
  Arena arena;
  auto truncated = u32;
  truncated.extent = 1;
  auto misaligned = u32;
  misaligned.alignment = 1;
  const auto padding = Schema::composite({}, 8, 8);
  const Schema failures[] = {truncated, misaligned, padding};
  for (const auto& source : failures) {
    Representation::compile(source, arena)
        .visit(
            [&](const Representation&) { EXPECT(false); },
            [&](Status status) { EXPECT(status == Status::Invalid); });
  }

  Schema cyclic{};
  const Schema::Position entry[] = {{cyclic, 0}};
  cyclic = Schema::composite({entry, 1}, 4, 4);
  Representation::compile(cyclic, arena)
      .visit(
          [&](const Representation&) { EXPECT(false); },
          [&](Status status) { EXPECT(status == Status::Invalid); });
}

// One repetition has no observable stride. Preparation normalizes that fact
// so both logical and physical lookup reach the value through the same shape.
PERIMORTEM_UNIT_TEST(TtxRepresentation, singleton_range) {
  Arena arena;
  const auto source = Schema::range(u32, 1, 0, 4, 4);
  Representation::compile(source, arena)
      .visit(
          [&](const Representation& value) {
            value.next(0).visit(
                [&](Representation::Position entry) {
                  EXPECT_EQ(entry.offset, Count(0));
                  EXPECT(entry.get_value() == Schema::Value::U32);
                },
                [&](Status) { EXPECT(false); });
            value.next(1).visit(
                [&](Representation::Position) { EXPECT(false); },
                [&](Status status) { EXPECT(status == Status::Bounds); });
          },
          [&](Status) { EXPECT(false); });
}

// The C entry publishes only after the whole compilation succeeds. Its caller
// may already have a usable publication in the slot, which failure must retain.
PERIMORTEM_UNIT_TEST(TtxRepresentation, publication_on_error) {
  Arena arena;
  const Representation sentinel{};
  const Representation* output = &sentinel;
  auto source = u32;
  source.extent = 1;
  const ttx_representation_allocator allocator = {
    &arena, [](void* owner, Count size, Count) -> void* {
      return static_cast<Arena*>(owner)->allocate(size).get_data();
    }};
  EXPECT(
      ttx_representation_compile(&source, allocator, &output) ==
      TTX_DATA_INVALID);
  EXPECT(output == &sentinel);
}

// Repeating or grouping unit does not manufacture hidden data positions.
// With no payload, no consumer can distinguish those source spellings.
PERIMORTEM_UNIT_TEST(TtxRepresentation, nested_units) {
  Arena arena;
  const auto empty = Schema::composite({}, 0);
  const auto repeated = Schema::range(empty, 1000000000, 8, 0, 8);
  const Schema::Position children[] = {{empty, 0}, {repeated, 0}};
  const auto grouped = Schema::composite({children, 2}, 0, 8);
  Representation::compile(grouped, arena)
      .visit(
          [&](const Representation& value) {
            EXPECT_EQ(value.get_bytes().get_size(), Count(4));
            EXPECT_EQ(value.get_extent(), Count(0));
            EXPECT_EQ(value.get_alignment(), Count(1));
          },
          [&](Status) { EXPECT(false); });
}

// Compilation merges neighboring progressions rather than requiring the
// author to spell one large Range. The first value from the second source
// remains observable at byte offset 12 after their descriptors are merged.
PERIMORTEM_UNIT_TEST(TtxRepresentation, adjacent_ranges) {
  Arena arena;
  const auto first = Schema::range(u32, 3, 4, 12, 4);
  const auto second = Schema::range(u32, 5, 4, 20, 4);
  const Schema::Position pieces[] = {
    Schema::Position(first, 0), Schema::Position(second, 12)};
  const auto source =
      Schema::composite(View::Vector<Schema::Position>(pieces, 2), 32, 4);

  Representation::compile(source, arena)
      .visit(
          [&](const Representation& prepared) {
            EXPECT_EQ(prepared.get_bytes().get_size(), Count(8));
            prepared.next(12).visit(
                [&](Representation::Position position) {
                  EXPECT_EQ(position.offset, Count(12));
                },
                [&](Status) { EXPECT(false); });
          },
          [&](Status) { EXPECT(false); });
}

// Two different primitive types form one repeated pattern. Neither transfer
// metadata nor boundary metadata may grow with a billion repetitions. The
// last value's physical coordinate distinguishes the pattern stride from
// a packed array of either primitive alone.
PERIMORTEM_UNIT_TEST(TtxRepresentation, repeated_records) {
  Arena arena;
  Arena other_arena;
  const auto real = Schema::primitive(Schema::Value::R32);
  const Schema::Position fields[] = {
    Schema::Position(u32, 0), Schema::Position(real, 8)};
  const auto record =
      Schema::composite(View::Vector<Schema::Position>(fields, 2), 16, 4);
  const auto source = Schema::range(record, 1000000000, 16, 16000000000ULL, 4);

  Representation::compile(source, arena)
      .visit(
          [&](const Representation& prepared) {
            EXPECT_EQ(prepared.get_bytes().get_size(), Count(80));
            Representation::compile(source, other_arena)
                .visit(
                    [&](const Representation& other) {
                      EXPECT(prepared.compatible(other));
                    },
                    [&](Status) { EXPECT(false); });
            prepared.next(15999999992ULL)
                .visit(
                    [&](Representation::Position position) {
                      EXPECT_EQ(position.offset, Count(15999999992ULL));
                    },
                    [&](Status) { EXPECT(false); });
          },
          [&](Status) { EXPECT(false); });
}

// A foreign owner can publish canonical bytes without our compiler. Identical
// buffers at independent addresses establish the same contract. Segmentation
// has already been normalized by each producer before this boundary.
PERIMORTEM_UNIT_TEST(TtxRepresentation, foreign_publication) {
  const U8 first[] = {0x11, 0x40, 0x40, 0x00, 0x03, 0x04, 0x00, 0x01};
  const U8 second[] = {0x11, 0x40, 0x40, 0x00, 0x03, 0x04, 0x00, 0x01};
  const Representation a(first, sizeof(first));
  const Representation b(second, sizeof(second));

  EXPECT(a.compatible(b));
  EXPECT(b.compatible(a));
}

// Both records expose the same four U32s, but their struct bodies differ:
// two pairs versus one and three. Those boundaries remain in the descriptor
// stream, so byte comparison rejects the different contracts directly.
PERIMORTEM_UNIT_TEST(TtxRepresentation, boundary_mismatch) {
  Arena arena;
  const Schema::Position pair_fields[] = {{u32, 0}, {u32, 4}};
  const Schema::Position triple_fields[] = {{u32, 0}, {u32, 4}, {u32, 8}};
  const Schema::Position single_field(u32, 0);
  const auto pair = Schema::composite(View::Vector<Schema::Position>(pair_fields, 2), 8, 4);
  const auto triple = Schema::composite(View::Vector<Schema::Position>(triple_fields, 3), 12, 4);
  const auto single = Schema::composite(View::Vector<Schema::Position>(&single_field, 1), 4, 4);
  const Schema::Position two_pairs[] = {{pair, 0}, {pair, 8}};
  const Schema::Position one_three[] = {{single, 0}, {triple, 4}};
  const auto a = Schema::composite(View::Vector<Schema::Position>(two_pairs, 2), 16, 4);
  const auto b = Schema::composite(View::Vector<Schema::Position>(one_three, 2), 16, 4);

  Representation::compile(a, arena).visit(
      [&](const Representation& left) {
        Representation::compile(b, arena).visit(
            [&](const Representation& right) {
              EXPECT_NOT(left.compatible(right));
              EXPECT_NOT(right.compatible(left));
            },
            [&](Status) { EXPECT(false); });
      },
      [&](Status) { EXPECT(false); });
}

// One author repeats a record containing two nested pairs. Another lists four
// copies explicitly. Compaction and body sharing produce identical streams
// while preserving each nested struct boundary.
PERIMORTEM_UNIT_TEST(TtxRepresentation, boundary_patterns) {
  Arena arena;
  const Schema::Position pair_fields[] = {{u32, 0}, {u32, 4}};
  const auto pair = Schema::composite(View::Vector<Schema::Position>(pair_fields, 2), 8, 4);
  const Schema::Position record_fields[] = {{pair, 0}, {pair, 8}};
  const auto record = Schema::composite(View::Vector<Schema::Position>(record_fields, 2), 16, 4);
  const auto repeated = Schema::range(record, 4, 16, 64, 4);
  const Schema::Position listed[] = {{record, 0}, {record, 16}, {record, 32}, {record, 48}};
  const auto explicit_record = Schema::composite(View::Vector<Schema::Position>(listed, 4), 64, 4);

  Representation::compile(repeated, arena).visit(
      [&](const Representation& left) {
        Representation::compile(explicit_record, arena).visit(
            [&](const Representation& right) {
              EXPECT(left.compatible(right));
              EXPECT(right.compatible(left));
            },
            [&](Status) { EXPECT(false); });
      },
      [&](Status) { EXPECT(false); });
}
