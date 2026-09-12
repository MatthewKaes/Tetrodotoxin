// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_test.hpp"
#include "validation/unit_tests/ttx/data/form/preparation.hpp"

#include "ttx/data/protocol/direct.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;

static Validation::Harness TtxDirect = {.name = "TTX::Data::Protocol::Direct"_view};
static constexpr auto u32 = Schema::primitive(Schema::Value::U32);

// Data can use a Direct pointer without importing Semantic or any UUID type.
// The provider recovers its own state. The returned pointer is the explicitly
// public C representation, whose storage is supplied by this fixture owner.
PERIMORTEM_UNIT_TEST(TtxDirect, direct_without_bind) {
  Validation::DataTests::Preparation prepare;
  struct Source {
    const Representation& representation;
    U32 value;
  } source{prepare(u32), 42};
  const Protocol::Direct::Access::Operations operations = {
    [](const void* state) -> const Representation* {
      return &static_cast<const Source*>(state)->representation;
    },
    [](const void* state) -> const void* {
      return &static_cast<const Source*>(state)->value;
    },
  };

  Protocol::Direct::Access access(&source, operations);
  EXPECT(access.get_representation().compatible(prepare(u32)));
  EXPECT_EQ(*static_cast<const U32*>(access.read_ptr()), U32(42));
}

