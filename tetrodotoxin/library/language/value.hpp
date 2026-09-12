// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/memory/allocator/arena.hpp"

#include "perimortem/system/uuid.hpp"

#include "ttx/concept/abstract.hpp"
#include "ttx/semantic/bound.hpp"

namespace Tetrodotoxin::Library::Language {

// Value describes a materialized answer through its semantic Type and packed
// bytes. The stride is the width of one endian conversion unit, not spacing
// between C++ objects. A vector of U32 answers therefore has stride four while
// a byte vector has stride one. Neither needs another projection category.
//
// These bytes preserve the provider's value representation. They are not a
// target's runtime layout. In particular a numeric Constant may retain a wider
// value than its eventual storage Type. The supplying Type's value policy must
// interpret the byte size as well as the Type identity.
class Value {
 public:
  static constexpr Perimortem::System::Uuid contract_id{
    0x01a084b0c85e71df,
    0x97a600c071d507aa,
  };

  struct Operations {
    auto (*get_type)(const void*) -> Ttx::Concept::Abstract::Handle;
    auto (*get_bytes)(const void*, Perimortem::Memory::Allocator::Arena&)
        -> Perimortem::Core::View::Bytes;
    auto (*get_byte_stride)(const void*) -> Count;
  };

  class Handle : public Ttx::Semantic::Bound<Operations> {
   public:
    using Bound::Bound;

    auto get_type() const -> Ttx::Concept::Abstract::Handle {
      return operations.get_type(source);
    }

    // A computed scalar can place its answer in the caller's Arena. Providers
    // that already own packed data return that view directly. Both forms stay
    // valid for the caller's observation without a temporary callback lifetime.
    auto get_bytes(Perimortem::Memory::Allocator::Arena& arena) const
        -> Perimortem::Core::View::Bytes {
      return operations.get_bytes(source, arena);
    }

    auto get_byte_stride() const -> Count {
      return operations.get_byte_stride(source);
    }
  };

  // Native scalar owners share the mechanical thunk, while get_value remains
  // their semantic operation. Taking the backing member's address instead
  // would bypass an implementation that computes or overrides that answer.
  template <typename Provider>
  static auto scalar(const Provider& provider) -> Ttx::Semantic::Binding {
    using Scalar = decltype(provider.get_value());
    static_assert(
        __is_integral(Scalar) || __is_same(Scalar, Bool) ||
            __is_same(Scalar, R32) || __is_same(Scalar, R64),
        "Scalar binding cannot encode an object's headers or pointers.");
    static const Operations operations = {
      [](const void* source) -> Ttx::Concept::Abstract::Handle {
        return static_cast<const Provider*>(source)->get_type().get_interface();
      },
      [](const void* source, Perimortem::Memory::Allocator::Arena& arena)
          -> Perimortem::Core::View::Bytes {
        const auto value = static_cast<const Provider*>(source)->get_value();
        if constexpr (__is_same(Scalar, Bool)) {
          const U8 byte = value ? 1 : 0;
          return arena.proxy(Perimortem::Core::View::Bytes(&byte, 1));
        }
        return arena.proxy(
            Perimortem::Core::View::Bytes(
                reinterpret_cast<const U8*>(&value), sizeof(value)));
      },
      [](const void* source) -> Count {
        if constexpr (__is_same(Scalar, Bool)) {
          return 1;
        }
        return sizeof(static_cast<const Provider*>(source)->get_value());
      },
    };
    return Ttx::Semantic::Binding::provide<Value>(&provider, operations);
  }
};

}  // namespace Tetrodotoxin::Library::Language
