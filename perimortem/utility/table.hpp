// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/bytes.hpp"
#include "perimortem/core/view/vector.hpp"
#include "perimortem/core/static/vector.hpp"
#include "perimortem/core/data.hpp"
#include "perimortem/core/option.hpp"

#include "perimortem/utility/pair.hpp"

namespace Perimortem::Utility {

// An optimized look up table for `Core::View::Bytes` that avoids a level of
// indirection by packing string keys inline and provides major speed gains
// when testings a large number of keys with a low hit rate making it ideal for
// keyword lookups.
//
// Tables are only immutable and unlike `Map` always have static linkage.
// For instances where keys are all known at compile time and the list of keys
// is relatively small the speed of Table tends to be >2x faster than any Map
// for key kits.
//
// The source template parameter is a bit funky since the type itself is
// parameterized over it's source data to create a type that expresses the
// compressed form of the lookups.
template <typename value_type, const auto& source, U64 cache_line_size = 1>
class Table {
 private:
  // Accepts both a raw C array and a View::Vector as source.
  static consteval auto get_source_count() -> Count {
    if constexpr (requires { source.get_size(); }) {
      return source.get_size();
    } else {
      return Core::Data::array_size(source);
    }
  }

  static consteval auto required_storage() -> Count {
    Count buckets[max_length()] = {0};
    for (Count i = 0; i < get_source_count(); i++) {
      buckets[source[i].key.get_size()] += source[i].key.get_size();
    }

    Count total = 0;
    for (Count i = 0; i < max_length(); i++) {
      const auto cache_alignment = (total % cache_line_size);
      if (cache_alignment != 0 &&
          cache_alignment + buckets[i] > cache_line_size) {
        total += cache_line_size - cache_alignment;
      }

      total += buckets[i];
    }

    return total;
  }

  // Get the number of size buckets we will need to store.
  // We give up one bucket for size "0" strings for clamping later.
  static consteval auto max_length() -> Count {
    Count max = 0;
    for (Count i = 0; i < get_source_count(); i++) {
      max = max > source[i].key.get_size() + 1 ? max
                                               : source[i].key.get_size() + 1;
    }

    return max;
  }

  // The stroage required for all of the string values.
  static constexpr Count storage_size = required_storage();
  static constexpr Count max_range = max_length();

  struct Storage {
    consteval Storage() {
      // Caculate the bytes required for each bucket
      Count buckets[max_length()] = {0};
      for (Count i = 0; i < get_source_count(); i++) {
        buckets[source[i].key.get_size()] += source[i].key.get_size();
      }

      // Align and set all bucket indexes
      Count total = 0;
      for (Count i = 0; i < max_length(); i++) {
        // If the bucket crosses a cache line then move it over.
        const auto cache_alignment = (total % cache_line_size);
        if (cache_alignment != 0 &&
            cache_alignment + buckets[i] > cache_line_size) {
          total += cache_line_size - cache_alignment;
        }

        buffer_coordinates[i].byte_index = total;
        total += buckets[i];
        buckets[i] = buffer_coordinates[i].byte_index;
      }

      total = 0;
      for (Count size = 0; size < max_length(); size++) {
        buffer_coordinates[size].item_index = total;
        for (Count i = 0; i < get_source_count(); i++) {
          if (source[i].key.get_size() != size) {
            continue;
          }

          mappings[total] = source[i].value;
          buffer_coordinates[size].item_count++;
          total++;
          for (Count c = 0; c < source[i].key.get_size(); c++) {
            buffer[buckets[size]++] = source[i].key[c];
          }
        }
      }
    }

    struct Coord {
      U8 item_index = 0;
      U8 item_count = 0;
      U16 byte_index = 0;
    };

    alignas(64) Core::Static::Vector<U8, storage_size> buffer = {};
    Core::Static::Vector<Coord, max_range + 1> buffer_coordinates = {};
    Core::Static::Vector<value_type, get_source_count()> mappings = {};
  };

  static constexpr Storage byte_pack;

  static constexpr auto find_or_null(const Core::View::Bytes key)
      -> const value_type* {
    // Get the value in range.
    const auto range_step = key.get_size();
    if (range_step > max_range) {
      return nullptr;
    }

    const auto entry = byte_pack.buffer_coordinates.get_data()[range_step];
    const auto item_start = entry.item_index;
    const auto item_count = entry.item_count;
    const auto byte_start = entry.byte_index;
    for (U8 i = 0; i < item_count; i++) {
      const auto byte_index = byte_start + i * range_step;
      if (byte_pack.buffer[byte_index] != key[0]) {
        continue;
      }

      if (Core::Data::compare(
              byte_pack.buffer.get_data() + byte_index, key.get_data(),
              range_step)) {
        return &byte_pack.mappings[item_start + i];
      }
    }

    return nullptr;
  }

 public:
  static constexpr auto find_or_default(
      const Core::View::Bytes key,
      const value_type default_value) -> value_type {
    auto value = find_or_null(key);
    return value == nullptr ? default_value : *value;
  }

  static constexpr auto find(const Core::View::Bytes key)
      -> Core::Option<value_type> {
    auto value = find_or_null(key);
    return value == nullptr ? Core::Option<value_type>() : *value;
  }

  static consteval auto get_values() -> Core::View::Vector<value_type> {
    return byte_pack.mappings;
  }
};

}  // namespace Perimortem::Utility
