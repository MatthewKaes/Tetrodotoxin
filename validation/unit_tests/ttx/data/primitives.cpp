// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/fixtures.hpp"

using namespace Validation::FlowTests;

// Every getter crosses the C module boundary with its declared primitive
// result type. The resulting Storage is a real C ABI record, including a
// callable with an agreed signature. This catches width, signedness, floating
// register and pointer carrier mistakes that a U32 example cannot expose.
PERIMORTEM_UNIT_TEST(TtxFlow, primitive_abi) {
  Module module;
  ASSERT(module.is_set());

  Reader reader{module.primitive_schema(), PROVIDES_FRAGMENT};
  Flow flow;
  ASSERT(
      flow.connect(reader.query(), module.primitives()) ==
      Flow::Status::Success);

  Module::Primitives output = {};
  ASSERT(Copy::flow(flow, storage(reader.schema, output)) == Status::Success);

  EXPECT_EQ(output.u8, U8(251));
  EXPECT_EQ(output.u16, U16(513));
  EXPECT_EQ(output.u32, U32(1234567));
  EXPECT_EQ(output.u64, U64(0xfedcba9876543210ULL));

  EXPECT_EQ(output.s8, S8(-12));
  EXPECT_EQ(output.s16, S16(-1234));
  EXPECT_EQ(output.s32, S32(-123456));
  EXPECT_EQ(output.s64, S64(-123456789012LL));

  EXPECT_EQ(output.r32, R32(1.25));
  EXPECT_EQ(output.r64, R64(-2.5));

  ASSERT(output.pointer);
  EXPECT_EQ(*static_cast<const U32*>(output.pointer), U32(42));

  ASSERT(output.function);
  EXPECT_EQ(output.function(41), U32(42));
}
