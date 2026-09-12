// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_test.hpp"
#include "validation/unit_tests/ttx/data/preparation.hpp"

#include "ttx/data/protocol/block.hpp"
#include "ttx/data/protocol/direct.hpp"
#include "ttx/data/protocol/fragment.hpp"
#include "ttx/data/protocol/shared.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;

static Validation::Harness TtxTransport = {.name = "TTX::Data::Protocols"_view};
static constexpr auto u32 = Schema::primitive(Schema::Value::U32);

// Data can use a Direct pointer without importing Semantic or any UUID type.
// The provider recovers its own state. The returned pointer is the explicitly
// public C representation, whose storage is supplied by this fixture owner.
PERIMORTEM_UNIT_TEST(TtxTransport, direct_without_bind) {
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

// The C getter fills its output during the call. Native users receive a typed
// Result and need no reply receiver, pending state or provider buffer lifetime.
PERIMORTEM_UNIT_TEST(TtxTransport, typed_fragment_read) {
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

PERIMORTEM_UNIT_TEST(TtxTransport, storage_check) {
  Validation::DataTests::Preparation prepare;
  const auto& representation = prepare(u32);
  U32 value = 0;
  EXPECT(
      Storage::create(
          representation, {reinterpret_cast<U8*>(&value), sizeof(value)})
          .visit(
              [](Storage storage) {
                return storage.get_bytes().get_size() == sizeof(U32);
              },
              [](Status) { return false; }));

  Storage::create(representation, {reinterpret_cast<U8*>(&value), 2})
      .visit(
          [&](Storage) { EXPECT(false); },
          [&](Status status) { EXPECT(status == Status::Bounds); });
}
