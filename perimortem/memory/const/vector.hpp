// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/vector.hpp"
#include "perimortem/core/data.hpp"
#include "perimortem/core/perimortem.hpp"

namespace Perimortem::Memory::Const {

// Owns temporary array storage in constant evaluation or at runtime. Using
// the same lifetime rules in both cases lets algorithms share their working
// storage without exposing a separate allocation policy to each caller.
template <typename type>
class Vector {
 public:
  constexpr Vector() = default;
  constexpr ~Vector() {
    if (source_block) {
      delete[] source_block;
    }
  }

  constexpr Vector(const Vector& rhs) {
    if (rhs.is_empty()) {
      return;
    }

    ensure_capacity(rhs.get_size());
    size = rhs.get_size();
    for (Count i = 0; i < rhs.get_size(); i++) {
      source_block[i] = rhs.source_block[i];
    }
  }

  constexpr Vector(Vector&& rhs) {
    Perimortem::Core::Data::swap(source_block, rhs.source_block);
    Perimortem::Core::Data::swap(size, rhs.size);
    Perimortem::Core::Data::swap(capacity, rhs.capacity);
  }

  constexpr auto operator=(const Vector& rhs) -> Vector& {
    if (this == &rhs) {
      return *this;
    }

    destruct();
    ensure_capacity(rhs.get_size());
    size = rhs.get_size();
    for (Count i = 0; i < size; i++) {
      source_block[i] = rhs.source_block[i];
    }

    return *this;
  };

  constexpr auto insert(type value) -> type& {
    ensure_capacity(get_size() + 1);
    source_block[size] = static_cast<type&&>(value);
    return source_block[size++];
  }

  constexpr auto remove(Count index) -> Bool {
    if (index >= size) {
      return False;
    }

    Count last_index = size - 1;
    if (index != last_index) {
      Core::Data::swap(source_block[index], source_block[last_index]);
    }

    size--;
    return True;
  }

  constexpr auto remove_stable(Count index) -> Bool {
    if (index >= size) {
      return False;
    }

    Count last_index = size - 1;
    for (Count shift_index = index; shift_index < last_index; shift_index++) {
      Core::Data::swap(
          source_block[shift_index], source_block[shift_index + 1]);
    }

    size--;
    return True;
  }

  constexpr auto resize(Count new_size) -> void {
    ensure_capacity(new_size);
    size = new_size;
  }
  constexpr auto contains(const type& data) const -> Bool {
    return get_view().contains(data);
  }

  constexpr auto at(Count index) const -> const type& {
    return source_block[index];
  }

  constexpr auto at(Count index) -> type& { return source_block[index]; }
  constexpr auto operator[](Count index) const -> const type& {
    return at(index);
  }

  constexpr auto operator[](Count index) -> type& { return at(index); }

  constexpr operator Core::View::Vector<type>() const { return get_view(); }

  constexpr auto get_size() const -> Count { return size; }
  constexpr auto get_capacity() const -> Count { return capacity; }
  constexpr auto get_view() const -> const Core::View::Vector<type> {
    return Core::View::Vector<type>(source_block, get_size());
  }

  constexpr auto get_data() const -> const type* { return source_block; }
  constexpr auto get_data() -> type* { return source_block; }

  constexpr auto is_empty() const -> Bool { return get_size() == 0; }

 private:
  // Ensures there is _at least_ enough room for the requested number of
  // objects.
  constexpr auto ensure_capacity(Count required_size) -> void {
    // Check if we can already fit required buffer.
    if (required_size <= get_capacity()) {
      return;
    }

    // Attempt to grow by a factor of 2.
    // If that doesn't work than grow to exact size.
    const auto new_capacity =
        Core::Math::max(get_capacity() * 2, required_size);

    auto* new_block = new type[new_capacity]{};
    if (source_block) {
      for (Count i = 0; i < size; i++) {
        new_block[i] = static_cast<type&&>(source_block[i]);
      }

      delete[] source_block;
    }

    source_block = new_block;
    capacity = new_capacity;
  }

  constexpr auto destruct() -> void {
    if (source_block) {
      delete[] source_block;
    }

    source_block = nullptr;
    size = 0;
    capacity = 0;
  }

  type* source_block = nullptr;
  Count size = 0;
  Count capacity = 0;
};

}  // namespace Perimortem::Memory::Const
