// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/concept/abstract.hpp"

#include "ttx/concept/none.hpp"
#include "ttx/concept/unknown.hpp"

using namespace Ttx::Concept;
using Ttx::Semantic::Binding;

auto Abstract::get_interface() const -> Handle {
  static const Operations operations = {
    [](const void* source, perimortem_uuid requested,
       ttx_binding* result) -> ttx_binding_status {
      // The C caller has the same Abstract proof as Handle's static shortcut.
      // Preserve that view without asking a policy to negotiate it again.
      if (requested.high == TTX_ABSTRACT_ID_HIGH &&
          requested.low == TTX_ABSTRACT_ID_LOW) {
        *result = {source, &operations};
        return TTX_BINDING_SATISFIED;
      }

      return static_cast<const Abstract*>(source)
          ->bind_interface(Perimortem::System::Uuid(requested))
          .visit(
              [&](const Binding& binding) -> ttx_binding_status {
                *result = binding.get_abi();
                return TTX_BINDING_SATISFIED;
              },
              [](Binding::Failure failure) -> ttx_binding_status {
                return static_cast<ttx_binding_status>(failure);
              });
    },
    [](const void* source) -> perimortem_view_bytes {
      const auto name = static_cast<const Abstract*>(source)->get_name();
      return {name.get_data(), name.get_size()};
    },
    [](const void* source) -> ttx_documentation {
      return static_cast<const Abstract*>(source)
          ->get_documentation()
          .get_interface()
          .get_abi();
    },
    [](const void* source) -> ttx_abstract {
      return static_cast<const Abstract*>(source)
          ->resolve()
          .get_interface()
          .get_abi();
    },
    [](const void* source, perimortem_view_bytes name) -> ttx_abstract {
      return static_cast<const Abstract*>(source)
          ->resolve_concept({name.data, name.size})
          .get_interface()
          .get_abi();
    },
    [](const void* source, ttx_concept_visitor visitor) {
      auto receive = [&](Perimortem::Core::View::Bytes name,
                         const Abstract& value) {
        visitor.receive(
            visitor.source, {name.get_data(), name.get_size()},
            value.get_interface().get_abi());
      };
      static_cast<const Abstract*>(source)->visit_concepts(Visitor(receive));
    },
  };
  return Handle(this, operations);
}

auto Abstract::bind_interface(Perimortem::System::Uuid) const
    -> Perimortem::Utility::Result<Binding, Binding::Failure> {
  return Binding::Failure::Unsupported;
}

auto Abstract::get_type() const -> const Abstract& {
  return None::get_none();
}

auto Abstract::resolve_concept(Perimortem::Core::View::Bytes) const
    -> const Abstract& {
  return None::get_none();
}

auto Abstract::visit_concepts(Visitor) const -> void {}

auto Abstract::satisfies(const Abstract&) const -> Bool {
  return False;
}
