// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/fixtures.hpp"

#include <stddef.h>

using namespace Validation::FlowTests;

// This provider deliberately cannot lend a record or populate a block. It
// computes U16, R64 and U32 observations separately, at coordinates describing
// a padded C record. Copy realizes that record in the target Storage. Swizzle
// then uses the same Flow with another output schema and another Storage.
PERIMORTEM_UNIT_TEST(TtxFlow, heterogeneous_outputs) {
  Preparation prepare;

  Module module("libnamed_provider.so", "named_provider_open");
  ASSERT(module.is_set());

  {
    Module::Heterogeneous state = {.seed = 7};
    struct Record {
      U16 tag;
      R64 energy;
      U32 frame;
    } output = {};

    struct Reordered {
      U32 frame;
      U16 tag;
      R64 energy;
    } projected = {};

    const auto tag = Schema::primitive(Schema::Value::U16);
    const auto energy = Schema::primitive(Schema::Value::R64);
    const Schema::Position fields[] = {
      {tag, offsetof(Record, tag)},
      {energy, offsetof(Record, energy)},
      {integer, offsetof(Record, frame)}};
    const auto input =
        prepare(Schema::composite({fields, 3}, sizeof(Record), alignof(Record)));

    const Schema::Position reordered[] = {
      {integer, offsetof(Reordered, frame)},
      {tag, offsetof(Reordered, tag)},
      {energy, offsetof(Reordered, energy)}};
    const auto target = prepare(
        Schema::composite({reordered, 3}, sizeof(Reordered), alignof(Reordered)));
    Reader reader{input};

    Flow flow;
    ASSERT(
        flow.connect(reader.query(), module.writer(state)) ==
        Flow::Status::Success);

    ASSERT(Copy::flow(flow, storage(input, output)) == Status::Success);
    EXPECT_EQ(output.tag, U16(8));
    EXPECT_EQ(output.energy, R64(1.75));
    EXPECT_EQ(output.frame, U32(700));

    const auto resolve = [](Count position) -> Count {
      if (position == offsetof(Reordered, frame)) {
        return offsetof(Record, frame);
      }

      if (position == offsetof(Reordered, tag)) {
        return offsetof(Record, tag);
      }

      return offsetof(Record, energy);
    };

    Swizzle::Mapping::create(input, target, resolve)
        .visit(
            [&](auto mapping) {
              ASSERT(
                  Swizzle::flow(flow, mapping, storage(target, projected)) ==
                  Status::Success);
            },
            [&](Status) { EXPECT(false); });

    EXPECT_EQ(projected.frame, U32(700));
    EXPECT_EQ(projected.tag, U16(8));
    EXPECT_EQ(projected.energy, R64(1.75));

    EXPECT_EQ(state.reads, Count(6));
    EXPECT_EQ(state.descriptions, Count(1));
  }
}
