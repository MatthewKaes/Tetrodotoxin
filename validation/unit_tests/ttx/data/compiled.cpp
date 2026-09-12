// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/form/compiled.hpp"

#include "validation/unit_test.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;

static Validation::Harness TtxCompiled = {
  .name = "TTX::Data::Form::Compiled"_view};
static constexpr auto integer = Schema::primitive(Schema::Value::U32);
static constexpr auto billion =
    Schema::range(integer, 1000000000, 4, 4000000000, 4);
static constexpr auto& prepared = Compiled<billion>::get_representation();
static_assert(prepared.count == 1000000000);
static_assert(prepared.extent == 4000000000);
static_assert(
    prepared.data.sequence.elements[0].representation->data.value.type ==
    TTX_SCHEMA_U32);

static constexpr auto real = Schema::primitive(Schema::Value::R32);
static constexpr auto fields = [] {
  Static::Vector<Schema::Position, 512> result;
  for (Count i = 0; i < result.get_size(); ++i) {
    result[i] = {i % 2 ? integer : real, i * 4};
  }

  return result;
}();
static constexpr auto record = Schema::composite(fields.get_view(), 2048, 4);
static constexpr auto& compiled_record = Compiled<record>::get_representation();
static_assert(compiled_record.count == 512);
static_assert(compiled_record.data.sequence.elements[511].offset == 2044);
static_assert(
    compiled_record.data.sequence.elements[510]
        .representation->data.value.type == TTX_SCHEMA_R32);

static constexpr auto empty = Schema::composite({}, 0);
static constexpr auto callable = Schema::mapping(record, empty);
static constexpr auto& compiled_callable =
    Compiled<callable>::get_representation();
static_assert(
    compiled_callable.extent == 8 && compiled_callable.alignment == 8);
static_assert(compiled_callable.data.mapping.input->count == 512);
static_assert(
    compiled_callable.data.mapping.output->kind == TTX_REPRESENTATION_EMPTY);

static constexpr Schema::Position padded_fields[] = {{real, 0}, {integer, 8}};
static constexpr auto padded = Schema::composite({padded_fields, 2}, 16, 4);
static constexpr auto records = Schema::range(padded, 4, 16, 64, 4);
static constexpr auto& compiled_groups =
    Compiled<records>::get_representation();
static_assert(compiled_groups.count == 8);
static_assert(compiled_groups.get_elements().get_size() == 1);
static_assert(
    compiled_groups.data.sequence.elements[0].representation->count == 2);

// Distinct Composite sources emit into one primitive inventory. Their authored
// nesting leaves intervals rather than intermediate Representation owners.
// Constant compilation must retain the same ordering and boundary facts as
// runtime preparation while its temporary containers grow.
static constexpr auto many_groups = [] {
  Static::Vector<Schema, 16> result;
  for (Count i = 0; i < result.get_size(); ++i) {
    result[i] = padded;
  }

  return result;
}();
static constexpr auto group_positions = [] {
  Static::Vector<Schema::Position, 16> result;
  for (Count i = 0; i < result.get_size(); ++i) {
    result[i] = {many_groups[i], i * 16};
  }

  return result;
}();
static constexpr auto group_record =
    Schema::composite(group_positions.get_view(), 256, 4);
static constexpr auto& compiled_many =
    Compiled<group_record>::get_representation();
static_assert(compiled_many.count == 32);
static_assert(compiled_many.get_elements().get_size() == 32);
static_assert(
    compiled_many.data.sequence.elements[31].representation->data.value.type ==
    TTX_SCHEMA_U32);

static constexpr auto pair = Schema::range(integer, 2, 4, 8, 4);
static constexpr auto factored =
    Schema::range(pair, 500000000, 8, 4000000000, 4);
static constexpr auto& compiled_factored =
    Compiled<factored>::get_representation();
static_assert(compiled_factored.data.sequence.elements[0].count == 1000000000);
static_assert(compiled_factored.data.sequence.elements[0].stride == 4);
static_assert(
    compiled_factored.data.sequence.elements[0].representation->kind ==
    TTX_SCHEMA_VALUE);

static constexpr auto units = Schema::range(empty, 1000000000, 8, 0, 8);
static constexpr auto& compiled_units = Compiled<units>::get_representation();
static_assert(compiled_units.kind == TTX_REPRESENTATION_EMPTY);
static_assert(compiled_units.count == 0 && compiled_units.extent == 0);

// The static assertions require actual constant evaluation, not a runtime
// initializer of a constexpr input. The same publication then serves ordinary
// runtime lookup through the C implementation used by foreign consumers.
PERIMORTEM_UNIT_TEST(TtxCompiled, constant_range) {
  prepared.at(999999999).visit(
      [&](Representation::Position position) {
        EXPECT_EQ(position.offset, Count(3999999996));
      },
      [&](Status) { EXPECT(false); });
}

// Static and runtime preparation share admission and normalization, but have
// independent output owners. Agreement must therefore succeed by their facts,
// with neither source addresses nor shared publication pointers required.
PERIMORTEM_UNIT_TEST(TtxCompiled, runtime_agreement) {
  Perimortem::Memory::Allocator::Arena arena;
  Representation::compile(callable, arena)
      .visit(
          [&](const Representation& runtime) {
            EXPECT(&runtime != &compiled_callable);
            EXPECT(runtime.compatible(compiled_callable));
            EXPECT(compiled_callable.compatible(runtime));
          },
          [&](Status) { EXPECT(false); });
}

// Transfer indexes primitives directly. The fourth primitive belongs to the
// second record, and its byte offset still includes that record's padding.
// Boundary agreement remains separate from this physical lookup.
PERIMORTEM_UNIT_TEST(TtxCompiled, grouped_coordinates) {
  compiled_groups.at(3).visit(
      [&](Representation::Position position) {
        EXPECT_EQ(position.offset, Count(24));
        EXPECT_EQ(position.representation->data.value.type, TTX_SCHEMA_U32);
      },
      [&](Status) { EXPECT(false); });

  EXPECT(prepared.compatible(compiled_factored));
}
