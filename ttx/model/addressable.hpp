// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/system/uuid.hpp"

#include "ttx/model/type.hpp"
#include "ttx/semantic/bound.hpp"

namespace Ttx::Model {

// Addressable names typed data that another semantic object can reach. Its
// total Type answer remains Unknown until the exact nonempty Type edge exists.
// Fields, receivers, locals, external symbols, interpreted endpoints, and
// runtime objects can all share this edge while keeping their richer behavior
// with the Dialect that defines them. That Dialect also decides whether an
// address can be written, invoked as a receiver, or observed only as a value.
class Addressable : public Concept::Abstract {
 public:
  static constexpr Perimortem::System::Uuid contract_id{
    0x01a084b0c85e726c,
    0x959c2ca55345d1cd,
  };

  TTX_CONTRACT(Addressable, Abstract);

  struct Operations {
    auto (*get_type)(const void*) -> Concept::Abstract::Handle;
  };

  class Handle : public Semantic::Bound<Operations> {
   public:
    using Bound::Bound;

    // This edge retains the policies through which the Type is reached.
    // A consumer needing a native Type can resolve it after asking any
    // boundary questions that would be lost through resolution.
    auto get_type() const -> Concept::Abstract::Handle {
      return operations.get_type(source);
    }
  };

  auto bind_interface(Perimortem::System::Uuid requested) const -> Perimortem::
      Utility::Result<Semantic::Binding, Semantic::Binding::Failure> override {
    if (requested != Addressable::contract_id) {
      return Abstract::bind_interface(requested);
    }
    static const Operations operations = {
      [](const void* source) -> Concept::Abstract::Handle {
        return static_cast<const Addressable*>(source)
            ->get_type()
            .get_interface();
      },
    };
    return Semantic::Binding::provide<Addressable>(this, operations);
  }

  virtual constexpr auto get_type() const
      -> const Ttx::Concept::Abstract& override = 0;
};

}  // namespace Ttx::Model
