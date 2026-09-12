// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/access/vector.hpp"
#include "perimortem/core/static/vector.hpp"
#include "perimortem/core/data.hpp"
#include "perimortem/core/math.hpp"

namespace Perimortem::Core::Algorithm {

// Perimortem's standard sort function.
// Very similar to std::sort but optimized to perform ~20% better for extremely
// large arrays.
//
// Sorting works on nonmovable types however it still invalidates all pointers
// to objects in the array.
//
// `Math::sort` only requires a type to support `operator>`.
template <typename type>
constexpr auto sort(Core::Access::Vector<type> access)
    -> Core::Access::Vector<type> {
  // Special case trivially sorted arrays.
  if (access.get_size() <= 1) {
    return access;
  }

  // Not a fan of polluting namespaces, so inline with the Perimortem coding
  // standards all of the helper functions are internal lambdas.
  //
  // Don't convert the lambdas to constexpr, that will cause clang to slowdown
  // by upwards of ~40% for larger arrays.
  auto data = access.get_data();
  auto size = access.get_size();

  // Takes three elements and caculates which index is the median of the three.
  // This improves our selection quality by quite a bit.
  // Tried switching to take an array of three elements but that won't optimize
  // out so we are stuck with the three element signiture.
  //
  // Best performing out of a few tested options:
  // https://godbolt.org/z/ddovqjosc
  auto branchless_median_three = [](const type* data, Count a, Count b,
                                    Count c) -> Count {
    Bool a_bigger_b = data[a] > data[b];
    Bool a_bigger_c = data[a] > data[c];
    Bool b_bigger_c = data[b] > data[c];
    Count index[] = {a, b, c};
    return index
        [(a_bigger_b == a_bigger_c).value + (a_bigger_c ^ b_bigger_c).value];
  };

  // Spread out sampling for larger array spans.
  auto ninther_median = [&](type* data, Count size) -> Count {
    auto group = size / 8;
    auto a = branchless_median_three(data, 0, group, group * 2);
    auto b = branchless_median_three(data, group * 3, group * 4, group * 5);
    auto c = branchless_median_three(data, group * 6, group * 7, size - 1);
    return branchless_median_three(data, a, b, c);
  };

  auto heapify_max = [](type* partition, Count size, Count root_index) -> void {
    auto left = 2 * root_index + 1;
    while (left < size) {
      // Start with the left index.
      auto largest = left;

      // Check if the right value exists and if it's larger than left.
      if (left + 1 < size && partition[left + 1] > partition[left]) {
        largest += 1;
      }

      // Hit a point where we are already a valid heap.
      if (!(partition[largest] > partition[root_index])) {
        return;
      }

      // Need to swap and decend to the next level.
      Core::Data::swap(partition[root_index], partition[largest]);
      root_index = largest;
      left = 2 * root_index + 1;
    }
  };

  // Simple insertion sort for fixing the left over chunks from intro sort.
  auto heap_sort = [&heapify_max](type* partition, Count size) -> void {
    // Heapify the partition.
    for (Count i = size / 2; i > 0; i--) {
      heapify_max(partition, size, i - 1);
    }

    // Perform the actual heap sort.
    for (Count i = size - 1; i > 0; i--) {
      Core::Data::swap(partition[0], partition[i]);
      heapify_max(partition, i, 0);
    }
  };

  // Simple insertion sort for fixing the left over chunks from intro sort.
  auto insertion_sort = [](type* data, Count size) -> void {
    for (Count i = 1; i < size; i++) {
      // Bit odd but Perimortem sort only requires the greater than operator to
      // be valid for the type in order to be sorted.
      if (!(data[i - 1] > data[i])) {
        continue;
      }

      // Also a bit janky, but we always have valid data and this let's us
      // easily capture if j would go below zero without S64.
      Count j = i;
      do {
        Core::Data::swap(data[j - 1], data[j]);
        j--;
      } while (j > 0 && data[j - 1] > data[j]);
    }
  };

  // Quick sort partitioning.
  auto quick_partition = [](type* partition, Count partition_size,
                            Count pivot_index) -> Count {
    Core::Data::swap(partition[pivot_index], partition[partition_size - 1]);
    Count new_pivot = 0;
    for (Count i = 0; i < partition_size - 1; i++) {
      if (!(partition[i] > partition[partition_size - 1])) {
        Core::Data::swap(partition[new_pivot], partition[i]);
        new_pivot++;
      }
    }

    Core::Data::swap(partition[new_pivot], partition[partition_size - 1]);
    return new_pivot;
  };

  // Lomuto quicksort + ninther/median of three pivot + heapsort fallback.
  // Subarrays chucks of size <= 32 are left for a single insertion sort.
  constexpr auto insertion_sort_cutoff = Count(32);
  constexpr auto ninther_median_cutoff = Count(128);
  auto introsort = [&](this auto&& self, type* partition, Count partition_size,
                       Count depth) -> void {
    while (partition_size > insertion_sort_cutoff) {
      // Decrement depth and check if we are below the point for heap_sort.
      // Perform a post decrement to decrement the value in the event we
      // continue. If the value is zero and underflows we throw it away anyway.
      if (depth-- == 0) {
        heap_sort(partition, partition_size);
        return;
      }

      // Caculate the pivot index by sampling the array to try and avoid picking
      // the worst possible pivot.
      // This makes the performance far more stable across runs.
      // For larger arrays use ninther sampling to further improve stability.
      Count pivot_index =
          (partition_size > ninther_median_cutoff)
              ? ninther_median(data, partition_size)
              : branchless_median_three(
                    data, Count(0), partition_size / 2, partition_size - 1);

      Count split = quick_partition(partition, partition_size, pivot_index);
      if (split < partition_size - split - 1) {
        self(partition, split, depth);
        partition += split + 1;
        partition_size -= split + 1;
      } else {
        self(partition + split + 1, partition_size - split - 1, depth);
        partition_size = split;
      }
    }
  };

  // Perform the introsort to start fixing the array.
  // 2 times the log2 depth gives us the cutoff point for heap sorting.
  introsort(data, size, 2 * Math::log2(size));

  // Fix residual fragmented chunks in the array.
  insertion_sort(data, size);
  return access;
}

template <typename type, Count item_count>
consteval auto sort(const type (&data)[item_count])
    -> Core::Static::Vector<type, item_count> {
  Core::Static::Vector<type, item_count> output;
  for (Count i = 0; i < item_count; i++) {
    output[i] = data[i];
  }

  sort(output.get_access());
  return output;
}

template <typename type, Count item_count>
constexpr auto sort(type (&data)[item_count]) {
  return sort(Core::Access::Vector<type>(data));
}

}  // namespace Perimortem::Core::Algorithm
