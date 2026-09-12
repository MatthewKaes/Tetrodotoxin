// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/system/uuid.hpp"

#include "ttx/concept/abstract.hpp"
#include "ttx/concept/layout.hpp"
#include "ttx/semantic/bound.hpp"

namespace Ttx::Model {

// Callable shares the parameter and result shapes of one invocation. An
// argument Pack fits the parameter Layout and the resulting Pack follows the
// result Layout. Editors, compilers, and language runtimes can all use that
// contract while the defining Dialect keeps the executable body and receiver
// rules that give the Callable its richer meaning.
class Callable : public Concept::Abstract {
 public:
  static constexpr Perimortem::System::Uuid contract_id{
    0x01a084b0c85e765a,
    0x804612a14f04eeb0,
  };

  TTX_CONTRACT(Callable, Abstract);

  struct Operations {
    auto (*get_parameters)(const void*) -> Concept::Layout::Handle;
    auto (*get_results)(const void*) -> Concept::Layout::Handle;
  };

  class Handle : public Semantic::Bound<Operations> {
   public:
    using Bound::Bound;

    auto get_parameters() const -> Concept::Layout::Handle {
      return operations.get_parameters(source);
    }

    auto get_results() const -> Concept::Layout::Handle {
      return operations.get_results(source);
    }
  };

  auto bind_interface(Perimortem::System::Uuid requested) const -> Perimortem::
      Utility::Result<Semantic::Binding, Semantic::Binding::Failure> override;

  virtual constexpr auto get_parameters() const -> const Concept::Layout& = 0;
  virtual constexpr auto get_results() const -> const Concept::Layout& = 0;
};

}  // namespace Ttx::Model
