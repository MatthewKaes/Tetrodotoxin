// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/fixtures.hpp"
#include "validation/unit_tests/ttx/data/measurement.hpp"
#include "validation/unit_tests/ttx/data/module.hpp"

using namespace Validation::FlowTests;

// C publishes one current U32 and increments the counter when its loan is
// released. Semantic must keep that observation alive until the host write
// completes. No C++ object or provider state layout crosses the boundary.
PERIMORTEM_UNIT_TEST(TtxFlow, one_c_value) {
  Module provider;
  ASSERT(provider.symbol);
  U32 next = 42, output = 0;
  EXPECT(
      extract(provider.counter(next), access(integer, output)) ==
      Status::Success);
  EXPECT_EQ(output, U32(42));
  EXPECT_EQ(next, U32(43));
}
PERIMORTEM_UNIT_TEST(TtxFlow, incompatible_value_is_not_observed) {
  Module provider;
  ASSERT(provider.symbol);
  U32 next = 42;
  R32 output = -1;
  EXPECT(
      extract(provider.counter(next), access(real, output)) ==
      Status::Incompatible);
  EXPECT_EQ(next, U32(42));
  EXPECT_EQ(output, R32(-1));
}

// Both providers advertise 100 U32s through the same binding. One computes
// each observation separately, while the other fills a private four value
// cache. Each access publishes one complete unit and releases it before the
// next read. The number of cache fills differs without changing the contract.
PERIMORTEM_UNIT_TEST(TtxFlow, scalar_and_batched_generation) {
  Module provider;
  ASSERT(provider.symbol);
  Module::State scalar = {}, batched = {.batch = 4};
  U32 a[100] = {}, b[100] = {};
  const auto shape = Schema::range(integer, 100, 4, 400, 100, 4);
  EXPECT(extract(provider.source(scalar), access(shape, a)) == Status::Success);
  EXPECT(
      extract(provider.source(batched), access(shape, b)) == Status::Success);
  EXPECT_EQ(memcmp(a, b, sizeof(a)), 0);
  EXPECT_EQ(a[99], U32(99));
  EXPECT_EQ(scalar.materializations, Count(100));
  EXPECT_EQ(batched.materializations, Count(25));
  EXPECT_EQ(batched.largest, Count(16));
  EXPECT_EQ(batched.releases, Count(100));
}

// Both endpoints are C thunks. A repeated source coordinate requests a new
// observation each time, so it cannot be optimized into copying one cached
// result. Every destination has exactly one supplier in ascending order.
PERIMORTEM_UNIT_TEST(TtxFlow, bounded_streaming_and_repeated_observations) {
  Module provider;
  ASSERT(provider.symbol);
  Module::State source = {.batch = 4}, destination = {};
  EXPECT(
      extract(provider.source(source), provider.destination(destination)) ==
      Status::Success);
  EXPECT_EQ(destination.output[99], U32(99));
  U32 output[2] = {};
  const auto pair = Schema::range(integer, 2, 4, 8, 2, 4);
  const Flow::Assignment repeated = {Index(0), Index(0), 2, 0, 1};
  EXPECT(
      extract(provider.source(source), access(pair, output), {&repeated, 1}) ==
      Status::Success);
  EXPECT_EQ(output[0], U32(100));
  EXPECT_EQ(output[1], U32(101));
}

// A million output observations require no million entry execution inventory.
// The source retains one U32 and the sink only a count and sum. Inline
// completions must use bounded stack space and perform no dispatch allocation.
PERIMORTEM_UNIT_TEST(TtxFlow, large_repetition_has_bounded_storage) {
  Module provider;
  ASSERT(provider.symbol);
  U32 next = 0;
  Module::Sink sink = {};
  const Flow::Assignment repeat = {Index(0), Index(0), 1000000, 0, 1};
  Measurement measurement;
  const auto status =
      extract(provider.counter(next), provider.sink(sink), {&repeat, 1});
  measurement.stop();
  EXPECT(status == Status::Success);
  EXPECT_EQ(sink.count, Count(1000000));
  EXPECT_EQ(sink.sum, U64(499999500000));
  EXPECT_EQ(measurement.get_allocations(), Count(0));
}
