// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/fixtures.hpp"

using namespace Validation::FlowTests;

// C supplies a position policy, not a result buffer. The same admitted policy
// reads four values in the Flow's schema and fills two target values. Direct
// reads public bytes, while Fragment asks the C writer for two typed values.
PERIMORTEM_UNIT_TEST(TtxFlow, swizzle_protocols) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  const auto mapping = module.selection();
  const auto pair = prepare(Schema::range(integer, 2, 4, 8, 4));
  const U8 protocols[] = {PROVIDES_DIRECT, PROVIDES_SHARED, PROVIDES_FRAGMENT};
  for (U8 protocol : protocols) {
    Module::State writer = {.provides = protocol, .values = {10, 20, 30, 40}};
    Reader reader{four};

    Flow flow;
    ASSERT(
        flow.connect(reader.query(), module.writer(writer)) ==
        Flow::Status::Success);

    U32 a[2] = {}, b[2] = {};
    EXPECT(Swizzle::flow(flow, mapping, storage(pair, a)) == Status::Success);
    EXPECT_EQ(a[0], U32(40));
    EXPECT_EQ(a[1], U32(10));

    EXPECT(module.select(flow, storage(pair, b)) == Status::Success);
    EXPECT_EQ(b[0], U32(40));
    EXPECT_EQ(b[1], U32(10));

    EXPECT_EQ(
        writer.reads, protocol == PROVIDES_FRAGMENT ? Count(4) : Count(0));
  }
}

// Output positions are supplied exactly once by their schema. A repeated
// source coordinate is valid, while a missing source coordinate or a different
// primitive type is rejected when the mapping is admitted, before operation.
PERIMORTEM_UNIT_TEST(TtxFlow, mapping_admission) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  const auto pair = prepare(Schema::range(integer, 2, 4, 8, 4));
  const auto repeated = [](const void*, Count) -> Count { return 0; };
  Swizzle::Mapping::create({&four, &pair, nullptr, repeated})
      .visit([&](auto) {}, [&](Status) { EXPECT(false); });

  Swizzle::Mapping::create(
      {&four, &pair, nullptr, [](const void*, Count) -> Count { return 16; }})
      .visit(
          [&](auto) { EXPECT(false); },
          [&](Status status) { EXPECT(status == Status::Bounds); });

  const auto wrong = prepare(Schema::range(real, 2, 4, 8, 4));
  Swizzle::Mapping::create({&four, &wrong, nullptr, repeated})
      .visit(
          [&](auto) { EXPECT(false); },
          [&](Status status) { EXPECT(status == Status::Incompatible); });
}

// The mapping can be reused with another target Storage, but not with a target
// whose schema differs from its declared output. Failure happens before the
// first Fragment observation, preserving both the writer and the destination.
PERIMORTEM_UNIT_TEST(TtxFlow, swizzle_wrong_target) {
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

  R32 output[2] = {-1, -1};
  const auto wrong = prepare(Schema::range(real, 2, 4, 8, 4));
  EXPECT(
      Swizzle::flow(flow, module.selection(), storage(wrong, output)) ==
      Status::Incompatible);
  EXPECT(module.select(flow, storage(wrong, output)) == Status::Incompatible);

  EXPECT_EQ(writer.reads, Count(0));
  EXPECT_EQ(output[0], R32(-1));
}

// Direct gives access to bytes, not an implicit snapshot for an overlapping
// permutation. The operation declines overlapping storage before changing
// either source value. No other protocol is negotiated as a fallback.
PERIMORTEM_UNIT_TEST(TtxFlow, swizzle_overlap) {
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

  const auto mapping = module.selection();
  EXPECT(
      Swizzle::flow(
          flow, mapping, storage(mapping.get_output(), writer.values)) ==
      Status::Unsupported);
  EXPECT_EQ(writer.values[0], U32(10));
  EXPECT_EQ(writer.values[1], U32(20));

  EXPECT_EQ(writer.binds[3], Count(0));
}

// Repetition and slicing are position policies over the same input. Neither
// changes Flow's selected protocol. The duplicate observes red twice, while
// the slice observes green followed by blue in a separately owned Storage.
PERIMORTEM_UNIT_TEST(TtxFlow, swizzle_and_slice) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  const auto pair = prepare(Schema::range(integer, 2, 4, 8, 4));
  const auto repeat = [](Count) -> Count { return 0; };
  const auto slice = [](Count output) -> Count { return output + 4; };
  const auto admitted = [](auto result) {
    return result.visit(
        [](auto mapping) { return mapping; },
        [](Status) -> Swizzle::Mapping {
          Diagnostics::Log::fatal("Invalid fixture mapping."_view);
        });
  };

  const auto red = admitted(Swizzle::Mapping::create(four, pair, repeat));
  const auto middle = admitted(Swizzle::Mapping::create(four, pair, slice));
  const U8 protocols[] = {PROVIDES_DIRECT, PROVIDES_SHARED, PROVIDES_FRAGMENT};
  for (U8 protocol : protocols) {
    Module::State writer = {.provides = protocol, .values = {10, 20, 30, 40}};
    Reader reader{four};

    Flow flow;
    ASSERT(
        flow.connect(reader.query(), module.writer(writer)) ==
        Flow::Status::Success);

    U32 output[2] = {};
    EXPECT(Swizzle::flow(flow, red, storage(pair, output)) == Status::Success);
    EXPECT_EQ(output[0], U32(10));
    EXPECT_EQ(output[1], U32(10));

    EXPECT(
        Swizzle::flow(flow, middle, storage(pair, output)) == Status::Success);
    EXPECT_EQ(output[0], U32(20));
    EXPECT_EQ(output[1], U32(30));
  }
}

// A projection reports its failure cause without certifying an output prefix.
// This provider fails its second observation, so the first write remains in
// the target. Keeping that evidence separate from the status shows why an
// owner needing rollback must supply that policy explicitly.
PERIMORTEM_UNIT_TEST(TtxFlow, swizzle_failure) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {
    .provides = PROVIDES_FRAGMENT,
    .failure = 1,
    .fail_at = 2,
    .values = {10, 20, 30, 40}};
  Reader reader{four};

  Flow flow;
  ASSERT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Success);

  const auto mapping = module.selection();
  U32 output[2] = {99, 99};
  EXPECT(
      Swizzle::flow(flow, mapping, storage(mapping.get_output(), output)) ==
      Status::IoError);
  EXPECT_EQ(output[0], U32(40));
  EXPECT_EQ(output[1], U32(99));
}

// Representation conversion belongs to the mapping operation. Direct starts
// with public native bytes, while Fragment returns a native U32. Both paths
// realize the same value in the requested big endian output representation.
PERIMORTEM_UNIT_TEST(TtxFlow, swizzle_byte_order) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  const auto big =
      prepare(Schema::primitive(Schema::Value::U32, Schema::ByteOrder::Big));
  const auto first = [](Count) -> Count { return 0; };
  Swizzle::Mapping::create(four, big, first)
      .visit(
          [&](auto mapping) {
            const U8 protocols[] = {PROVIDES_DIRECT, PROVIDES_FRAGMENT};
            for (U8 protocol : protocols) {
              Module::State writer = {
                .provides = protocol, .values = {0x12345678}};
              Reader reader{four};

              Flow flow;
              ASSERT(
                  flow.connect(reader.query(), module.writer(writer)) ==
                  Flow::Status::Success);

              alignas(U32) U8 output[4] = {};
              EXPECT(
                  Swizzle::flow(flow, mapping, storage(big, output)) ==
                  Status::Success);
              EXPECT_EQ(output[0], U8(0x12));
              EXPECT_EQ(output[1], U8(0x34));
              EXPECT_EQ(output[2], U8(0x56));
              EXPECT_EQ(output[3], U8(0x78));
            }
          },
          [&](Status) { EXPECT(false); });
}
