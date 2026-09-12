// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_test.hpp"
#include "validation/unit_tests/ttx/data/form/preparation.hpp"

#include "ttx/data/protocol/fragment.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;

static Validation::Harness TtxFragment = {.name = "TTX::Data::Protocol::Fragment"_view};
static constexpr auto u32 = Schema::primitive(Schema::Value::U32);

// The C getter fills its output during the call. Native users receive a typed
// Result and need no reply receiver, pending state or provider buffer lifetime.
PERIMORTEM_UNIT_TEST(TtxFragment, typed_fragment_read) {
  Validation::DataTests::Preparation prepare;
  const auto& representation = prepare(u32);
  const Protocol::Fragment::Access::Operations operations = {
    .representation = [](const void* state) -> const Representation* {
      return static_cast<const Representation*>(state);
    },
    .get_u32 = [](const void*, Count coordinate,
                  U32* result) -> ttx_data_status {
      if (coordinate) {
        return TTX_DATA_BOUNDS;
      }

      *result = 42;
      return TTX_DATA_SUCCESS;
    },
  };

  Protocol::Fragment::Access access(&representation, operations);
  access.get_u32(0).visit(
      [&](U32 value) { EXPECT_EQ(value, U32(42)); },
      [&](Status) { EXPECT(false); });

  access.get_u32(4).visit(
      [&](U32) { EXPECT(false); },
      [&](Status status) { EXPECT(status == Status::Bounds); });
}

