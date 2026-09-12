// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/core/option.hpp"

#include "validation/unit_test.hpp"

#include "perimortem/core/null_terminated.hpp"

#include "perimortem/abi/core/option.hpp"

using namespace Perimortem::Core;
using namespace Validation;

static Harness CoreOption = {
  .name = "Core::Option"_view,
};

// Presence is useful in conditions, but it cannot stand in for the selected
// integer. Otherwise assigning an unread Option can quietly store one instead
// of the value its caller meant to extract.
static_assert(!__is_convertible_to(Option<U64>, U64));
static_assert(!__is_convertible_to(Option<U64&>, U64));
static_assert(requires(Option<U64> value) { value ? true : false; });
static_assert(requires(Option<U64&> value) { value ? true : false; });
static_assert(Bool(Option<U64>(U64(0))) == True);
static_assert(Bool(Option<U64>()) == False);
static_assert([] {
  U64 value = 0;
  return Bool(Option<U64&>(value)) == True && Bool(Option<U64&>()) == False;
}());

class BorrowedBase {};
class BorrowedDerived final : public BorrowedBase {};

class StackValue {
 public:
  StackValue(S32 value, Count& destructions)
      : value(value), destructions(destructions) {}

  StackValue(const StackValue& source)
      : value(source.value), destructions(source.destructions) {}
  StackValue(StackValue&& source)
      : value(source.value), destructions(source.destructions) {
    source.moved = True;
  }

  ~StackValue() {
    if (!moved) {
      destructions++;
    }
  }

  auto increment() -> void { value++; }
  auto get() const -> S32 { return value; }

 private:
  S32 value;
  Bool moved = False;
  Count& destructions;
};

static auto create_stack_value(Count& destructions) -> Option<StackValue> {
  StackValue value(41, destructions);
  return Data::take(value);
}

PERIMORTEM_UNIT_TEST(CoreOption, abi_carrier) {
  auto absent = Perimortem::Abi::Core::Option<U64>::create();
  auto present = Perimortem::Abi::Core::Option<U64>::create(U64(42));

  EXPECT(!absent);
  EXPECT(present);
  EXPECT_EQ(*present, U64(42));
}

PERIMORTEM_UNIT_TEST(CoreOption, visits_none) {
  Option<const S32&> selected;

  Count branch = selected.visit(
      []() { return Count(1); }, [](const S32&) { return Count(2); });

  EXPECT_EQ(branch, 1);
}

PERIMORTEM_UNIT_TEST(CoreOption, visits_reference) {
  S32 value = 41;
  Option<S32&> selected(value);

  selected.visit([]() {}, [](S32& found) -> void { found++; });

  EXPECT_EQ(value, 42);
}

PERIMORTEM_UNIT_TEST(CoreOption, copies_borrow) {
  const S32 value = 42;
  Option<const S32&> first(value);
  Option<const S32&> second(first);

  S32 found = second.visit(
      []() { return S32(0); }, [](const S32& selected) { return selected; });

  EXPECT_EQ(found, value);
}

PERIMORTEM_UNIT_TEST(CoreOption, copies_value) {
  Option<S32> first(41);
  Option<S32> second(first);

  second.visit([]() {}, [](S32& selected) -> void { selected++; });

  S32 first_value = first.visit(
      []() { return S32(0); }, [](S32 selected) { return selected; });
  S32 second_value = second.visit(
      []() { return S32(0); }, [](S32 selected) { return selected; });

  EXPECT_EQ(first_value, 41);
  EXPECT_EQ(second_value, 42);
}

PERIMORTEM_UNIT_TEST(CoreOption, owns_stack_value) {
  Count destructions = 0;

  {
    Option<StackValue> selected = create_stack_value(destructions);
    Option<StackValue> moved(Data::take(selected));

    moved.visit([]() {}, [](StackValue& value) -> void { value.increment(); });

    const Option<StackValue>& observed = moved;
    S32 found = observed.visit(
        []() { return S32(0); },
        [](const StackValue& value) { return value.get(); });

    EXPECT_EQ(found, 42);
  }

  EXPECT_EQ(destructions, Count(1));
}

PERIMORTEM_UNIT_TEST(CoreOption, arrow_access) {
  Count owned_destructions = 0;
  Option<StackValue> owned = create_stack_value(owned_destructions);
  owned->increment();

  const Option<StackValue>& const_owned = owned;
  EXPECT_EQ(const_owned->get(), 42);

  Count borrowed_destructions = 0;
  StackValue value(40, borrowed_destructions);
  Option<StackValue&> borrowed(value);
  borrowed->increment();

  const Option<StackValue&>& const_borrowed = borrowed;
  const_borrowed->increment();
  EXPECT_EQ(value.get(), 42);
}

PERIMORTEM_UNIT_TEST(CoreOption, accepts_empty) {
  Count destructions = 0;
  Option<StackValue> selected = create_stack_value(destructions);
  selected = {};

  Count branch = selected.visit(
      []() { return Count(1); }, [](const StackValue&) { return Count(2); });

  EXPECT_EQ(branch, Count(1));
  EXPECT_EQ(destructions, Count(1));
}

static_assert(__is_trivially_copyable(Option<const S32&>));
static_assert(__is_constructible(Option<const S32&>, const S32&));
static_assert(!__is_constructible(Option<const S32&>, S32&&));
static_assert(
    !__is_constructible(Option<const BorrowedBase&>, BorrowedDerived&&));
static_assert(__is_constructible(Option<StackValue>, StackValue&&));
static_assert(__is_constructible(Option<StackValue>, Option<StackValue>&&));
