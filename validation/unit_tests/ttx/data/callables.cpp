// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/fixtures.hpp"
#include "validation/unit_tests/ttx/data/measurement.hpp"
#include "validation/unit_tests/ttx/data/module.hpp"

using namespace Validation::FlowTests;

// This free function has no arguments or results, but the function pointer
// is still a value that must be transferred. Mapping's carrier extent is
// independent of its empty input and output Schemas. There is no receiver for
// this free function, unlike the bound operation in the next example.
PERIMORTEM_UNIT_TEST(TtxFlow, empty_callable_retains_carrier) {
  // Unary plus converts this captureless lambda to a function pointer, so the
  // carrier is the pointer itself rather than a C++ closure object.
  auto function = +[]() {};
  void (*output)() = nullptr;
  const auto arguments = Schema::composite({}, 0, 0);
  const auto callable = Schema::mapping(
      arguments, arguments, sizeof(function), alignof(decltype(function)));
  ASSERT(
      extract(view(callable, function), access(callable, output)) ==
      Status::Success);
  EXPECT(output == function);
  output();
}

// The C provider's table stores next before self. The host wants self before
// next, so extraction maps their names into a different physical arrangement.
// The callable Schema explicitly includes the opaque receiver pointer.
// Once populated, the table itself is the connection: a million calls use its
// function pointer directly while the loaded module and receiver remain alive.
// Removing self from the requested signature must reject a later transfer.
PERIMORTEM_UNIT_TEST(TtxFlow, operation_table_is_the_connection) {
  Module provider;
  ASSERT(provider.symbol);
  Module::State state = {};
  Module::Operations table = {};
  const auto function = Schema::mapping(
      pointer, integer, sizeof(table.next), alignof(decltype(table.next)));
  const Schema::Position fields[] = {
    {pointer, offsetof(Module::Operations, self), 0},
    {function, offsetof(Module::Operations, next), 1}};
  const auto shape =
      Schema::group({fields, 2}, sizeof(table), 2, alignof(Module::Operations));
  const auto source = provider.operations(state);
  const Flow::Assignment assignments[] = {
    {named(source, "self"_view), Index(0)},
    {named(source, "next"_view), Index(1)}};
  // A second query must not rebind the receiver of the first query. Both
  // answers remain borrowed from their own state for later extraction.
  Module::State other_state = {.next = 4000000};
  const auto other_source = provider.operations(other_state);
  Measurement measurement;
  const auto status = extract(source, access(shape, table), {assignments, 2});
  measurement.stop();
  ASSERT(status == Status::Success);
  ASSERT(table.next);
  EXPECT_EQ(measurement.get_allocations(), Count(0));

  // Invocation uses only the populated table. No Flow object participates.
  const Count queries = state.queries;
  Measurement invocation;
  U32 last = 0;
  for (Count i = 0; i < 1000000; ++i) {
    last = table.next(table.self);
  }
  invocation.stop();
  EXPECT_EQ(last, U32(999999));
  EXPECT_EQ(state.queries, queries);
  EXPECT_EQ(state.next, U32(1000000));
  EXPECT_EQ(other_state.next, U32(4000000));
  EXPECT_EQ(invocation.get_allocations(), Count(0));

  Module::Operations other_table = {};
  ASSERT(
      extract(other_source, access(shape, other_table), {assignments, 2}) ==
      Status::Success);
  EXPECT_EQ(other_table.next(other_table.self), U32(4000000));
  EXPECT_EQ(state.next, U32(1000000));

  const auto no_arguments = Schema::composite({}, 0, 0);
  const auto wrong_function = Schema::mapping(
      no_arguments, integer, sizeof(table.next), alignof(decltype(table.next)));
  const Schema::Position bad_fields[] = {
    {pointer, 0, 0}, {wrong_function, offsetof(Module::Operations, next), 1}};
  const auto bad = Schema::group(
      {bad_fields, 2}, sizeof(table), 2, alignof(Module::Operations));
  EXPECT(
      extract(source, access(bad, table), {assignments, 2}) ==
      Status::Incompatible);
}
