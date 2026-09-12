// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "tetrodotoxin/library/language/model/type.hpp"
#include "ttx/concept/domain.hpp"
#include "ttx/model/addressable.hpp"

namespace Tetrodotoxin::Library::Language::Model {

// Memory supplies the Library operations of a storage relationship. A Field,
// Local or foreign symbol can answer these questions without exposing how its
// declaration was authored or how a source pass completes it. Those questions
// belong to the separately supplied Source declaration policy.
//
// Instance members require the Library access operator, so ordinary context
// lookup does not turn a memory location into its Type's member namespace.
class Memory : public Ttx::Model::Addressable {
 public:
  TTX_CONTRACT(Memory, Ttx::Model::Addressable);

  auto bind_interface(Perimortem::System::Uuid requested) const
      -> Perimortem::Utility::Result<Ttx::Semantic::Binding,
                                     Ttx::Semantic::Binding::Failure> override {
    if (requested == Ttx::Concept::Domain::contract_id) {
      return Ttx::Concept::Domain::provide(*this);
    }
    return Ttx::Model::Addressable::bind_interface(requested);
  }

  // Instance Layout assembly retains this exact Addressable identity, but the
  // declaration alone knows whether its evaluation policy creates storage.
  // Neutral Addressables remain contextual or flow identities.
  virtual constexpr auto contributes_to_instance_layout() const -> Bool {
    return False;
  }

  // Mutation is declaration authority rather than a property inferred from
  // concrete storage classes. Neutral and computed Addressables are read only.
  virtual auto permits_write_from(const Type&) const -> Bool { return False; }

  // Constant flow is owned by the declaration that can prove it. Expressions
  // ask the selected Addressable instead of enumerating Field and Local, while
  // runtime storage and foreign bindings retain the neutral absence.
  virtual auto get_constant() const -> Perimortem::Core::Option<Pack&> {
    return {};
  }

  auto resolve_concept(Perimortem::Core::View::Bytes route) const
      -> const Ttx::Concept::Abstract& override {
    const Ttx::Concept::Abstract& resolved = resolve();
    if (&resolved != this) {
      return resolved.resolve_concept(route);
    }

    return Ttx::Concept::Unknown::get_unknown();
  }

  virtual constexpr auto get_type() const
      -> const Ttx::Concept::Abstract& override = 0;

 protected:
  friend class Ttx::Concept::Domain;

  // A concrete memory provider can retain a richer source edge than its
  // native value Type. That override keeps import policy on Domain queries.
  virtual auto get_domain() const -> Ttx::Concept::Domain::Answer {
    const auto& type = get_type();
    if (&type == &Ttx::Concept::Unknown::get_unknown()) {
      return Ttx::Semantic::Binding::Failure::Pending;
    }
    return type.get_interface();
  }
};

}  // namespace Tetrodotoxin::Library::Language::Model
