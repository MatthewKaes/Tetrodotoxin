// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/system/uuid.hpp"

#include "ttx/concept/abstract.hpp"
#include "ttx/concept/layout.hpp"
#include "ttx/model/layouts/value.hpp"
#include "ttx/semantic/bound.hpp"

namespace Ttx::Model {

// Type is the shared contract for a semantic identity that can participate in
// value flow and lowering. Its Layout describes target neutral shape, while the
// concrete language owns scalar families, defaults, access rules, and any
// representation facts needed by its compilers.
//
// A language may reserve the Type identity before every declaration edge is
// ready. It resolves to Unknown during that work, then exposes its completed
// Layout through the same object.
//
// Atomic Types use one Value leaf containing their exact identity. Structural
// Types expose the shape built by their owner. A completed Type admitted to
// ordinary value flow has at least one entry, while an empty Type remains
// useful as a context for names and Static Callables.
class Type : public Concept::Abstract {
 public:
  static constexpr Perimortem::System::Uuid contract_id{
    0x01a084b0c85e7e8a,
    0x89540719a5f0d59b,
  };

  TTX_CONTRACT(Type, Abstract);

  struct Operations {
    auto (*get_layout)(const void*) -> Concept::Layout::Handle;
  };

  class Handle : public Semantic::Bound<Operations> {
   public:
    using Bound::Bound;

    auto get_layout() const -> Concept::Layout::Handle {
      return operations.get_layout(source);
    }
  };

  auto bind_interface(Perimortem::System::Uuid requested) const -> Perimortem::
      Utility::Result<Semantic::Binding, Semantic::Binding::Failure> override;

  virtual constexpr auto get_layout() const -> const Concept::Layout& {
    return layout;
  }

 protected:
  constexpr Type() : layout(*this) {}

  // The Value Layout leaf borrows this exact semantic identity. Copying or
  // moving a Type would leave that leaf naming the original object, so every
  // Type remains a stable graph identity after construction.
  Type(const Type&) = delete;
  Type(Type&&) = delete;
  auto operator=(const Type&) -> Type& = delete;
  auto operator=(Type&&) -> Type& = delete;

 private:
  Layouts::Value layout;
};

}  // namespace Ttx::Model
