// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/option.hpp"

#include "tetrodotoxin/library/language/model/memory.hpp"
#include "ttx/model/callable.hpp"

namespace Tetrodotoxin::Library::Language::Model {

// Callable derives Library's Static or Self receiver role from the one real
// parameter Layout instead of retaining a second marker on each Function.
class Callable : public Ttx::Model::Callable {
 public:
  TTX_CONTRACT(Callable, Ttx::Model::Callable);

  auto bind_interface(Perimortem::System::Uuid requested) const
      -> Perimortem::Utility::Result<
          Ttx::Semantic::Binding,
          Ttx::Semantic::Binding::Failure> override {
    if (requested == Tetrodotoxin::Source::Declaration::contract_id) {
      return Tetrodotoxin::Source::Declaration::provide(*this);
    }
    return Ttx::Model::Callable::bind_interface(requested);
  }

  // A selected Self Callable may impose receiver authority beyond exact Type
  // binding. Ordinary invocations accept the resolved receiver unchanged.
  // Borrowing intrinsic Callables use this boundary to require writable
  // storage without teaching Call about a concrete declaration category.
  virtual auto accepts_receiver(
      const Ttx::Concept::Abstract&,
      const Ttx::Concept::Abstract&) const -> Bool {
    return True;
  }

  // The owning declaration context asks each retained Callable to cross its
  // closure barriers. Signatures settle before Fields may invoke them, while
  // bodies wait until every initializer has linked. Bodyless and generated
  // Callables keep the neutral behavior.
  virtual auto link_declaration_signature(Ttx::Lexical::Cursor&) -> Bool {
    return True;
  }

  virtual auto link_declaration_body(Ttx::Lexical::Cursor&) -> Bool {
    return True;
  }

  virtual auto finalize_declaration(Ttx::Lexical::Cursor&) -> Bool {
    return True;
  }

  virtual auto link_restored_declaration_signature() -> Bool { return True; }

  // Generated semantic operations may expose an immutable result without
  // changing ordinary invocation. Absence keeps the Call dynamic.
  virtual auto fold_call(
      Perimortem::Memory::Allocator::Arena&,
      Perimortem::Core::Option<const Model::Pack&>,
      const Model::Pack&) const -> Perimortem::Core::Option<Model::Pack&> {
    return {};
  }

  virtual constexpr auto get_declaration_anchor() const
      -> Perimortem::Core::Option<Ttx::Lexical::Anchor> {
    return {};
  }

  // Registration needs receiver role before an authored Signature has linked
  // its Addressable entries. Completed and generated Callables derive the same
  // answer from their parameter Layout, while an authored owner may answer from
  // its retained Signature shape.
  virtual auto declares_self() const -> Bool { return is_type_bound(); }

  constexpr auto get_type_binding() const
      -> Perimortem::Core::Option<const Type&> {
    auto first = get_parameters().get_abstract(0);
    if (!first) {
      return {};
    }

    auto parameter = first->select<Ttx::Model::Addressable>();
    if (!parameter || parameter->get_name() != "self"_view) {
      return {};
    }
    return parameter->get_type().select<Type>();
  }

  constexpr auto is_type_bound() const -> Bool {
    return Bool(get_type_binding());
  }

  constexpr auto is_type_bound(const Type& receiver) const -> Bool {
    auto binding = get_type_binding();
    return binding && &*binding == &receiver;
  }

  // `[self]` returns the exact receiver Addressable rather than one copied
  // value. Keeping the identity check here gives every semantic and lowering
  // consumer one canonical test for the reserved reference result.
  auto get_self_result() const
      -> Perimortem::Core::Option<const Ttx::Model::Addressable&> {
    auto first = get_parameters().get_abstract(0);
    auto self =
        first ? first->select<Ttx::Model::Addressable>()
              : Perimortem::Core::Option<const Ttx::Model::Addressable&>();
    auto returned =
        get_results().get_size() == 1
            ? get_results().get_abstract(0)
            : Perimortem::Core::Option<const Ttx::Concept::Abstract&>();
    auto reference =
        returned ? returned->select<Ttx::Model::Addressable>()
                 : Perimortem::Core::Option<const Ttx::Model::Addressable&>();
    return self && self->get_name() == "self"_view && reference &&
                   &*self == &*reference
               ? reference
               : Perimortem::Core::Option<const Ttx::Model::Addressable&>();
  }

 private:
  friend class Tetrodotoxin::Source::Declaration;
  auto complete_source(
      Tetrodotoxin::Source::Declaration::Phase phase,
      Ttx::Lexical::Cursor* cursor)
      -> Tetrodotoxin::Source::Declaration::Completion {
    using Phase = Tetrodotoxin::Source::Declaration::Phase;
    switch (phase) {
    case Phase::Signature:
      return link_declaration_signature(*cursor);
    case Phase::Body:
      return link_declaration_body(*cursor);
    case Phase::Finalize:
      return finalize_declaration(*cursor);
    case Phase::RestoredSignature:
      return link_restored_declaration_signature();
    default:
      return True;
    }
  }
};

}  // namespace Tetrodotoxin::Library::Language::Model
