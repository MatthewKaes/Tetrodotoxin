// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/core/algorithm/sort.hpp"

#include "validation/unit_test.hpp"

#include "perimortem/core/null_terminated.hpp"

#include "perimortem/memory/dynamic/bytes.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;

using namespace Validation;

static auto bytes_greater(View::Bytes lhs, View::Bytes rhs) -> Bool {
  if (lhs.get_size() != rhs.get_size()) {
    return lhs.get_size() > rhs.get_size();
  }

  for (Count i = 0; i < lhs.get_size(); i++) {
    if (lhs[i] != rhs[i]) {
      return lhs[i] > rhs[i];
    }
  }

  return false;
}

class SortBytes {
 public:
  auto operator=(View::Bytes view) -> SortBytes& {
    bytes = view;
    return *this;
  }

  auto append(U8 byte) -> void { bytes.append(byte); }

  constexpr auto get_view() const -> View::Bytes { return bytes.get_view(); }

  auto operator>(const SortBytes& rhs) const -> Bool {
    return bytes_greater(get_view(), rhs.get_view());
  }

 private:
  Dynamic::Bytes bytes;
};

static Harness AlgoSort = {
  .name = "Core::Algorithm::Sort"_view,
};

PERIMORTEM_UNIT_TEST(AlgoSort, simple_sort) {
  constexpr S32 test_const[] = {9, 8, 1, 4, 3, 7};
  S32 test[] = {9, 8, 1, 4, 3, 7};

  constexpr auto sorted_const = Algorithm::sort(test_const);
  auto sorted = Algorithm::sort(Access::Vector(test));
  auto* sorted_data = sorted.get_data();

  EXPECT_EQ(sorted_data[0], 1);
  EXPECT_EQ(sorted_data[1], 3);
  EXPECT_EQ(sorted_data[2], 4);
  EXPECT_EQ(sorted_data[3], 7);
  EXPECT_EQ(sorted_data[4], 8);
  EXPECT_EQ(sorted_data[5], 9);

  EXPECT_EQ(sorted_data[0], sorted_const[0]);
  EXPECT_EQ(sorted_data[1], sorted_const[1]);
  EXPECT_EQ(sorted_data[2], sorted_const[2]);
  EXPECT_EQ(sorted_data[3], sorted_const[3]);
  EXPECT_EQ(sorted_data[4], sorted_const[4]);
  EXPECT_EQ(sorted_data[5], sorted_const[5]);
}

PERIMORTEM_UNIT_TEST(AlgoSort, empty_sort) {
  constexpr auto sorted_const = Algorithm::sort(Access::Vector<S32>());
  auto sorted = Algorithm::sort(Access::Vector<S32>());

  EXPECT_EQ(sorted.get_size(), 0);
  EXPECT_EQ(sorted_const.get_size(), 0);
}

PERIMORTEM_UNIT_TEST(AlgoSort, large_sort) {
  constexpr auto item_count = 10017;
  S32 test[item_count] = {};
  for (Count i = 0; i < item_count; i++) {
    test[i] = item_count - i - 1;
  }

  auto sorted = Algorithm::sort(test);
  auto* sorted_data = sorted.get_data();
  for (Count i = 0; i < item_count; i++) {
    EXPECT_EQ(sorted_data[i], i);
  }
}

PERIMORTEM_UNIT_TEST(AlgoSort, heap_fallback) {
  struct Counted {
    Count value;
    Count* comparisons;

    auto operator>(const Counted& other) const -> Bool {
      ++*comparisons;
      return value > other.value;
    }
  };

  Static::Vector<Counted, 32768> storage;
  auto measure = [&](Count size) {
    Count comparisons = 0;
    for (Count i = 0; i < size; ++i) {
      storage[i] = {i < size / 2 ? i : size - i, &comparisons};
    }

    Algorithm::sort(Access::Vector<Counted>(storage.get_data(), size));
    for (Count i = 0; i < size; ++i) {
      EXPECT_EQ(storage[i].value, (i + 1) / 2);
    }

    return comparisons;
  };

  const Count smaller = measure(16384);
  const Count larger = measure(32768);
  EXPECT(larger < 3 * smaller);
}

PERIMORTEM_UNIT_TEST(AlgoSort, dynamic_types) {
  constexpr auto item_count = 37;
  SortBytes test[item_count] = {};
  for (Count i = 0; i < item_count; i++) {
    Count value = item_count - i - 1;
    test[i] = "test_string #"_view;
    test[i].append(U8('0' + (value / 10)));
    test[i].append(U8('0' + (value % 10)));
  }

  auto sorted = Algorithm::sort(Access::Vector(test));
  auto* sorted_data = sorted.get_data();

  Dynamic::Bytes validate = {};
  for (Count i = 0; i < item_count; i++) {
    validate = "test_string #"_view;
    validate.append(U8('0' + (i / 10)));
    validate.append(U8('0' + (i % 10)));
    EXPECT_TEXT(sorted_data[i].get_view(), validate.get_view());
  }
}
