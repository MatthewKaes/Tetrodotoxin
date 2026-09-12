// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/fixtures.hpp"

using namespace Validation::FlowTests;

// The module entry is the prearranged C bootstrap. It supplies a writer for
// the Query record, while the host's reader permits only Direct or Shared.
// The agreed C representation grants the cast that imports its bind thunk.
// That imported Query then participates in ordinary protocol negotiation.
PERIMORTEM_UNIT_TEST(TtxFlow, bootstrap_bind) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  const auto pointer = Schema::primitive(Schema::Value::Pointer);
  const auto status = Schema::primitive(Schema::Value::U8);
  const auto word = Schema::primitive(Schema::Value::U64);
  const auto uuid = Schema::range(word, 2, 8, 16, 8);
  const Schema::Position args[] = {{pointer, 0}, {uuid, 8}, {pointer, 24}};
  const auto input = Schema::composite({args, 3}, 32, 8);
  const auto callable = Schema::mapping(input, status);

  const Schema::Position fields[] = {{pointer, 0}, {callable, 8}};
  const auto query_schema = prepare(Schema::composite({fields, 2}, 16, 8));
  Reader receiver{query_schema, PROVIDES_DIRECT | PROVIDES_SHARED};

  Flow bootstrap;
  ASSERT(
      bootstrap.connect(receiver.query(), module.bootstrap_writer()) ==
      Flow::Status::Success);
  EXPECT(bootstrap.get_protocol() == Protocol::Direct);

  const auto imported = module.import_query(bootstrap);
  Reader data{four};
  Flow flow;
  ASSERT(flow.connect(data.query(), imported) == Flow::Status::Success);

  U32 target[4] = {};
  ASSERT(Copy::flow(flow, storage(four, target)) == Status::Success);
  EXPECT_EQ(target[3], U32(4));
}
