// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/fixtures.hpp"

using namespace Validation::FlowTests;

// Every commit consumes the surface for that call only. The first result is
// already readable when Copy returns, and a later call cannot change its
// Storage.
PERIMORTEM_UNIT_TEST(TtxFlow, block_destinations) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {.provides = PROVIDES_BLOCK, .values = {1, 2, 3, 4}};
  Reader reader{four};

  Flow flow;
  ASSERT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Success);
  EXPECT_EQ(writer.commits, Count(0));

  U32 a[4] = {}, b[4] = {};
  ASSERT(Copy::flow(flow, storage(four, a)) == Status::Success);

  writer.values[0] = 7;
  ASSERT(Copy::flow(flow, storage(four, b)) == Status::Success);
  EXPECT_EQ(a[0], U32(1));
  EXPECT_EQ(b[0], U32(7));

  EXPECT_EQ(writer.commits, Count(2));
}

// A failed whole commit reports its cause. This fixture declines before
// writing, so its sentinel remains intact. That behavior belongs to this
// provider rather than a Copy promise to restore bytes on failure.
PERIMORTEM_UNIT_TEST(TtxFlow, block_failure) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {.provides = PROVIDES_BLOCK, .failure = 1};
  Reader reader{four};

  Flow flow;
  ASSERT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Success);

  U32 output[4] = {99, 99, 99, 99};
  EXPECT(Copy::flow(flow, storage(four, output)) == Status::IoError);
  EXPECT_EQ(output[0], U32(99));
  EXPECT_EQ(writer.commits, Count(1));
}
