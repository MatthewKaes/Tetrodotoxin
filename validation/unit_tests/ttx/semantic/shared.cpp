// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/semantic/fixtures.hpp"

using namespace Validation::FlowTests;

// Acquisition returns a ready lifetime. Flow retains it across calls, so Copy
// never reacquires it and closing releases the agreement exactly once.
PERIMORTEM_UNIT_TEST(TtxFlow, shared_lifetime) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {.provides = PROVIDES_SHARED, .values = {1, 2, 3, 4}};
  Validation::FlowTests::Reader reader{four};

  Flow flow;
  ASSERT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Success);

  U32 a[4] = {}, b[4] = {};
  EXPECT(Copy::flow(flow, storage(four, a)) == Status::Success);
  EXPECT(Copy::flow(flow, storage(four, b)) == Status::Success);

  EXPECT_EQ(writer.acquires, Count(1));
  EXPECT_EQ(writer.releases, Count(0));
  EXPECT_EQ(b[3], U32(4));

  flow.close();
  flow.close();
  EXPECT_EQ(writer.releases, Count(1));
}

// A selected Shared provider can fail acquisition. The returned failure ends
// that attempt instead of silently negotiating Block afterward.
PERIMORTEM_UNIT_TEST(TtxFlow, shared_failure) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {
    .provides = PROVIDES_SHARED | PROVIDES_BLOCK, .failure = 1};
  Validation::FlowTests::Reader reader{four};

  Flow flow;
  EXPECT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::IoError);
  EXPECT_EQ(writer.binds[2], Count(0));
  EXPECT_EQ(writer.releases, Count(0));
}

// Release still invokes owner code even though acquisition and operations are
// synchronous. That hook must see the old Flow unpublished before reconnecting.
struct OnRelease {
  Flow& flow;
  Validation::FlowTests::Reader& reader;
  Query next;
  Bool was_closed = false;
  Flow::Status result = Flow::Status::Rejected;

  auto run() -> void {
    was_closed = flow.get_protocol() == Protocol::None;
    result = flow.connect(reader.query(), next);
  }
};

PERIMORTEM_UNIT_TEST(TtxFlow, release_reentrancy) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State first = {.provides = PROVIDES_SHARED};
  Module::State second = {.provides = PROVIDES_DIRECT, .values = {1, 2, 3, 4}};
  Validation::FlowTests::Reader reader{four};
  Flow flow;

  OnRelease observer{flow, reader, module.writer(second)};
  first.observer = &observer;
  first.released = [](void* object) { static_cast<OnRelease*>(object)->run(); };

  ASSERT(
      flow.connect(reader.query(), module.writer(first)) ==
      Flow::Status::Success);

  flow.close();
  EXPECT(observer.was_closed);
  EXPECT(observer.result == Flow::Status::Success);

  U32 output[4] = {};
  EXPECT(Copy::flow(flow, storage(four, output)) == Status::Success);
  EXPECT_EQ(output[3], U32(4));
}
