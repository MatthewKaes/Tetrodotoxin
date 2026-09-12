// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/fixtures.hpp"
#include "validation/unit_tests/ttx/data/measurement.hpp"

#include "ttx/semantic/flow.hpp"

using namespace Validation::FlowTests;

// Establishment needs only a reader schema. Each synchronous Copy then takes
// its own Storage and returns a finished result without a retained request
// object.
PERIMORTEM_UNIT_TEST(TtxFlow, copy_multiple_targets) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {
    .provides = PROVIDES_DIRECT, .values = {10, 20, 30, 40}};
  Reader reader{four};

  Flow flow;
  ASSERT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Success);

  U32 a[4] = {}, b[4] = {};
  Measurement measurement;
  const auto first = Copy::flow(flow, storage(four, a));
  const auto second = Copy::flow(flow, storage(four, b));
  measurement.stop();

  EXPECT(first == Status::Success);
  EXPECT(second == Status::Success);
  EXPECT_EQ(a[3], U32(40));
  EXPECT_EQ(b[0], U32(10));

  EXPECT_EQ(reader.descriptions, Count(1));
  EXPECT_EQ(writer.descriptions, Count(1));
  EXPECT_EQ(writer.binds[0], Count(1));

  EXPECT_EQ(measurement.get_allocations(), Count(0));
  EXPECT_EQ(measurement.get_copies(), Count(2));
}

// Storage checks capacity and Copy checks agreement with the selected Flow.
// Neither failure reaches the provider or writes to the mismatching
// destination.
PERIMORTEM_UNIT_TEST(TtxFlow, copy_target_mismatch) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {.provides = PROVIDES_BLOCK};
  Reader reader{four};

  Flow flow;
  ASSERT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Success);

  U32 small = 99;
  Storage::create(four, {reinterpret_cast<U8*>(&small), sizeof(small)})
      .visit(
          [&](Storage) { EXPECT(false); },
          [&](Status status) { EXPECT(status == Status::Bounds); });

  EXPECT(
      Copy::flow(flow, storage(prepare(integer), small)) ==
      Status::Incompatible);
  EXPECT_EQ(small, U32(99));
  EXPECT_EQ(writer.commits, Count(0));
}

// A Storage can lend its reader without becoming the permanent destination. The
// agreement borrows only the schema, so another Storage receives this call's
// result.
PERIMORTEM_UNIT_TEST(TtxFlow, reusable_reader) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {.provides = PROVIDES_DIRECT, .values = {1, 2, 3, 4}};
  U32 original[4] = {99, 99, 99, 99}, output[4] = {};

  Flow flow;
  ASSERT(
      flow.connect(
          Flow::reader(storage(four, original)), module.writer(writer)) ==
      Flow::Status::Success);

  EXPECT(Copy::flow(flow, storage(four, output)) == Status::Success);
  EXPECT_EQ(original[0], U32(99));
  EXPECT_EQ(output[3], U32(4));
}

// The C entry reports the same outcome as the native facade. The partial write
// below is observable because this test owns the failing provider, but Copy
// does not certify that prefix as a second kind of successful observation.
PERIMORTEM_UNIT_TEST(TtxFlow, c_copy_result) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {
    .provides = PROVIDES_FRAGMENT, .values = {1, 2, 3, 4}};
  Reader reader{four};

  Flow flow;
  ASSERT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Success);

  U32 output[4] = {};
  const auto outcome =
      ttx_copy(flow.get_abi(), storage(four, output).get_abi());
  EXPECT_EQ(outcome.status, TTX_DATA_SUCCESS);
  EXPECT_EQ(output[3], U32(4));

  writer.failure = 1;
  writer.fail_at = 3;
  writer.reads = 0;
  output[2] = 99;
  const auto failure =
      ttx_copy(flow.get_abi(), storage(four, output).get_abi());
  EXPECT_EQ(failure.status, TTX_DATA_IO_ERROR);
  EXPECT_EQ(output[2], U32(99));
}
