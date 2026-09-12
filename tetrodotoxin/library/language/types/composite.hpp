// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/selection.hpp"
#include "perimortem/core/option.hpp"

#include "perimortem/memory/managed/vector.hpp"

#include "tetrodotoxin/language/definition.hpp"
#include "tetrodotoxin/language/monograph.hpp"
#include "tetrodotoxin/language/visibility.hpp"
#include "tetrodotoxin/library/language/access/instance.hpp"
#include "tetrodotoxin/library/language/access/static.hpp"
#include "tetrodotoxin/library/language/model/type.hpp"
#include "ttx/concept/reference.hpp"
#include "ttx/lexical/anchor.hpp"
#include "ttx/lexical/cursor.hpp"
#include "ttx/model/layouts/named.hpp"

namespace Tetrodotoxin::Library::Language::Types {

// Composite owns the member inventories, lookup categories, instance Layout,
// and completion lifecycle shared by Source, Structure, and Object. Each
// concrete Type supplies its own presentation and authored semantics.
class Composite : public Model::Type {
 public:
  // Category names the three independent declaration spaces owned by a
  // Composite. It is transaction input, not a property recovered from an
  // Alias. Forward parsing or an imported provider already proves the space,
  // so an opaque name can enter it before its target graph completes.
  enum class Category : ::U8 {
    Addressable,
    Callable,
    Type,
  };

 protected:
  Composite(
      Perimortem::Memory::Allocator::Arena& domain,
      Tetrodotoxin::Language::Definition& definition);

  auto can_accept_definition() const -> Bool;

  auto can_bind_definition(
      const Ttx::Concept::Abstract& binding,
      Category category) const -> Bool;

  auto publish_binding(
      Ttx::Concept::Abstract& binding,
      Category category,
      Bool published,
      Bool persistent = True,
      Bool prepend = False) -> Bool;

  auto resolve_binding(
      Perimortem::Core::View::Bytes route,
      Category category,
      Tetrodotoxin::Language::Visibility visibility =
          Tetrodotoxin::Language::Visibility::Private) const
      -> const Ttx::Concept::Abstract&;

  virtual auto retain_binding(
      Ttx::Concept::Abstract& binding,
      Tetrodotoxin::Language::Definition& definition,
      Category category,
      Ttx::Lexical::Cursor& cursor) -> Bool;

  auto complete_field_layout() -> void;

  constexpr auto get_domain() const -> Perimortem::Memory::Allocator::Arena& {
    return domain;
  }

  // Source owns the closure barrier. These tree operations settle forward
  // Alias routes without making an Alias discover or complete its siblings.
  auto link_aliases() -> Count override;
  auto validate_aliases(Ttx::Lexical::Cursor& cursor) const -> Bool override;

 public:
  TTX_CONTRACT(Composite, Model::Type);

  auto bind_interface(Perimortem::System::Uuid requested) const
      -> Perimortem::Utility::Result<
          Ttx::Semantic::Binding,
          Ttx::Semantic::Binding::Failure> override {
    if (requested == Tetrodotoxin::Language::Definition::contract_id) {
      return Tetrodotoxin::Language::Definition::provide(*this);
    }
    return Model::Type::bind_interface(requested);
  }

  Composite(const Composite&) = delete;
  Composite(Composite&&) = delete;
  auto operator=(const Composite&) -> Composite& = delete;
  auto operator=(Composite&&) -> Composite& = delete;

  // Source interpretation selects the declaration category from authored
  // grammar. Composite applies its one registration and collision policy to
  // that real identity without learning how the declaration was parsed.
  auto retain_authored_definition(
      Ttx::Concept::Abstract& binding,
      Tetrodotoxin::Language::Definition& definition,
      Category category,
      Ttx::Lexical::Cursor& cursor) -> Bool;

  // A producer that has already validated category and visibility can retain
  // the same semantic identity without borrowing source interpretation.
  auto retain_definition(
      Ttx::Concept::Abstract& binding,
      Category category,
      Bool published) -> Bool;

  virtual auto complete_body() -> void { complete_field_layout(); }

  constexpr auto get_definition() const
      -> const Tetrodotoxin::Language::Definition& {
    return definition;
  }

  constexpr auto get_host() -> Ttx::Concept::Abstract& {
    return definition.get_host();
  }

  constexpr auto get_host() const -> const Ttx::Concept::Abstract& {
    return definition.get_host();
  }

  constexpr auto get_anchor() const -> Ttx::Lexical::Anchor {
    return definition.get_anchor();
  }

  TTX_NAME(definition.get_name());
  TTX_DOCUMENTATION(definition.get_documentation());

  // A caller carries private authority only through its exact Definition host
  // chain. The chain authenticates access without becoming a semantic parent
  // route or supplying an implicit receiver.
  auto has_private_access_to(const Model::Type& owner) const -> Bool override;

  auto is_externally_reachable(const Model::Type& type) const -> Bool override;

  // Declaration Types settle recursively before any Composite in the same
  // closure may complete Field Type edges.
  auto link_types(Ttx::Lexical::Cursor& cursor) -> Bool override;
  auto link_fields(Ttx::Lexical::Cursor& cursor) -> Bool override;
  auto validate_layout(Ttx::Lexical::Cursor& cursor) const -> Bool override;

  auto validate_layout_restored() const -> Bool;
  auto link_initializers(Ttx::Lexical::Cursor& cursor) -> Bool override;
  auto link_callable_signatures(Ttx::Lexical::Cursor& cursor) -> Bool override;
  auto link_callable_bodies(Ttx::Lexical::Cursor& cursor) -> Bool override;
  auto finalize(Ttx::Lexical::Cursor& cursor) -> Bool override;

  auto link_restored_types() -> Bool override;

  auto link_restored_callable_signatures() -> Bool override;

  auto link_restored_fields() -> Bool override;

  auto link_restored_initializers() -> Bool override;

  auto finalize_restored() -> Bool override;

  constexpr auto get_declaration_anchor() const
      -> Perimortem::Core::Option<Ttx::Lexical::Anchor> override {
    return definition.get_declaration_anchor();
  }

  auto resolve() const -> const Ttx::Concept::Abstract& override;

  // An explicit context query exposes only public Type names. A missing local
  // name forwards outward, but a selected Composite never lends private
  // declaration authority to the remainder of a qualified route.
  auto resolve_concept(Perimortem::Core::View::Bytes route) const
      -> const Ttx::Concept::Abstract& override;

  auto visit_concepts(Ttx::Concept::Abstract::Visitor visitor) const
      -> void override;

  virtual auto resolve_public_context(Perimortem::Core::View::Bytes route) const
      -> const Ttx::Concept::Abstract&;

  // Declaration owners use this one name query for their unqualified root.
  // Actual containment grants local access without attaching authority to any
  // later segment selected by TypeReference.
  auto resolve_lexical_context(Perimortem::Core::View::Bytes route) const
      -> const Ttx::Concept::Abstract& override;

  auto get_layout() const -> const Ttx::Model::Layouts::Named& override;

  auto get_addressables(
      Tetrodotoxin::Language::Visibility visibility =
          Tetrodotoxin::Language::Visibility::Private) const {
    auto selected = visibility == Tetrodotoxin::Language::Visibility::Private
                        ? addressables.get_view()
                        : published_addressables.get_view();
    return Perimortem::Core::View::Selection(selected);
  }

  auto get_types(
      Tetrodotoxin::Language::Visibility visibility =
          Tetrodotoxin::Language::Visibility::Private) const {
    auto selected = visibility == Tetrodotoxin::Language::Visibility::Private
                        ? types.get_view()
                        : published_types.get_view();
    return Perimortem::Core::View::Selection(selected);
  }

  constexpr auto get_declarations() const {
    return Perimortem::Core::View::Selection(declarations.get_view());
  }

  auto is_published(const Ttx::Concept::Abstract& declaration) const -> Bool;

  constexpr auto get_static_authority() const
      -> const Tetrodotoxin::Library::Language::Access::Static& {
    return static_authority;
  }

  constexpr auto get_instance_authority() const
      -> const Tetrodotoxin::Library::Language::Access::Instance& {
    return instance_authority;
  }

  constexpr auto is_linked() const -> Bool {
    return stage >= Stage::FieldsLinked;
  }

  constexpr auto is_finalized() const -> Bool {
    return stage == Stage::Finalized;
  }

 private:
  enum class Stage : ::U8 {
    Authored,
    TypesLinked,
    CallableSignaturesLinked,
    FieldsLinked,
    InitializersLinked,
    CallablesLinked,
    Finalized,
  };

  Tetrodotoxin::Language::Definition& definition;
  Perimortem::Memory::Allocator::Arena& domain;
  Tetrodotoxin::Library::Language::Access::Static& static_authority;
  Tetrodotoxin::Library::Language::Access::Instance& instance_authority;
  Perimortem::Memory::Managed::Vector<
      Ttx::Concept::Reference<Ttx::Concept::Abstract>>
      addressables;
  Perimortem::Memory::Managed::Vector<
      Ttx::Concept::Reference<Ttx::Concept::Abstract>>
      published_addressables;
  Perimortem::Memory::Managed::Vector<
      Ttx::Concept::Reference<Ttx::Concept::Abstract>>
      types;
  Perimortem::Memory::Managed::Vector<
      Ttx::Concept::Reference<Ttx::Concept::Abstract>>
      published_types;
  Perimortem::Memory::Managed::Vector<
      Ttx::Concept::Reference<Ttx::Concept::Abstract>>
      declarations;
  Perimortem::Core::Option<const Ttx::Model::Layouts::Named&> layout;
  Stage stage = Stage::Authored;
};

}  // namespace Tetrodotoxin::Library::Language::Types
