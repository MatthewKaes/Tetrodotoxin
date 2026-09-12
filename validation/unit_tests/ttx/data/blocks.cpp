// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/fixtures.hpp"
#include "validation/unit_tests/ttx/data/measurement.hpp"

using namespace Validation::FlowTests;

// Value flow provides the simpilest example of how to use the TTX flow engine.
//
// For the first case we'll use it to flow data from two stack locations. While
// this is the least efficent way to copy local data it serves as an easy local
// debug case to work through the engine.
//
// First we need the primitive schema that describes the shared data format.
// In addition to that we need two Pack records to borrow the actual storage.
// Packs need to stay alive throughout extract because the binding thunks refer
// to these records.
//
// Here the data transfer is straight forward, each owner offers stable block
// access. We could do a raw TTX data flow but we can instead let the 
PERIMORTEM_UNIT_TEST(TtxFlow, one_native_value) {
  U32 source = 42, output = 0;
  auto from = view(integer, source);
  auto to = access(integer, output);
  Flow::Request request;
  Measurement measurement;
  request.extract(bind_view(from), bind_access(to));
  measurement.stop();
  ASSERT(request.get_status() == Status::Success);
  EXPECT_EQ(output, U32(42));
  EXPECT_EQ(measurement.get_copies(), Count(1));
  EXPECT_EQ(measurement.get_allocations(), Count(0));
}

// Equal widths alone cannot authorize a byte copy. Semantic can reflow the
// same U16 in another byte order through indexed access, while U32 to R32 is
// rejected before either endpoint is acquired or observed.
PERIMORTEM_UNIT_TEST(TtxFlow, endian_stride_and_rejection) {
  const auto big = Schema::primitive(Value::U16, ByteOrder::Big);
  const auto little = Schema::primitive(Value::U16);
  alignas(U16) U8 source[] = {0x12, 0x34};
  U16 destination = 0;
  EXPECT(
      extract(view(big, source), access(little, destination)) ==
      Status::Success);
  EXPECT_EQ(destination, U16(0x1234));
  U32 value = 10;
  R32 output = -1;
  EXPECT(
      extract(view(integer, value), access(real, output)) ==
      Status::Incompatible);
  EXPECT_EQ(output, R32(-1));
}

// Whole copies preserve overlapping source bytes. Repacking a swizzle into
// overlapping storage needs a separate policy and is not authorized by this
// whole block operation.
PERIMORTEM_UNIT_TEST(TtxFlow, block_engine_format_agreement) {
  U32 values[] = {1, 2, 3, 4};
  const auto three = Schema::range(integer, 3, 4, 12, 3, 4);
  const Block::View source(three, {reinterpret_cast<const U8*>(values), 12});
  const Block::Access destination(
      three, {reinterpret_cast<U8*>(values + 1), 12});
  EXPECT(DataFlow::block(source, destination) == DataStatus::Success);
  EXPECT_EQ(values[1], U32(1));
  EXPECT_EQ(values[2], U32(2));
  EXPECT_EQ(values[3], U32(3));
}

// A packed scalar has weaker alignment without changing its byte order.
// Reflow into naturally aligned storage must preserve its value, not mistake
// an alignment difference for a request to reverse the bytes.
PERIMORTEM_UNIT_TEST(TtxFlow, alignment_adaptation_preserves_byte_order) {
  auto packed = integer;
  packed.alignment = 1;
  U8 bytes[] = {0, 0x78, 0x56, 0x34, 0x12};
  U32 output = 0;
  Block::View source(packed, {bytes + 1, 4});
  EXPECT(
      extract(Pack::view(source), access(integer, output)) == Status::Success);
  EXPECT_EQ(output, U32(0x12345678));
}
