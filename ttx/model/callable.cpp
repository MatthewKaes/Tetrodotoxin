// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/model/callable.hpp"

#include "ttx/concept/unknown.hpp"

using namespace Ttx;

auto Model::Callable::bind_interface(Perimortem::System::Uuid requested) const
    -> Perimortem::Utility::
        Result<Semantic::Binding, Semantic::Binding::Failure> {
  if (requested != Callable::contract_id) {
    return Abstract::bind_interface(requested);
  }

  if (resolve().is<Concept::Unknown>()) {
    return Semantic::Binding::Failure::Pending;
  }

  static const Operations operations = {
    [](const void* source) -> Concept::Layout::Handle {
      return static_cast<const Callable*>(source)
          ->get_parameters()
          .get_interface();
    },
    [](const void* source) -> Concept::Layout::Handle {
      return static_cast<const Callable*>(source)
          ->get_results()
          .get_interface();
    },
  };
  return Semantic::Binding::provide<Callable>(this, operations);
}
