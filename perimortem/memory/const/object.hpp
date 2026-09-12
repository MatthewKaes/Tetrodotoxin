// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

namespace Perimortem::Memory::Const {

// Provides a compile time wrapper for a dynamic object.
template <typename type>
class Object {
 public:
  consteval Object() = default;
  constexpr ~Object() { destruct(); }

  consteval Object(const type& value) {
    construct();
    *source_block = value;
  }

  consteval Object(const Object& rhs) {
    if (rhs.source_block) {
      construct();
      *source_block = *rhs.source_block;
    }
  }

  consteval Object(Object&& rhs) {
    Perimortem::Core::Data::swap(source_block, rhs.source_block);
  }

  consteval auto operator=(const Object& rhs) -> Object& {
    // Noop
    if (source_block == rhs.source_block) {
      return *this;
    }

    // Requesting a clear.
    if (!rhs.source_block) {
      destruct();
      return *this;
    }

    // Since stroage is requested then make sure we have storage.
    construct();
    *source_block = *rhs.source_block;
    return *this;
  }

  consteval auto operator=(Object&& rhs) -> Object& {
    Perimortem::Core::Data::swap(source_block, rhs.source_block);
    return *this;
  }

  consteval auto get_data() const -> const type* { return source_block; }
  consteval auto get_data() -> type* { return source_block; }

 private:
  consteval auto construct() -> void {
    if (!source_block) {
      source_block = new type{};
    }
  }

  constexpr auto destruct() -> void {
    if (source_block) {
      delete source_block;
      source_block = nullptr;
    }
  }

  type* source_block = nullptr;
};

}  // namespace Perimortem::Memory::Const
