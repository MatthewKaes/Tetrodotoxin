// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/memory/allocator/arena.hpp"

#include "validation/unit_test.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Validation;

static Harness Arena = {
  .name = "Memory::Allocator::Arena"_view,
};

class FactoryValue {
 public:
  FactoryValue(const FactoryValue&) = delete;
  FactoryValue(FactoryValue&&) = delete;

  static auto create(Allocator::Arena& arena, U32 value, Bool selected)
      -> FactoryValue& {
    return arena.construct_from<FactoryValue>(
        [=]() { return FactoryValue(value, selected); });
  }

  constexpr auto get_value() const -> U32 { return value; }
  constexpr auto is_selected() const -> Bool { return selected; }

 private:
  constexpr FactoryValue(U32 value, Bool selected)
      : value(value), selected(selected) {}

  U32 value;
  Bool selected;
};

static_assert(!__is_constructible(FactoryValue, U32, Bool));
static_assert(!__is_constructible(FactoryValue, const FactoryValue&));
static_assert(!__is_constructible(FactoryValue, FactoryValue&&));

PERIMORTEM_UNIT_TEST(Arena, reserve) {
  class RequiredValue {
   public:
    constexpr RequiredValue(U32 value) : value(value) {}

    constexpr auto get_value() const -> U32 { return value; }

   private:
    U32 value;
  };

  Allocator::Arena arena;
  U32& scalar = arena.reserve<U32>();
  auto values = arena.reserve<RequiredValue>(2);
  auto* value_data = values.get_data();

  scalar = 7;
  value_data[0] = RequiredValue(1);
  value_data[1] = RequiredValue(2);

  EXPECT_EQ(scalar, U32(7));
  EXPECT_EQ(values.get_size(), Count(2));
  EXPECT_EQ(value_data[0].get_value(), U32(1));
  EXPECT_EQ(value_data[1].get_value(), U32(2));
}

PERIMORTEM_UNIT_TEST(Arena, owner_factory) {
  Allocator::Arena arena;

  FactoryValue& value = FactoryValue::create(arena, 42, True);

  EXPECT_EQ(value.get_value(), U32(42));
  EXPECT(value.is_selected());
}

PERIMORTEM_UNIT_TEST(Arena, move_cursor) {
  Allocator::Arena source;
  auto* first = source.allocate(sizeof(U64)).get_data();
  *Data::cast<U64>(first) = 42;

  Allocator::Arena moved(static_cast<Allocator::Arena&&>(source));
  auto* next = moved.allocate(sizeof(U64)).get_data();
  EXPECT(next == first + sizeof(U64));
  EXPECT_EQ(*Data::cast<U64>(first), U64(42));
}
