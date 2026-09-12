// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/null_terminated.hpp"

#include "ttx/concept/abstract.hpp"

namespace Ttx::Concept {

// Unknown is the provisional answer to a semantic question that the current
// graph cannot settle. Repeating the original question may later produce a
// factual identity, so Unknown is not Constant and cannot be cached as absence.
class Unknown : public Abstract {
 public:
  TTX_CONTRACT(Unknown, Abstract);

  static auto get_unknown() -> const Unknown&;

  Unknown(const Unknown&) = delete;
  auto operator=(const Unknown&) -> Unknown& = delete;

  TTX_NAME("Unknown"_view);
  TTX_EMPTY_DOCUMENTATION();

  auto bind_interface(Perimortem::System::Uuid requested) const -> Perimortem::
      Utility::Result<Semantic::Binding, Semantic::Binding::Failure> override {
    return Semantic::Binding::Failure::Pending;
  }

  constexpr auto resolve() const -> const Abstract& override { return *this; }
  constexpr auto get_type() const -> const Abstract& override { return *this; }
  constexpr auto resolve_concept(Perimortem::Core::View::Bytes) const
      -> const Abstract& override {
    return *this;
  }

 private:
  constexpr Unknown() = default;
};

}  // namespace Ttx::Concept
