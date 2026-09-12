// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/option.hpp"

#include "perimortem/memory/allocator/arena.hpp"

#include "tetrodotoxin/language/definition.hpp"
#include "tetrodotoxin/library/language/constant.hpp"
#include "tetrodotoxin/library/language/model/memory.hpp"
#include "tetrodotoxin/library/language/model/pack.hpp"
#include "tetrodotoxin/library/language/type_reference.hpp"
#include "tetrodotoxin/library/language/writability.hpp"
#include "tetrodotoxin/source/declaration.hpp"
#include "ttx/concept/reference.hpp"
#include "ttx/lexical/anchor.hpp"
#include "ttx/lexical/cursor.hpp"

namespace Tetrodotoxin::Library::Language {

// Field is the exact Addressable binding retained by a Library Composite. Its
// authored Visibility decides readable lookup while Writability records its
// Static, state, or compile time evaluation policy without widening the shared
// TTX Addressable contract. An initializer remains its real Pack: declared
// Fields receive the complete flow through Layout fitting, while inference
// accepts only one scalar output and retains that exact Type.
class Field : public Model::Memory {
 private:
  constexpr Field(
      Perimortem::Memory::Allocator::Arena& domain,
      Tetrodotoxin::Language::Definition& definition,
      Writability writability,
      Perimortem::Core::Option<TypeReference> type_reference,
      Perimortem::Core::Option<Model::Pack&> initializer,
      Perimortem::Core::Option<Ttx::Concept::Reference<const Model::Type>>
          type = {},
      Bool generated = False)
      : definition(definition),
        domain(domain),
        writability(writability),
        type_reference(type_reference),
        initializer(initializer),
        type(type),
        generated(generated),
        initializer_linked(!initializer) {}

 public:
  TTX_CONTRACT(Field, Model::Memory);

  auto bind_interface(Perimortem::System::Uuid requested) const
      -> Perimortem::Utility::Result<
          Ttx::Semantic::Binding,
          Ttx::Semantic::Binding::Failure> override {
    if (requested == Tetrodotoxin::Source::Declaration::contract_id) {
      return Tetrodotoxin::Source::Declaration::provide(*this);
    }
    if (requested == Tetrodotoxin::Language::Definition::contract_id) {
      return Tetrodotoxin::Language::Definition::provide(*this);
    }
    if (requested == Ttx::Model::Addressable::contract_id) {
      static const Ttx::Model::Addressable::Operations operations = {
        [](const void* source) -> Ttx::Concept::Abstract::Handle {
          const auto& field = *static_cast<const Field*>(source);
          if (field.type_reference) {
            return field.type_reference->get_interface();
          }
          return field.get_type().get_interface();
        },
      };
      return Ttx::Semantic::Binding::provide<Ttx::Model::Addressable>(
          this, operations);
    }
    return Model::Memory::bind_interface(requested);
  }

  // Source interpretation supplies the declaration facts it could establish
  // from the authored form. Keeping construction independent from Cursor lets
  // the same Field model participate in another Dialect without borrowing its
  // grammar machinery.
  static auto create_authored(
      Perimortem::Memory::Allocator::Arena& domain,
      Tetrodotoxin::Language::Definition& definition,
      Writability writability,
      Perimortem::Core::Option<TypeReference> type_reference,
      Perimortem::Core::Option<Model::Pack&> initializer) -> Field&;

  static auto create(
      Perimortem::Memory::Allocator::Arena& domain,
      Tetrodotoxin::Language::Definition& definition,
      Writability writability,
      Perimortem::Core::Option<TypeReference> type_reference,
      Perimortem::Core::Option<Model::Pack&> initializer) -> Field&;

  static auto create_generated(
      Perimortem::Memory::Allocator::Arena& domain,
      Tetrodotoxin::Language::Definition& definition,
      Writability writability,
      const Model::Type& type) -> Field&;

  static auto create_generated(
      Perimortem::Memory::Allocator::Arena& domain,
      Tetrodotoxin::Language::Definition& definition,
      Writability writability,
      TypeReference type_reference) -> Field&;

  auto retain_generated_type(const Model::Type& selected) -> Bool;

  // An Interface default is already linked by its declaration owner before a
  // concrete implementation adopts that exact Pack. The generated Field keeps
  // one real initializer edge without reparsing or copying the expression.
  auto retain_generated_initializer(const Field& requirement) -> Bool;

  auto link_declaration_type(Ttx::Lexical::Cursor& cursor) -> Bool;

  auto link_inferred_declaration_type(Ttx::Lexical::Cursor& cursor) -> Bool;

  Field(const Field&) = delete;
  Field(Field&&) = delete;
  auto operator=(const Field&) -> Field& = delete;
  auto operator=(Field&&) -> Field& = delete;

  auto link_declaration_initializer(Ttx::Lexical::Cursor& cursor) -> Bool;

  auto link_restored_declaration_type() -> Bool;

  auto link_restored_declaration_initializer() -> Bool;

  // Const completion is a required link barrier. The initializer must reduce
  // to one exact constant Pack before any body can consume this Field.
  auto link_declaration_constant(Ttx::Lexical::Cursor& cursor) const -> Bool;

  // Finalization visits the real initializer Pack after linking has frozen
  // its output Layout. Field remains the declaration owner. No Expression
  // side inventory is required merely to cache constant producers.
  auto finalize_declaration(Ttx::Lexical::Cursor& cursor) -> Bool;

  constexpr auto contributes_to_instance_layout() const -> Bool override {
    return writability == Writability::Internal;
  }

  constexpr auto permits_write_from(const Model::Type& access_scope) const
      -> Bool override {
    switch (writability) {
    case Writability::Full:
      return True;
    case Writability::Internal:
      return Bool(
          get_definition().get_visibility() ==
              Tetrodotoxin::Language::Visibility::Public ||
          access_scope.has_private_access_to(get_host()));
    case Writability::Constant:
      return False;
    }
    return False;
  }

  TTX_DOCUMENTATION(get_definition().get_documentation());
  TTX_NAME(definition.get_name());

  constexpr auto get_definition() const
      -> const Tetrodotoxin::Language::Definition& {
    return definition;
  }

  constexpr auto get_anchor() const -> Ttx::Lexical::Anchor {
    return definition.get_anchor();
  }

  constexpr auto get_declaration_anchor() const
      -> Perimortem::Core::Option<Ttx::Lexical::Anchor> {
    return definition.get_declaration_anchor();
  }

  auto resolve_concept(Perimortem::Core::View::Bytes route) const
      -> const Ttx::Concept::Abstract& override;

  auto resolve() const -> const Ttx::Concept::Abstract& override;

  auto get_type() const -> const Ttx::Concept::Abstract& override;

  auto get_type_reference() const
      -> Perimortem::Core::Option<const TypeReference&> {
    return type_reference.visit(
        []() -> Perimortem::Core::Option<const TypeReference&> { return {}; },
        [](const TypeReference& selected)
            -> Perimortem::Core::Option<const TypeReference&> {
          return selected;
        });
  }

  constexpr auto get_writability() const -> Writability { return writability; }

  auto get_type_anchor() const
      -> Perimortem::Core::Option<Ttx::Lexical::Anchor> {
    return type_reference.visit(
        []() -> Perimortem::Core::Option<Ttx::Lexical::Anchor> { return {}; },
        [](const TypeReference& selected)
            -> Perimortem::Core::Option<Ttx::Lexical::Anchor> {
          return selected.get_anchor();
        });
  }

  constexpr auto get_host() const -> const Model::Type& {
    return static_cast<const Model::Type&>(get_definition().get_host());
  }

  auto get_initializer() const -> Perimortem::Core::Option<const Model::Pack&>;

  // Const Fields are declaration owned compile time values. They never denote
  // per instance storage, regardless of which valid receiver selects them.
  auto get_constant() const -> Perimortem::Core::Option<Model::Pack&> override;

  constexpr auto is_linked() const -> Bool {
    return Bool(type) && initializer_linked;
  }

 private:
  friend class Tetrodotoxin::Source::Declaration;
  auto get_domain() const -> Ttx::Concept::Domain::Answer override {
    if (type_reference) {
      return type_reference->get_interface();
    }
    return Model::Memory::get_domain();
  }

  auto complete_source(
      Tetrodotoxin::Source::Declaration::Phase phase,
      Ttx::Lexical::Cursor* cursor)
      -> Tetrodotoxin::Source::Declaration::Completion;

  enum class ConstantState : U8 {
    Unresolved,
    Folding,
    Folded,
    Failed,
  };

  auto cache_constant() const -> Bool;

  auto validate_publication(Ttx::Lexical::Cursor& cursor) const -> Bool;

  Tetrodotoxin::Language::Definition& definition;
  Perimortem::Memory::Allocator::Arena& domain;
  Writability writability;
  Perimortem::Core::Option<TypeReference> type_reference;
  Perimortem::Core::Option<Model::Pack&> initializer;
  Perimortem::Core::Option<Ttx::Concept::Reference<const Model::Type>> type;
  Bool generated;
  mutable Perimortem::Core::Option<Ttx::Model::PackReference<Model::Pack>>
      constant;
  mutable ConstantState constant_state = ConstantState::Unresolved;
  Bool initializer_linked;
};

}  // namespace Tetrodotoxin::Library::Language
