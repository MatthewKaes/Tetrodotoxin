// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/vector.hpp"
#include "perimortem/core/access/vector.hpp"
#include "perimortem/core/bibliotheca.hpp"
#include "perimortem/core/data.hpp"
#include "perimortem/core/math.hpp"
#include "perimortem/core/perimortem.hpp"

namespace Perimortem::Memory::Dynamic {

// A simple linear flat array of trivially constructable values.
template <typename type>
class Vector {
 public:
  static constexpr Count start_capacity = 8;
  static constexpr Count growth_factor = 2;

  Vector() {};
  Vector(const Vector& rhs) {
    ensure_capacity(rhs.get_size());
    size = rhs.get_size();
    for (Count i = 0; i < size; i++) {
      new (source_block + i, Core::Placement::Construct)
          type(rhs.source_block[i]);
    }
  }

  Vector(Vector&& rhs) {
    Core::Data::swap(size, rhs.size);
    Core::Data::swap(capacity, rhs.capacity);
    Core::Data::swap(source_block, rhs.source_block);
  }

  Vector(Count capacity) {
    ensure_capacity(capacity);
    size = 0;
  }

  auto operator=(Vector&& rhs) -> Vector& {
    // Swap source blocks. Since move is not destructive, the donor destructor
    // releases the old block this vector used to own.
    Core::Data::swap(size, rhs.size);
    Core::Data::swap(capacity, rhs.capacity);
    Core::Data::swap(source_block, rhs.source_block);
    return *this;
  }

  auto operator=(const Vector& rhs) -> Vector& {
    if (this == &rhs) {
      return *this;
    }

    clear();
    ensure_capacity(rhs.get_size());
    size = rhs.get_size();
    for (Count i = 0; i < size; i++) {
      new (source_block + i, Core::Placement::Construct)
          type(rhs.source_block[i]);
    }

    return *this;
  }

  ~Vector() { reset(); }

  constexpr operator Core::View::Vector<type>() const { return get_view(); }
  constexpr operator Core::Access::Vector<type>() { return get_access(); }

  auto clear() -> void {
    if (source_block) {
      destruct();
    }

    size = 0;
  }

  // Unlike the clear function, reset returns the actual buffer.
  // Useful if the vector will be reused to store highly variable size objects.
  auto reset() -> void {
    if (source_block) {
      destruct();
      Core::Bibliotheca::remit((U8*)source_block);
    }

    size = 0;
    capacity = 0;
    source_block = nullptr;
  }

  constexpr auto insert(const type& data) -> type& {
    ensure_capacity(size + 1);

    // Construct using the copy constructor.
    return *new (source_block + (size++), Core::Placement::Construct)
        type(data);
  }

  constexpr auto emplace(type&& data) -> type& {
    ensure_capacity(size + 1);

    // Construct using the move constructor.
    return *new (source_block + (size++), Core::Placement::Construct)
        type(Core::Data::take(data));
  }

  auto remove(Count index) -> Bool {
    if (index >= size) {
      return False;
    }

    Count last_index = size - 1;
    if (index != last_index) {
      Core::Data::swap(source_block[index], source_block[last_index]);
    }

    source_block[last_index].~type();
    size--;
    return True;
  }

  auto remove_stable(Count index) -> Bool {
    if (index >= size) {
      return False;
    }

    Count last_index = size - 1;
    for (Count shift_index = index; shift_index < last_index; shift_index++) {
      Core::Data::swap(
          source_block[shift_index], source_block[shift_index + 1]);
    }

    source_block[last_index].~type();
    size--;
    return True;
  }

  // Resizes the container but attempts to preserve as much of the original
  // buffer as will fit in the new size.
  //
  // Trivial values can be recovered by restoring the old size. Owning values
  // are destroyed when removed and constructed again when the range grows.
  auto resize(Count new_size) -> void {
    // Noop
    if (new_size == size) {
      return;
    }

    if (new_size < size) {
      // If not trivially destructable then we need to destruct the values that
      // are now outside of the range.
      if constexpr (!__is_trivially_destructible(type)) {
        for (Count i = new_size; i < size; i++) {
          source_block[i].~type();
        }
      }

      size = new_size;
      return;
    }

    // If the size is larger check if we need to perform a growth opreation.
    ensure_capacity(new_size);
    if constexpr (!__is_trivially_constructible(type)) {
      for (Count i = size; i < new_size; i++) {
        new (source_block + i, Core::Placement::Construct) type();
      }
    }

    size = new_size;
  }

  // Ensures there is enough room to store a required size, but declares we
  // don't care about the buffer's existing contents.
  //
  // Both growing and shrinking the buffer can be destructive operations so the
  // contents after a forgetful operation should always be assumed to be in an
  // invalid state.
  // Skipping element construction and destruction is only available when
  // neither operation has work to perform. Compiler intrinsics establish
  // that restriction without a runtime branch or additional headers.
  auto forgetful_resize(Count required_size) -> void
    requires(
        __is_trivially_constructible(type) &&
        __is_trivially_destructible(type))
  {
    // Always set the size.
    size = required_size;

    // Get the capacity bounds and check if we need a realloc.
    // If the block fits in the current Bibliotheca archive then reuse it.
    // If the block size requires at least one step up or step down then request
    // a new block.
    if (required_size <= capacity && required_size > (capacity >> 1)) {
      return;
    }

    if (source_block) {
      Core::Bibliotheca::remit((U8*)source_block);
    }

    auto alloc = Core::Bibliotheca::check_out(required_size * sizeof(type));
    source_block = Core::Data::cast<type>(alloc.ptr);
    capacity = alloc.capacity / sizeof(type);
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

  constexpr auto get_size() const -> Count { return size; }
  constexpr auto get_capacity() const -> Count { return capacity; }
  constexpr auto get_view() const -> const Core::View::Vector<type> {
    return Core::View::Vector<type>(source_block, get_size());
  }

  constexpr auto get_data() const -> const type* { return source_block; }
  constexpr auto get_data() -> type* { return source_block; }
  constexpr auto get_access() -> Core::Access::Vector<type> {
    return Core::Access::Vector<type>(source_block, get_size());
  }

 private:
  auto destruct() -> void {
    // Look over all entries and destruct the keys and values.
    for (Count i = 0; i < size; i++) {
      source_block[i].~type();
    }
  }

  // Ensures there is _at least_ enough room for the requested number of
  // objects.
  auto ensure_capacity(Count required_size) -> void {
    // Check if we can already fit required buffer.
    if (required_size <= get_capacity()) {
      return;
    }

    // Attempt to grow by a factor of 2.
    // If that doesn't work than grow to exact size.
    const auto new_capacity =
        Core::Math::max(get_capacity() * 2, required_size);

    // Fetch and transfer to new block.
    auto alloc = Core::Bibliotheca::check_out(new_capacity * sizeof(type));
    auto new_block = Core::Data::cast<type>(alloc.ptr);
    if (source_block) {
      if constexpr (__is_trivially_copyable(type)) {
        memcpy(new_block, source_block, sizeof(type) * size);
      } else {
        for (Count i = 0; i < size; i++) {
          new (new_block + i, Core::Placement::Construct) type(source_block[i]);
        }

        destruct();
      }

      Core::Bibliotheca::remit((U8*)source_block);
    }

    // Update block and get the new capacity.
    source_block = new_block;

    // Get the actual capacity provided which is often more than we actual
    // requested.
    capacity = alloc.capacity / sizeof(type);
  }

  type* source_block = nullptr;
  Count size = 0;
  Count capacity = 0;
};

}  // namespace Perimortem::Memory::Dynamic
