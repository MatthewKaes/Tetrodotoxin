// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/concept/none.hpp"
#include "ttx/concept/reference.hpp"
#include "ttx/concept/unknown.hpp"

namespace Ttx::Model {

// Alias makes one required referent shareable before its identity is known.
// Every semantic question goes to that referent, so a consumer can use the
// relationship without knowing that an Alias carries it. Names, documentation,
// and policy belong to concrete declarations rather than to this forwarding
// machinery. There is no Alias contract to discover through binding.
//
// Unknown represents the initial promise. Commitment spot resolves the supplied
// subject and retains one exact answer, leaving no Alias chain to walk later.
// None cannot fulfill a required relationship, and a different exact answer
// cannot replace one already observed. A new source generation needs a new
// Alias rather than rebinding an existing commitment.
//
// The constructing owner keeps the referent alive and orders commitment against
// queries. Each forwarding operation captures its referent once, so a callback
// cannot turn one observation into a mixture of the old and new answers.
class Alias : public Concept::Abstract {
 public:
  Alias() : target(Concept::Unknown::get_unknown()) {}

  // A failed commitment preserves the current referent. Unknown can leave the
  // promise open, while repeating the same exact commitment is harmless.
  constexpr auto commit(const Concept::Abstract& subject) -> Bool {
    const Concept::Abstract& selected = subject.resolve();
    if (&selected == this || &selected == &Concept::None::get_none() ||
        &selected.resolve() != &selected) {
      return False;
    }

    const Concept::Abstract& current = target.get();
    if (&current != &Concept::Unknown::get_unknown()) {
      return &current == &selected;
    }
    target = Concept::Reference<const Concept::Abstract>(selected);
    return True;
  }

  constexpr auto get_name() const -> Perimortem::Core::View::Bytes override {
    const Concept::Abstract& current = target.get();
    return current.get_name();
  }

  constexpr auto get_documentation() const
      -> const Concept::Documentation& override {
    const Concept::Abstract& current = target.get();
    return current.get_documentation();
  }

  constexpr auto resolve() const -> const Concept::Abstract& override {
    return target.get();
  }

  constexpr auto get_type() const -> const Concept::Abstract& override {
    const Concept::Abstract& current = target.get();
    return current.get_type();
  }

  constexpr auto resolve_concept(Perimortem::Core::View::Bytes name) const
      -> const Concept::Abstract& override {
    const Concept::Abstract& current = target.get();
    return current.resolve_concept(name);
  }

  constexpr auto visit_concepts(Concept::Abstract::Visitor visitor) const
      -> void override {
    const Concept::Abstract& current = target.get();
    current.visit_concepts(visitor);
  }

  constexpr auto satisfies(const Concept::Abstract& requirement) const
      -> Bool override {
    const Concept::Abstract& current = target.get();
    return current.satisfies(requirement);
  }

  constexpr auto bind_interface(Perimortem::System::Uuid requested) const
      -> Perimortem::Utility::
          Result<Semantic::Binding, Semantic::Binding::Failure> override {
    const Concept::Abstract& current = target.get();
    return current.bind_interface(requested);
  }

 private:
  Concept::Reference<const Concept::Abstract> target;
};

}  // namespace Ttx::Model
