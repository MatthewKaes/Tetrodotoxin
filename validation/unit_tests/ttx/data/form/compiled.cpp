// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/form/compiled.hpp"

#include "validation/unit_tests/ttx/data/form/preparation.hpp"

#include "perimortem/core/static/vector.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;

static Validation::Harness TtxCompiled = {
  .name = "TTX::Data::Form::Compiled"_view};
static constexpr auto integer = Schema::primitive(Schema::Value::U32);
static constexpr auto real = Schema::primitive(Schema::Value::R32);
static constexpr auto billion =
    Schema::range(integer, 1000000000, 4, 4000000000, 4);
static constexpr auto& prepared = Compiled<billion>::get_representation();

// The count changes field width, not the size of the temporary inventory.
// Even one billion values need only a header and one repeated element block.
static_assert(prepared.get_extent() == 4000000000);
static_assert(prepared.get_depth() == 4);
static_assert(prepared.get_bytes().get_size() == 32);

static constexpr auto fields = [] {
  Static::Vector<Schema::Position, 512> result;
  for (Count i = 0; i < result.get_size(); ++i) {
    result[i] = Schema::Position(i % 2 ? integer : real, i * 4);
  }

  return result;
}();
static constexpr auto record = Schema::composite(fields.get_view(), 2048, 4);
static constexpr auto& compiled_record = Compiled<record>::get_representation();
static_assert(compiled_record.get_extent() == 2048);
static_assert(compiled_record.get_bytes().get_size() == 513 * 8);

static constexpr auto empty =
    Schema::composite(View::Vector<Schema::Position>(), 0);
static constexpr auto& compiled_empty = Compiled<empty>::get_representation();
static_assert(compiled_empty.get_extent() == 0);
static_assert(compiled_empty.get_bytes().get_size() == 4);

PERIMORTEM_UNIT_TEST(TtxCompiled, runtime_agreement) {
  Validation::DataTests::Preparation prepare;

  EXPECT(prepared.compatible(prepare(billion)));
  EXPECT(compiled_record.compatible(prepare(record)));
  EXPECT(compiled_empty.compatible(prepare(empty)));
}

PERIMORTEM_UNIT_TEST(TtxCompiled, primitive_bytes) {
  // These bytes are written independently of Encoding's field helpers. They
  // establish the first byte depth, header geometry and primitive descriptor.
  // Header: F=1, C=1, alignment=4, extent=4. Element: A=1, O=0, S=4, U32=3.
  const U8 expected[] = {0x11, 0x40, 0x40, 0x00, 0x03, 0x04, 0x00, 0x01};
  const auto& representation = Compiled<integer>::get_representation();

  ASSERT_EQ(representation.get_bytes().get_size(), sizeof(expected));
  EXPECT(Data::compare(representation.data, expected, sizeof(expected)));
}

PERIMORTEM_UNIT_TEST(TtxCompiled, repeated_publication) {
  // Preparing owns temporary records. Two writes after preparation must not
  // depend on the Schema still being present, or compile it a second time.
  Compiler compiler;
  {
    auto source = integer;
    ASSERT(compiler.compile(source) == Status::Success);
    source.data.value.type = TTX_SCHEMA_R64;
  }

  U8 first[8];
  U8 second[8];
  ASSERT_EQ(compiler.get_size(), Count(8));
  ASSERT(compiler.write(Access::Bytes(first)) == Status::Success);
  ASSERT(compiler.write(Access::Bytes(second)) == Status::Success);

  const auto& reference = Compiled<integer>::get_representation();
  EXPECT(Representation(first, sizeof(first)).compatible(reference));
  EXPECT(Data::compare(first, second, sizeof(first)));
}

PERIMORTEM_UNIT_TEST(TtxCompiled, short_publication) {
  Compiler compiler;
  ASSERT(compiler.compile(integer) == Status::Success);

  U8 bytes[7] = {11, 22, 33, 44, 55, 66, 77};
  const U8 expected[] = {11, 22, 33, 44, 55, 66, 77};
  EXPECT(compiler.write(Access::Bytes(bytes)) == Status::Bounds);
  EXPECT(Data::compare(bytes, expected, sizeof(bytes)));
}
