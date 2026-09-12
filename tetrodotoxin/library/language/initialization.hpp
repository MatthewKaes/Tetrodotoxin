// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/memory/allocator/arena.hpp"

#include "perimortem/system/uuid.hpp"

#include "ttx/concept/abstract.hpp"
#include "ttx/semantic/bound.hpp"

namespace Tetrodotoxin::Library::Language {

// Initialization is a Library question beyond the shape shared by TTX Type.
// Its answer can be a materialized Value or a Callable supplied by a compiled
// implementation. A contextual Type may have no initializer. Failure to
// provide an implementation must remain distinct from that completed absence.
class Initialization {
 public:
  static constexpr Perimortem::System::Uuid contract_id{
    0x01a084b0c85e75e5,
    0xb7c5566278c41d66,
  };

  using Answer = Perimortem::Utility::Result<
      Perimortem::Core::Option<Ttx::Concept::Abstract::Handle>,
      Ttx::Semantic::Binding::Failure>;

  struct Operations {
    auto (*get_default)(const void*, Perimortem::Memory::Allocator::Arena&)
        -> Answer;
  };

  class Handle : public Ttx::Semantic::Bound<Operations> {
   public:
    using Bound::Bound;

    auto get_default(Perimortem::Memory::Allocator::Arena& arena) const
        -> Answer {
      return operations.get_default(source, arena);
    }
  };
};

}  // namespace Tetrodotoxin::Library::Language
