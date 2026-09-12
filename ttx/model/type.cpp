// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/model/type.hpp"

#include "ttx/concept/domain.hpp"
#include "ttx/concept/unknown.hpp"

using namespace Ttx;

auto Model::Type::bind_interface(Perimortem::System::Uuid requested) const
    -> Perimortem::Utility::
        Result<Semantic::Binding, Semantic::Binding::Failure> {
  if (requested == Concept::Domain::contract_id) {
    static const Concept::Domain::Operations operations = {
      [](const void* source, ttx_abstract* result) -> ttx_binding_status {
        *result = static_cast<const Type*>(source)->get_interface().get_abi();
        return TTX_BINDING_SATISFIED;
      },
    };
    return Semantic::Binding::provide<Concept::Domain>(this, operations);
  }
  if (requested != Type::contract_id) {
    return Abstract::bind_interface(requested);
  }

  if (resolve().is<Concept::Unknown>()) {
    return Semantic::Binding::Failure::Pending;
  }

  static const Operations operations = {
    [](const void* source) -> Concept::Layout::Handle {
      return static_cast<const Type*>(source)->get_layout().get_interface();
    },
  };
  return Semantic::Binding::provide<Type>(this, operations);
}
