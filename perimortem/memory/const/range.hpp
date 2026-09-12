// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/data.hpp"
#include "perimortem/core/perimortem.hpp"

namespace Perimortem::Memory::Const {

// Supplies a consteval lifetime managed wrapper around an allocated buffer.
template <typename type>
class Range {
 public:
  consteval Range() = default;
  constexpr ~Range() { destruct(); }
  consteval Range(Count size) { construct(size); };

  consteval Range(const Range& rhs) {
    construct(rhs.get_size());
    for (Count i = 0; i < size; i++) {
      source_block[i] = rhs.get_data()[i];
    }
  }

  consteval Range(Range&& rhs) {
    Perimortem::Core::Data::swap(size, rhs.size);
    Perimortem::Core::Data::swap(source_block, rhs.source_block);
  }

  consteval auto operator=(const Range& rhs) -> Range& {
    // Noop
    if (source_block == rhs.source_block) {
      return *this;
    }

    if (rhs.source_block == nullptr) {
      destruct();
      return *this;
    }

    if (rhs.get_size() != get_size()) {
      construct(rhs.get_size());
    }

    for (Count i = 0; i < size; i++) {
      source_block[i] = rhs.get_data()[i];
    }

    return *this;
  }

  consteval auto operator=(Range&& rhs) -> Range& {
    Perimortem::Core::Data::swap(size, rhs.size);
    Perimortem::Core::Data::swap(source_block, rhs.source_block);
    return *this;
  }

  consteval auto operator[](Count index) const -> const type& {
    return get_data()[index];
  }
  consteval auto operator[](Count index) -> type& { return get_data()[index]; }

  consteval auto get_data() const -> const type* { return source_block; }
  consteval auto get_data() -> type* { return source_block; }
  consteval auto get_size() const -> Count { return size; }

 private:
  constexpr auto construct(Count new_size) -> void {
    destruct();
    source_block = new type[new_size]{};
    size = new_size;
  }

  constexpr auto destruct() -> void {
    if (source_block) {
      delete[] source_block;
    }

    source_block = nullptr;
    size = 0;
  }

  type* source_block = nullptr;
  Count size = 0;
};

}  // namespace Perimortem::Memory::Const
