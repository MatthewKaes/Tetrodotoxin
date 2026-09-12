// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_test.hpp"

#include "perimortem/core/null_terminated.hpp"

#include "perimortem/memory/allocator/arena.hpp"
#include "perimortem/memory/managed/map.hpp"

using namespace Perimortem::Memory;
using namespace Validation;

static Harness ManagedMap = {
  .name = "Managed::Map"_view,
};

PERIMORTEM_UNIT_TEST(ManagedMap, empty) {
  Allocator::Arena arena;
  Managed::Map<S32, S32> values(arena);

  EXPECT(values.is_empty());
  EXPECT_EQ(values.get_size(), Count(0));
  EXPECT(!values.find(4));
}

PERIMORTEM_UNIT_TEST(ManagedMap, find) {
  Allocator::Arena arena;
  Managed::Map<S32, S32> values(arena);

  values.insert(4, 5);

  auto found = values.find(4);
  ASSERT(found);
  EXPECT_EQ((*found).value, 5);
  EXPECT(!values.find(8));

  const auto& const_values = values;
  auto const_found = const_values.find(4);
  ASSERT(const_found);
  EXPECT_EQ((*const_found).value, 5);
  EXPECT(!const_values.find(8));
}

PERIMORTEM_UNIT_TEST(ManagedMap, simple_insert) {
  Allocator::Arena arena;
  Managed::Map<S32, S32> values(arena);

  values.insert(1, 2);
  values.insert(2, 3);
  values.insert(4, 5);

  EXPECT_EQ(values.get_size(), Count(3));
  EXPECT_EQ(values[1], 2);
  EXPECT_EQ(values[2], 3);
  EXPECT_EQ(values[4], 5);
}

PERIMORTEM_UNIT_TEST(ManagedMap, duplicate_keys) {
  Allocator::Arena arena;
  Managed::Map<S32, S32> values(arena);

  values.insert(1, 2);
  values.insert(1, 4);
  values.insert(2, 8);

  EXPECT_EQ(values.get_size(), Count(2));
  EXPECT_EQ(values[1], 4);
  EXPECT_EQ(values[2], 8);
}

PERIMORTEM_UNIT_TEST(ManagedMap, visit) {
  Allocator::Arena arena;
  Managed::Map<S32, S32> values(arena);

  values.insert(1, 2);

  S32 found = values.visit(
      1, [](const S32& selected) { return selected; },
      []() -> S32 { return -1; });
  S32 missing = values.visit(
      4, [](const S32& selected) { return selected; },
      []() -> S32 { return -1; });

  EXPECT_EQ(found, 2);
  EXPECT_EQ(missing, -1);
}

PERIMORTEM_UNIT_TEST(ManagedMap, text_keys) {
  Allocator::Arena arena;
  Managed::Map<Perimortem::Core::View::Bytes, S32> values(arena);

  values["Hello"_view] = 1;
  values["World"_view] = 2;
  values["Longer test string"_view] = 3;

  EXPECT(values.contains("Hello"_view));
  EXPECT_EQ(values["Hello"_view], 1);
  EXPECT_EQ(values["World"_view], 2);
  EXPECT_EQ(values["Longer test string"_view], 3);
}

PERIMORTEM_UNIT_TEST(ManagedMap, clear) {
  Allocator::Arena arena;
  Managed::Map<S32, S32> values(arena);

  values.insert(1, 2);
  values.insert(2, 3);
  values.clear();

  EXPECT(values.is_empty());
  EXPECT(!values.contains(1));
  values.insert(1, 4);
  EXPECT_EQ(values[1], 4);
}

PERIMORTEM_UNIT_TEST(ManagedMap, independent_copy) {
  Allocator::Arena arena;
  Managed::Map<S32, S32> source(arena);
  source.insert(1, 2);
  source.insert(3, 4);

  auto copy = source;
  copy.insert(1, 9);
  copy.insert(5, 6);
  EXPECT_EQ(source.get_size(), Count(2));
  EXPECT_EQ(source[1], 2);
  EXPECT(!source.contains(5));

  copy.clear();
  EXPECT(copy.is_empty());
  EXPECT_EQ(source.get_size(), Count(2));
  EXPECT(source.contains(1));
  EXPECT(source.contains(3));
}

PERIMORTEM_UNIT_TEST(ManagedMap, empty_copy) {
  Allocator::Arena arena;
  Managed::Map<S32, S32> source(arena);
  auto copy = source;
  copy.insert(1, 2);

  EXPECT(source.is_empty());
  EXPECT_EQ(copy.get_size(), Count(1));
}

PERIMORTEM_UNIT_TEST(ManagedMap, insert_stress_test) {
  Allocator::Arena arena;
  Managed::Map<S32, S32> values(arena);
  for (Count i = 0; i < 1000; i++) {
    values.insert(i, i + 2);
  }

  EXPECT_EQ(values.get_size(), Count(1000));
  for (Count i = 0; i < 1000; i++) {
    ASSERT_EQ(values[i], i + 2);
  }
}
