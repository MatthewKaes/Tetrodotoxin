// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/memory/dynamic/vector.hpp"

#include "validation/unit_test.hpp"

#include "perimortem/core/null_terminated.hpp"

using namespace Perimortem::Memory;
using namespace Validation;

static Harness DynamicVector = {
  .name = "Dynamic::Vector"_view,
};

template <typename type>
static constexpr bool supports_forgetful_resize =
    requires(Dynamic::Vector<type>& values) { values.forgetful_resize(1); };

// Ordinary resize can support owning values, while forgetful resize skips
// their lifecycle entirely. Make sure the API rejects types that aren't
// trivially constructable and destructable, including a deleted constructor,
// without excluding the whole Vector type.
class ConstructedValue {
 public:
  ConstructedValue() : value(42) {}
  U32 value;
};

class DestructedValue {
 public:
  DestructedValue() = default;
  ~DestructedValue() {}
};

class UnconstructibleValue {
 public:
  UnconstructibleValue() = delete;
};

static_assert(supports_forgetful_resize<U32>);
static_assert(!supports_forgetful_resize<ConstructedValue>);
static_assert(!supports_forgetful_resize<DestructedValue>);
static_assert(!supports_forgetful_resize<UnconstructibleValue>);

class ResizeValue {
 public:
  inline static Count instances = 0;

  ResizeValue() { ++instances; }
  ResizeValue(const ResizeValue& rhs) : value(rhs.value) { ++instances; }
  ~ResizeValue() { --instances; }

  U32 value = 42;
};

// Shrinking ends the removed lifetimes. Growing within capacity must begin
// them again just as growth into a new allocation does, preserving survivors.
PERIMORTEM_UNIT_TEST(DynamicVector, resize_lifetimes) {
  {
    Dynamic::Vector<ResizeValue> values;
    values.resize(3);
    EXPECT_EQ(ResizeValue::instances, Count(3));
    values[0].value = 7;

    values.resize(1);
    EXPECT_EQ(ResizeValue::instances, Count(1));
    values.resize(3);
    EXPECT_EQ(ResizeValue::instances, Count(3));
    EXPECT_EQ(values[1].value, U32(42));

    values.resize(100);
    EXPECT_EQ(ResizeValue::instances, Count(100));
    EXPECT_EQ(values[0].value, U32(7));
  }

  EXPECT_EQ(ResizeValue::instances, Count(0));
}

PERIMORTEM_UNIT_TEST(DynamicVector, forgetful_scalars) {
  Dynamic::Vector<U32> values;
  values.forgetful_resize(100);
  EXPECT_EQ(values.get_size(), Count(100));
  EXPECT(values.get_capacity() >= 100);

  values[99] = 42;
  EXPECT_EQ(values[99], U32(42));
}

PERIMORTEM_UNIT_TEST(DynamicVector, remove) {
  Dynamic::Vector<S32> values;
  values.insert(1);
  values.insert(2);
  values.insert(3);
  values.insert(4);

  EXPECT(values.remove(1));
  EXPECT_EQ(values.get_size(), 3);
  EXPECT_EQ(values[0], 1);
  EXPECT_EQ(values[1], 4);
  EXPECT_EQ(values[2], 3);
  EXPECT(!values.contains(2));
  EXPECT(!values.remove(3));
}

PERIMORTEM_UNIT_TEST(DynamicVector, remove_stable) {
  Dynamic::Vector<S32> values;
  values.insert(1);
  values.insert(2);
  values.insert(3);
  values.insert(4);

  EXPECT(values.remove_stable(1));
  EXPECT_EQ(values.get_size(), 3);
  EXPECT_EQ(values[0], 1);
  EXPECT_EQ(values[1], 3);
  EXPECT_EQ(values[2], 4);
  EXPECT(!values.contains(2));
  EXPECT(!values.remove_stable(3));
}
