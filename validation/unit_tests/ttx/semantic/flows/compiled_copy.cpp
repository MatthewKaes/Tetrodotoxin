// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/semantic/fixtures.hpp"
#include "validation/unit_tests/ttx/semantic/measurement.hpp"

#include "ttx/data/form/compiled.hpp"

using namespace Validation::FlowTests;
using Ttx::Data::Form::Compiled;

// The reader publishes metadata prepared during C++ translation. The loaded
// C provider prepared its own metadata at module opening. Their agreement
// therefore crosses both a language boundary and a preparation strategy.
PERIMORTEM_UNIT_TEST(TtxFlow, static_form_flow) {
  constexpr auto& representation = Compiled<four_schema>::get_representation();
  static_assert(representation.get_extent() == sizeof(U32) * 4);
  Module module;
  ASSERT(module.is_set());

  Module::State writer = {
    .provides = PROVIDES_DIRECT, .values = {10, 20, 30, 40}};
  U32 output[4] = {};
  const auto target = storage(representation, output);
  Flow flow;

  Measurement measurement;
  const auto agreement =
      flow.connect(Flow::reader(representation), module.writer(writer));
  ASSERT(agreement == Flow::Status::Success);
  const auto status = Copy::flow(flow, target);
  measurement.stop();

  EXPECT(status == Status::Success);
  EXPECT_EQ(output[0], U32(10));
  EXPECT_EQ(output[3], U32(40));
  EXPECT_EQ(measurement.get_allocations(), Count(0));
}
