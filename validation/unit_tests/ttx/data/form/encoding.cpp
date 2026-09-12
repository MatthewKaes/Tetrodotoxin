// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/form/preparation.hpp"

#include "ttx/data/form/compiled.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;
using Validation::DataTests::Preparation;

static Validation::Harness TtxEncoding = {
  .name = "TTX::Data::Form::Encoding"_view};
static constexpr auto byte = Schema::primitive(Schema::Value::U8);
static constexpr auto integer = Schema::primitive(Schema::Value::U32);

template <Data::ByteOrder order>
static constexpr auto word_bytes() -> Static::Bytes<12> {
  Static::Bytes<12> bytes;
  Writer::Binary<order> writer(Access::Bytes(bytes.get_data(), 12));
  writer << U64(0x0102030405060708ULL);
  writer << U32(0x090a0b0c);
  return bytes;
}

static_assert([] {
  const auto bytes = word_bytes<Data::ByteOrder::Big>();
  Reader::Binary<Data::ByteOrder::Big> reader(
      View::Bytes(bytes.get_data(), 12));
  const auto first = reader.read_u64();
  const auto second = reader.read_u32();
  const auto past_end = reader.read_u32();
  return first && second && *first == 0x0102030405060708ULL &&
         *second == 0x090a0b0c && !past_end && !reader.has_content();
}());

// Both evaluation modes use the same integer writer. Literal expected bytes
// also test Big endian so the new constexpr support cannot accidentally make
// a generic Perimortem writer specific to TTX's little endian format.
PERIMORTEM_UNIT_TEST(TtxEncoding, constant_word_io) {
  static constexpr auto little = word_bytes<Data::ByteOrder::Little>();
  static constexpr auto big = word_bytes<Data::ByteOrder::Big>();
  const U8 expected_little[] = {8, 7, 6, 5, 4, 3, 2, 1, 12, 11, 10, 9};
  const U8 expected_big[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

  EXPECT(Data::compare(little.get_data(), expected_little, 12));
  EXPECT(Data::compare(big.get_data(), expected_big, 12));
  EXPECT(
      Data::compare(
          word_bytes<Data::ByteOrder::Little>().get_data(), little.get_data(),
          12));
  EXPECT(
      Data::compare(
          word_bytes<Data::ByteOrder::Big>().get_data(), big.get_data(), 12));

  Reader::Binary<Data::ByteOrder::Little> le(
      View::Bytes(little.get_data(), 12));
  Reader::Binary<Data::ByteOrder::Big> be(View::Bytes(big.get_data(), 12));
  le.read_u64().visit(
      [&] { EXPECT(false); },
      [&](U64 value) { EXPECT_EQ(value, U64(0x0102030405060708ULL)); });
  be.read_u64().visit(
      [&] { EXPECT(false); },
      [&](U64 value) { EXPECT_EQ(value, U64(0x0102030405060708ULL)); });
  le.read_u32().visit(
      [&] { EXPECT(false); },
      [&](U32 value) { EXPECT_EQ(value, U32(0x090a0b0c)); });
  be.read_u32().visit(
      [&] { EXPECT(false); },
      [&](U32 value) { EXPECT_EQ(value, U32(0x090a0b0c)); });
}

// These are literal streams, not bytes generated with Encoding's helpers.
// Crossing each array threshold exercises a new global field width. F3 also
// proves that its trailing U32 follows the lower U64 without padding.
PERIMORTEM_UNIT_TEST(TtxEncoding, depth_bytes) {
  Preparation prepare;
  const U8 f2[] = {
    0x12, 0x00, 0x10, 0x00, 0x00, 0x10, 0x00, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
  };
  const U8 f3[] = {
    0x13, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
  };
  const U8 f4[] = {
    0x14, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
  };

  EXPECT(prepare(Schema::range(byte, 256, 1, 256))
             .compatible(Representation(f2, sizeof(f2))));
  EXPECT(prepare(Schema::range(byte, 65536, 1, 65536))
             .compatible(Representation(f3, sizeof(f3))));
  EXPECT(prepare(Schema::range(byte, 16777216, 1, 16777216))
             .compatible(Representation(f4, sizeof(f4))));
}

// Equal occupied bytes do not erase a real struct boundary. The flat case
// needs two blocks. The array of cells needs a referenced cell header and its
// primitive, even though both payloads occupy 24 bytes at alignment eight.
PERIMORTEM_UNIT_TEST(TtxEncoding, struct_bytes) {
  Preparation prepare;
  const Schema::Position fields[] = {{integer, 0}, {integer, 8}, {integer, 16}};
  const auto flat =
      Schema::composite(View::Vector<Schema::Position>(fields, 3), 24, 8);
  const Schema::Position member(integer, 0);
  const auto cell =
      Schema::composite(View::Vector<Schema::Position>(&member, 1), 8, 8);
  const auto cells = Schema::range(cell, 3, 8, 24, 8);
  const U8 flat_bytes[] = {
    0x11, 0x80, 0x80, 0x01, 0x03, 0x08, 0x00, 0x03,
  };
  const U8 cell_bytes[] = {
    0x11, 0x80, 0x80, 0x01, 0x82, 0x08, 0x00, 0x03,
    0x11, 0x80, 0x80, 0x00, 0x03, 0x04, 0x00, 0x01,
  };

  EXPECT(
      prepare(flat).compatible(Representation(flat_bytes, sizeof(flat_bytes))));
  EXPECT(prepare(cells).compatible(
      Representation(cell_bytes, sizeof(cell_bytes))));
  EXPECT_NOT(prepare(flat).compatible(prepare(cells)));
}

// Consecutive bytes coalesce without swallowing the padding before U32.
// Both ordinary members and an authored byte array produce this same stream.
PERIMORTEM_UNIT_TEST(TtxEncoding, padding_bytes) {
  Preparation prepare;
  const Schema::Position fields[] = {
    {byte, 0},
    {byte, 1},
    {byte, 2},
    {integer, 4},
  };
  const auto bytes = Schema::range(byte, 3, 1, 3);
  const Schema::Position grouped[] = {{bytes, 0}, {integer, 4}};
  const auto flat =
      Schema::composite(View::Vector<Schema::Position>(fields, 4), 8, 4);
  const auto array =
      Schema::composite(View::Vector<Schema::Position>(grouped, 2), 8, 4);
  const U8 expected[] = {
    0x21, 0x40, 0x80, 0x00, 0x01, 0x01, 0x00, 0x03, 0x03, 0x04, 0x04, 0x01,
  };

  EXPECT(prepare(flat).compatible(Representation(expected, sizeof(expected))));
  EXPECT(prepare(flat).compatible(prepare(array)));
}

PERIMORTEM_UNIT_TEST(TtxEncoding, native_profiles) {
  Preparation prepare;
  for (U8 depth = 1; depth <= 8; ++depth) {
    const Count count = Count(1) << (8 * (depth - 1));
    const auto schema = Schema::range(byte, count, 1, count);
    const auto& representation = prepare(schema);

    EXPECT_EQ(representation.get_depth(), depth);
    EXPECT_EQ(representation.get_extent(), count);
    EXPECT_EQ(representation.get_bytes().get_size(), Count(8 * depth));
  }
}

PERIMORTEM_UNIT_TEST(TtxEncoding, field_promotions) {
  Preparation prepare;
  const Schema::Position distant(byte, 256);
  const Schema::Position first(byte, 0);
  const Schema forms[] = {
    Schema::range(byte, 256, 1, 256),
    Schema::composite(View::Vector<Schema::Position>(&distant, 1), 257),
    Schema::range(byte, 2, 256, 257),
    Schema::composite(View::Vector<Schema::Position>(&first, 1), 4096),
    Schema::composite(View::Vector<Schema::Position>(&first, 1), 256, 256),
  };
  for (const auto& schema : forms) {
    EXPECT_EQ(prepare(schema).get_depth(), U8(2));
  }

  // Alternating byte types prevent run compaction, forcing the header count
  // to widen while offsets and primitive codes still fit the narrow profile.
  const auto signed_byte = Schema::primitive(Schema::Value::S8);
  Schema::Position fields[256];
  for (Count i = 0; i < 256; ++i) {
    fields[i] = Schema::Position(i % 2 ? byte : signed_byte, i);
  }

  const auto wide =
      Schema::composite(View::Vector<Schema::Position>(fields, 256), 256);
  EXPECT_EQ(prepare(wide).get_depth(), U8(2));
}

// Each wrapper is a real, distinct body. At 65 headers the deepest referenced
// body starts at block 128, exceeding narrow P even though all extents, counts
// and offsets still fit F1. The whole stream must promote together.
PERIMORTEM_UNIT_TEST(TtxEncoding, reference_promotion) {
  Preparation prepare;
  Schema bodies[65];
  Schema::Position fields[65];
  for (Count i = 0; i < 65; ++i) {
    fields[i] = Schema::Position(i ? &bodies[i - 1] : &byte, 0);
    bodies[i] =
        Schema::composite(View::Vector<Schema::Position>(&fields[i], 1), 1);
  }

  EXPECT_EQ(prepare(bodies[63]).get_depth(), U8(1));
  EXPECT_EQ(prepare(bodies[64]).get_depth(), U8(2));
}

// The first member of the second input range extends the first output run,
// while its remainder starts a singleton. Treating source ranges as atomic
// descriptors would make these two equivalent inputs encode differently.
PERIMORTEM_UNIT_TEST(TtxEncoding, split_run_prefix) {
  Preparation prepare;
  const auto first = Schema::range(integer, 2, 4, 8, 4);
  const auto second = Schema::range(integer, 2, 8, 12, 4);
  const Schema::Position split[] = {{first, 0}, {second, 8}};
  const Schema::Position flat[] = {
    {integer, 0}, {integer, 4}, {integer, 8}, {integer, 16}};
  const auto a =
      Schema::composite(View::Vector<Schema::Position>(split, 2), 20, 4);
  const auto b =
      Schema::composite(View::Vector<Schema::Position>(flat, 4), 20, 4);

  EXPECT(prepare(a).compatible(prepare(b)));
  EXPECT_EQ(prepare(a).get_bytes().get_size(), Count(12));
}

// Gaps between batches prevent a single arithmetic progression. The format
// needs three runs here, but still needs no synthetic struct to hold padding.
PERIMORTEM_UNIT_TEST(TtxEncoding, gapped_batches) {
  Preparation prepare;
  const auto pair = Schema::range(integer, 2, 4, 8, 4);
  const auto repeated = Schema::range(pair, 3, 12, 32, 4);
  const Schema::Position fields[] = {
    {integer, 0},  {integer, 4},  {integer, 12},
    {integer, 16}, {integer, 24}, {integer, 28},
  };
  const auto flat =
      Schema::composite(View::Vector<Schema::Position>(fields, 6), 32, 4);

  EXPECT(prepare(repeated).compatible(prepare(flat)));
  EXPECT_EQ(prepare(repeated).get_bytes().get_size(), Count(16));
}

PERIMORTEM_UNIT_TEST(TtxEncoding, interleaved_runs) {
  Preparation prepare;
  const auto signed_byte = Schema::primitive(Schema::Value::S8);
  const auto even = Schema::range(byte, 3, 4, 9);
  const auto odd = Schema::range(signed_byte, 2, 4, 5);
  const Schema::Position ranges[] = {{even, 0}, {odd, 2}};
  const Schema::Position fields[] = {
    {byte, 0}, {signed_byte, 2}, {byte, 4}, {signed_byte, 6}, {byte, 8},
  };
  const auto a =
      Schema::composite(View::Vector<Schema::Position>(ranges, 2), 9);
  const auto b =
      Schema::composite(View::Vector<Schema::Position>(fields, 5), 9);
  const auto& representation = prepare(a);

  EXPECT(representation.compatible(prepare(b)));
  Count seen = 0;
  const auto status =
      representation.visit([&](Representation::Position position) {
        EXPECT_EQ(position.offset, seen * 2);
        ++seen;
        return Status::Success;
      });
  EXPECT(status == Status::Success);
  EXPECT_EQ(seen, Count(5));
}

// Source ordering and allocation identity cannot influence the body indices.
// Two independent cell declarations share one body in either source order.
PERIMORTEM_UNIT_TEST(TtxEncoding, independent_bodies) {
  Preparation prepare;
  const Schema::Position member(integer, 0);
  const auto cell =
      Schema::composite(View::Vector<Schema::Position>(&member, 1), 4, 4);
  auto other_cell = cell;
  const Schema::Position ordered[] = {{cell, 0}, {other_cell, 8}};
  const Schema::Position reversed[] = {{other_cell, 8}, {cell, 0}};
  const auto a =
      Schema::composite(View::Vector<Schema::Position>(ordered, 2), 12, 4);
  const auto b =
      Schema::composite(View::Vector<Schema::Position>(reversed, 2), 12, 4);

  EXPECT(prepare(a).compatible(prepare(b)));
  EXPECT_EQ(prepare(a).get_bytes().get_size(), Count(16));
}

PERIMORTEM_UNIT_TEST(TtxEncoding, payload_endian) {
  Preparation prepare;
  const auto big_byte =
      Schema::primitive(Schema::Value::U8, Schema::ByteOrder::Big);
  const auto big_integer =
      Schema::primitive(Schema::Value::U32, Schema::ByteOrder::Big);

  EXPECT(prepare(byte).compatible(prepare(big_byte)));
  EXPECT_NOT(prepare(integer).compatible(prepare(big_integer)));
  prepare(big_integer)
      .next(0)
      .visit(
          [&](Representation::Position position) {
            EXPECT(position.get_byte_order() == Schema::ByteOrder::Big);
          },
          [&](Status) { EXPECT(false); });
}

// The compiler's coordinates are Count values, even when a descriptor uses a
// wider block. The largest native count exercises a 256 bit block without
// allocating payload storage or overflowing the final occupied byte.
PERIMORTEM_UNIT_TEST(TtxEncoding, native_count_limit) {
  Preparation prepare;
  const auto huge = Schema::range(byte, Count(-1), 1, Count(-1));
  const auto& representation = prepare(huge);

  EXPECT_EQ(representation.get_depth(), U8(8));
  EXPECT_EQ(representation.get_bytes().get_size(), Count(64));
  representation.next(Count(-2)).visit(
      [&](Representation::Position position) {
        EXPECT_EQ(position.offset, Count(-2));
      },
      [&](Status) { EXPECT(false); });

  representation.next(Count(-1)).visit(
      [&](Representation::Position) { EXPECT(false); },
      [&](Status status) { EXPECT(status == Status::Bounds); });
}

// Final storage is requested only after admission. A rejected source keeps
// both the output slot and its owner's allocation inventory unchanged.
PERIMORTEM_UNIT_TEST(TtxEncoding, final_allocation) {
  struct Owner {
    alignas(Representation) U8 bytes[128];
    Count allocations = 0;
  } owner;
  const ttx_representation_allocator allocator = {
    &owner,
    [](void* source, Count size, Count) -> void* {
      auto& owner = *static_cast<Owner*>(source);
      ++owner.allocations;
      return size <= sizeof(owner.bytes) ? owner.bytes : nullptr;
    },
  };
  const Representation* output = nullptr;
  auto invalid = integer;
  invalid.data.value.type = 11;

  EXPECT(
      ttx_representation_compile(&invalid, allocator, &output) ==
      TTX_DATA_INVALID);
  EXPECT_EQ(owner.allocations, Count(0));
  EXPECT(output == nullptr);

  ASSERT(
      ttx_representation_compile(&integer, allocator, &output) ==
      TTX_DATA_SUCCESS);
  EXPECT_EQ(owner.allocations, Count(1));
  EXPECT(output->compatible(Compiled<integer>::get_representation()));
}
