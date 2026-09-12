// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/vector.hpp"
#include "perimortem/core/access/vector.hpp"
#include "perimortem/core/bibliotheca.hpp"
#include "perimortem/core/data.hpp"

namespace Perimortem::Core {

template <typename value_type = void>
class Object;

// Object<void> is the erased one word carrier shared by C++ and generated
// code. It deliberately performs no automatic lifetime work because compiler
// output places retain and release at its semantic value boundaries.
template <>
class Object<void> {
 public:
  using Finalizer = void (*)(U8*);

  class Descriptor {
   public:
    constexpr Descriptor(Count size, Count alignment, Finalizer finalizer)
        : size(size), alignment(alignment), finalizer(finalizer) {}

    constexpr auto get_size() const -> Count { return size; }
    constexpr auto get_alignment() const -> Count { return alignment; }
    constexpr auto get_finalizer() const -> Finalizer { return finalizer; }

   private:
    Count size;
    Count alignment;
    Finalizer finalizer;
  };

  constexpr Object() = default;
  explicit constexpr Object(U8* payload) : payload(payload) {}

  static auto create(const Descriptor& descriptor) -> Object;
  static auto create(
      const Descriptor& descriptor,
      Count count,
      Count element_size) -> Object;

  auto retain() const -> void;
  auto release() const -> void;

  constexpr auto get_payload() const -> U8* { return payload; }
  auto get_capacity() const -> Count;
  auto get_reservations() const -> Count;
  auto get_descriptor() const -> const Descriptor&;

  constexpr auto is_empty() const -> Bool { return !payload; }

 private:
  U8* payload = {};
};

// Object<T> is the typed Core owner mirrored by Library Object[T]. Empty
// storage is one valid value. Copies retain the same Object, so writable
// access observes the same elements through every alias. Growing one handle
// replaces only that handle because Bibliotheca allocations have fixed bounds.
template <typename value_type>
class Object {
 public:
  constexpr Object() = default;

  explicit Object(Count capacity) { create_buffer(capacity, {}, 0); }

  Object(const Object& rhs) : storage(rhs.storage) { storage.retain(); }

  Object(Object&& rhs) : storage(rhs.storage) { rhs.storage = Object<>(); }

  auto operator=(const Object& rhs) -> Object& {
    if (storage.get_payload() == rhs.storage.get_payload()) {
      return *this;
    }

    rhs.storage.retain();
    storage.release();
    storage = rhs.storage;
    return *this;
  }

  auto operator=(Object&& rhs) -> Object& {
    if (this == &rhs) {
      return *this;
    }

    storage.release();
    storage = rhs.storage;
    rhs.storage = Object<>();
    return *this;
  }

  ~Object() { storage.release(); }

  auto get_capacity() const -> Count {
    return storage.get_capacity() / sizeof(value_type);
  }

  auto get_view() const -> View::Vector<value_type> {
    return View::Vector<value_type>(get_data(), get_capacity());
  }

  auto get_access() -> Access::Vector<value_type> {
    return Access::Vector<value_type>(get_data(), get_capacity());
  }

  auto reserve(Count count) -> Access::Vector<value_type> {
    Count capacity = get_capacity();
    if (count <= capacity) {
      return Access::Vector<value_type>(get_data(), capacity);
    }

    Count requested = Math::max(count, capacity);
    Object replacement;
    replacement.create_buffer(requested, get_data(), capacity);
    *this = static_cast<Object&&>(replacement);
    return Access::Vector<value_type>(get_data(), get_capacity());
  }

  auto clone() -> void {
    Count capacity = get_capacity();
    if (capacity == 0) {
      return;
    }

    Object replacement;
    replacement.create_buffer(capacity, get_data(), capacity);
    *this = static_cast<Object&&>(replacement);
  }

  constexpr auto is_empty() const -> Bool { return storage.is_empty(); }

  auto is_shared() const -> Bool {
    return !storage.is_empty() && storage.get_reservations() > 1;
  }

 private:
  static auto destroy_buffer(U8* payload) -> void {
    Count capacity = Bibliotheca::capacity(payload) / sizeof(value_type);
    value_type* values = Data::cast<value_type>(payload);
    for (Count index = capacity; index != 0; index--) {
      values[index - 1].~value_type();
    }
  }

  auto create_buffer(
      Count requested,
      const value_type* source,
      Count source_size) -> void {
    if (requested == 0) {
      return;
    }

    storage =
        Object<>::create(buffer_descriptor, requested, sizeof(value_type));
    value_type* values = get_data();
    Count capacity = get_capacity();
    for (Count index = 0; index < capacity; index++) {
      if (index < source_size) {
        new (values + index, Placement::Construct) value_type(source[index]);
      } else {
        new (values + index, Placement::Construct) value_type();
      }
    }
  }

  auto get_data() const -> value_type* {
    return Data::cast<value_type>(storage.get_payload());
  }

  inline static constexpr Object<>::Descriptor buffer_descriptor{
    sizeof(value_type), alignof(value_type), destroy_buffer};

  Object<> storage;
};

static_assert(sizeof(Object<>) == sizeof(U8*));
static_assert(alignof(Object<>) == alignof(U8*));
static_assert(__is_trivially_copyable(Object<>));
static_assert(
    sizeof(Object<>::Descriptor) ==
    sizeof(Count) * 2 + sizeof(Object<>::Finalizer));
static_assert(alignof(Object<>::Descriptor) == alignof(Count));
static_assert(__is_standard_layout(Object<>::Descriptor));
static_assert(sizeof(Object<U8>) == sizeof(Object<>));

}  // namespace Perimortem::Core
