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
  EXPECT_EQ(prepared->count, Count(2));
  prepared->at(1).visit(
      [&](Representation::Position entry) {
        EXPECT_EQ(entry.offset, Count(8));
        EXPECT_EQ(entry.representation->data.value.type, TTX_SCHEMA_U32);
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
            EXPECT(value.get_elements().get_size() == 1);
            value.at(999999999).visit(
                [&](Representation::Position entry) {
                  EXPECT_EQ(entry.offset, Count(3999999996));
                },
                [&](Status) { EXPECT(false); });
          },
          [&](Status) { EXPECT(false); });
}

// Alternating types cannot compress to a scalar Range. Their placement table
// still provides direct lookup after the transparent outer wrapper disappears.
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
            EXPECT_EQ(value.data.sequence.size, value.count);
            value.at(1).visit(
                [&](Representation::Position entry) {
                  EXPECT_EQ(entry.offset, Count(8));
                  EXPECT_EQ(
                      entry.representation->data.value.type, TTX_SCHEMA_R32);
                },
                [&](Status) { EXPECT(false); });
          },
          [&](Status) { EXPECT(false); });
}

// Empty input and output remain meaningful for a callable, while the callable
// itself occupies one native pointer. Group and Range spellings of no values
// normalize to Empty and expose no synthetic members for indexing.
PERIMORTEM_UNIT_TEST(TtxRepresentation, unit_and_callable) {
  Arena arena;
  const auto empty = Schema::composite({}, 0);
  const auto other = Schema::range(u32, 0, 4, 0, 4);
  const auto callable = Schema::mapping(empty, other);
  Representation::compile(callable, arena)
      .visit(
          [&](const Representation& value) {
            EXPECT_EQ(value.extent, Count(8));
            EXPECT_EQ(value.alignment, Count(8));
            EXPECT(value.data.mapping.input->kind == TTX_REPRESENTATION_EMPTY);
            EXPECT(value.data.mapping.input->compatible(
                *value.data.mapping.output));
            value.data.mapping.input->at(0).visit(
                [&](Representation::Position) { EXPECT(false); },
                [&](Status status) { EXPECT(status == Status::Bounds); });
          },
          [&](Status) { EXPECT(false); });
}

// Failure leaves the publication slot untouched. A caller can discard its
// preparation arena without having lent an incomplete result to consumers.
PERIMORTEM_UNIT_TEST(TtxRepresentation, invalid_publication) {
  Arena arena;
  auto callable = Schema::mapping(u32, u32);
  callable.extent = 1;
  auto misaligned = u32;
  misaligned.alignment = 1;
  const auto padding = Schema::composite({}, 8, 8);
  const Schema failures[] = {callable, misaligned, padding};
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
                  EXPECT_EQ(
                      entry.representation->data.value.type, TTX_SCHEMA_U32);
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

// Repeating or grouping unit does not manufacture hidden logical positions.
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
            EXPECT(value.kind == TTX_REPRESENTATION_EMPTY);
            EXPECT_EQ(value.count, Count(0));
            EXPECT_EQ(value.extent, Count(0));
            EXPECT_EQ(value.alignment, Count(1));
          },
          [&](Status) { EXPECT(false); });
}

// Compilation merges neighboring progressions rather than requiring the
// author to spell one large Range. Logical positions survive that merge, so
// the first value from the second source still has index three and offset 12.
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
            ASSERT_EQ(prepared.get_elements().get_size(), Count(1));
            EXPECT_EQ(prepared.get_elements()[0].count, Count(8));
            prepared.at(3).visit(
                [&](Representation::Position position) {
                  EXPECT_EQ(position.index, Count(3));
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
            ASSERT_EQ(prepared.get_elements().get_size(), Count(1));
            EXPECT(prepared.get_composites().get_size() <= 1);
            Representation::compile(source, other_arena)
                .visit(
                    [&](const Representation& other) {
                      EXPECT(prepared.compatible(other));
                    },
                    [&](Status) { EXPECT(false); });
            prepared.at(1999999999)
                .visit(
                    [&](Representation::Position position) {
                      EXPECT_EQ(position.offset, Count(15999999992ULL));
                    },
                    [&](Status) { EXPECT(false); });
          },
          [&](Status) { EXPECT(false); });
}

// A foreign owner can publish a different segmentation of the same physical
// progression. Agreement consumes matching prefixes without expanding a run
// or requiring records to share addresses or exact array lengths.
PERIMORTEM_UNIT_TEST(TtxRepresentation, split_publication) {
  Arena arena;
  Representation::compile(u32, arena)
      .visit(
          [&](const Representation& primitive) {
            const Representation::Element split[] = {
              Representation::Element(&primitive, 0, 0, 3, 4),
              Representation::Element(&primitive, 12, 3, 5, 4)};
            const Representation::Element joined(&primitive, 0, 0, 8, 4);
            Representation a(Representation::Kind::Sequence, 32, 4);
            a.count = 8;
            a.data.sequence.elements = split;
            a.data.sequence.size = 2;
            Representation b(Representation::Kind::Sequence, 32, 4);
            b.count = 8;
            b.data.sequence.elements = &joined;
            b.data.sequence.size = 1;

            EXPECT(a.compatible(b));
            EXPECT(b.compatible(a));
          },
          [&](Status) { EXPECT(false); });
}

// Both records expose the same four U32s and the same number of Composite
// intervals. Only their boundaries differ: two pairs versus one and three.
// Agreement must inspect the intervals, not just bytes or interval counts.
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
              EXPECT_EQ(left.composite_count, right.composite_count);
              EXPECT_NOT(left.compatible(right));
              EXPECT_NOT(right.compatible(left));
            },
            [&](Status) { EXPECT(false); });
      },
      [&](Status) { EXPECT(false); });
}

// One author repeats a record containing two nested pairs. Another lists four
// copies explicitly. The physical inventory and all twelve nested intervals
// agree even though one boundary inventory uses a repeat pattern and the other
// mixes individual intervals with compact runs.
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
              EXPECT_EQ(left.composite_count, Count(12));
              EXPECT_EQ(right.composite_count, Count(12));
              EXPECT(left.compatible(right));
              EXPECT(right.compatible(left));
            },
            [&](Status) { EXPECT(false); });
      },
      [&](Status) { EXPECT(false); });
}
