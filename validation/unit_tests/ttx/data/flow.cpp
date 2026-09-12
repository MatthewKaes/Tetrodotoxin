// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/fixtures.hpp"
#include "validation/unit_tests/ttx/data/module.hpp"

using namespace Validation::FlowTests;

// The loaded C library supplies the selector, not the values. The sequence is
// deliberately visible: the host owns {10, 20, 30, 40} and its two output
// slots, then passes both queries and the host Flow entry to C. C submits
// assignments from position 3 to output 0 and position 0 to output 1. The host
// engine checks those pairs and joins the host's indexed read and write thunks.
// Thus 40 and 10 cross host access thunks, while the independent C policy
// chooses them. Generated data from C is covered separately in values.cpp and
// named.cpp.
PERIMORTEM_UNIT_TEST(TtxFlow, c_provider_calls_cpp_engine) {
  Module provider;
  ASSERT(provider.symbol);
  U32 source[] = {10, 20, 30, 40}, output[] = {0, 0};
  const auto four = Schema::range(integer, 4, 4, 16, 4, 4);
  const auto two = Schema::range(integer, 2, 4, 8, 2, 4);
  EXPECT(
      provider.select_edges(view(four, source), access(two, output)) ==
      Status::Success);
  EXPECT_EQ(output[0], U32(40));
  EXPECT_EQ(output[1], U32(10));
}

// C sends the same selector, but the destination now requires R32 and U16.
// Neither source U32 can satisfy it. The complete mapping is rejected before
// any transfer, leaving both differently sized sentinel fields unchanged.
PERIMORTEM_UNIT_TEST(TtxFlow, c_selection_rejects_incompatible_output) {
  Module provider;
  ASSERT(provider.symbol);
  U32 source[] = {10, 20, 30, 40};
  struct Output {
    R32 real;
    U16 small;
  } output = {-1, 77};
  const auto small = Schema::primitive(Value::U16);
  const Schema::Position fields[] = {
    {real, 0, 0}, {small, offsetof(Output, small), 1}};
  const auto target =
      Schema::composite({fields, 2}, sizeof(Output), 2, alignof(Output));
  const auto four = Schema::range(integer, 4, 4, 16, 4, 4);
  EXPECT(
      provider.select_edges(view(four, source), access(target, output)) ==
      Status::Incompatible);
  EXPECT_EQ(output.real, R32(-1));
  EXPECT_EQ(output.small, U16(77));
}
