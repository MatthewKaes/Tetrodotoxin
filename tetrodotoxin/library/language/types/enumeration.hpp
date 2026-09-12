// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/option.hpp"

#include "perimortem/memory/allocator/arena.hpp"
#include "perimortem/memory/managed/vector.hpp"

#include "tetrodotoxin/language/definition.hpp"
#include "tetrodotoxin/library/language/model/memory.hpp"
#include "tetrodotoxin/library/language/model/type.hpp"
#include "tetrodotoxin/library/language/type_reference.hpp"
#include "ttx/concept/reference.hpp"
#include "ttx/lexical/anchor.hpp"
#include "ttx/lexical/cursor.hpp"

namespace Tetrodotoxin::Library::Language::Types {

// Enumeration is one authored Library Type whose cases are named immutable
// values. It keeps source facts private until one exact integer storage Type
// and every named Constant declaration are complete.
class Enumeration : public Model::Type {
 public:
  // Case retains exactly the authored spelling and Documentation needed to
  // create the immutable value after storage linking. It is source model data
  // rather than an intermediate parser record.
  struct Case {
    Perimortem::Core::View::Bytes name;
    Perimortem::Core::View::Bytes value;
    const Ttx::Concept::Documentation& documentation;
    Ttx::Lexical::Anchor anchor;
    Ttx::Lexical::Anchor name_anchor;
    Ttx::Lexical::Anchor value_anchor;
  };

 private:
  Enumeration(
      Perimortem::Memory::Allocator::Arena& domain,
      Tetrodotoxin::Language::Definition& definition,
      TypeReference storage_reference);

 public:
  TTX_CONTRACT(Enumeration, Model::Type);

  auto bind_interface(Perimortem::System::Uuid requested) const
      -> Perimortem::Utility::Result<
          Ttx::Semantic::Binding,
          Ttx::Semantic::Binding::Failure> override {
    if (requested == Tetrodotoxin::Language::Definition::contract_id) {
      return Tetrodotoxin::Language::Definition::provide(*this);
    }
    return Model::Type::bind_interface(requested);
  }

  static auto create_authored(
      Perimortem::Memory::Allocator::Arena& domain,
      Tetrodotoxin::Language::Definition& definition,
      TypeReference storage_reference,
      Perimortem::Core::View::Vector<Case> cases) -> Enumeration&;

  static auto create(
      Perimortem::Memory::Allocator::Arena& domain,
      Tetrodotoxin::Language::Definition& definition,
      TypeReference storage_reference,
      Perimortem::Core::View::Vector<Case> cases) -> Enumeration&;

  Enumeration(const Enumeration&) = delete;
  Enumeration(Enumeration&&) = delete;
  auto operator=(const Enumeration&) -> Enumeration& = delete;
  auto operator=(Enumeration&&) -> Enumeration& = delete;

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

  constexpr auto get_declaration_anchor() const
      -> Perimortem::Core::Option<Ttx::Lexical::Anchor> override {
    return definition.get_declaration_anchor();
  }

  TTX_NAME(definition.get_name());
  TTX_DOCUMENTATION(definition.get_documentation());

  auto link_types(Ttx::Lexical::Cursor& cursor) -> Bool override;
  auto finalize(Ttx::Lexical::Cursor& cursor) -> Bool override;

  auto link_restored_types() -> Bool override;

  auto finalize_restored() -> Bool override;

  auto resolve() const -> const Ttx::Concept::Abstract& override;

  auto resolve_concept(Perimortem::Core::View::Bytes route) const
      -> const Ttx::Concept::Abstract& override;

  auto create_default(Perimortem::Memory::Allocator::Arena& arena) const
      -> Perimortem::Core::Option<Model::Pack&> override;

  auto accepts_iteration(const Ttx::Concept::Layout& bindings) const
      -> Bool override;

  auto get_storage_type() const -> Perimortem::Core::Option<const Model::Type&>;

  constexpr auto get_storage_reference() const -> const TypeReference& {
    return storage_reference;
  }

  auto get_cases() const -> Perimortem::Core::View::Vector<
      Ttx::Concept::Reference<const Ttx::Concept::Abstract>>;

  auto visit_concepts(Ttx::Concept::Abstract::Visitor visitor) const
      -> void override;

  constexpr auto get_case_count() const -> Count {
    return source_cases.get_size();
  }

  auto get_case_value(Count index) const -> Perimortem::Core::Option<U64>;

  auto get_case_name(Count index) const -> Perimortem::Core::View::Bytes;

  auto retain_restored_case(
      Perimortem::Core::View::Bytes name,
      U64 value,
      const Ttx::Concept::Documentation& documentation) -> Bool;

  auto find_case_name(U64 value) const -> Perimortem::Core::View::Bytes;

 private:
  // A case is a real declaration of an enum value. Its authored facts belong
  // to Definition, while the constant supplies its value and Type questions.
  // Keeping that declaration here preserves labels without teaching transparent
  // Alias to retain names or introducing another member registry.
  class Member : public Ttx::Concept::Abstract {
   public:
    constexpr Member(
        Tetrodotoxin::Language::Definition& definition,
        const Ttx::Concept::Abstract& value)
        : definition(definition), value(value) {}

    TTX_NAME(definition.get_name());
    TTX_DOCUMENTATION(definition.get_documentation());

    constexpr auto get_definition() const
        -> const Tetrodotoxin::Language::Definition& {
      return definition;
    }
    auto resolve() const -> const Ttx::Concept::Abstract& override {
      return value.get().resolve();
    }
    auto get_type() const -> const Ttx::Concept::Abstract& override {
      return value.get().get_type();
    }
    auto resolve_concept(Perimortem::Core::View::Bytes name) const
        -> const Ttx::Concept::Abstract& override {
      return value.get().resolve_concept(name);
    }
    auto visit_concepts(Ttx::Concept::Abstract::Visitor visitor) const
        -> void override {
      value.get().visit_concepts(visitor);
    }
    auto bind_interface(Perimortem::System::Uuid requested) const
        -> Perimortem::Utility::Result<
            Ttx::Semantic::Binding,
            Ttx::Semantic::Binding::Failure> override {
      if (requested == Tetrodotoxin::Language::Definition::contract_id) {
        return Tetrodotoxin::Language::Definition::provide(*this);
      }
      return value.get().bind_interface(requested);
    }

   private:
    Tetrodotoxin::Language::Definition& definition;
    Ttx::Concept::Reference<const Ttx::Concept::Abstract> value;
  };

  // Static lookup has its own subject while the Enumeration keeps its cases
  // and completion state. The scope borrows those facts directly, so exposing
  // it requires no second case inventory or publication lifecycle.
  class Authority : public Ttx::Concept::Abstract {
   public:
    constexpr explicit Authority(const Enumeration& owner) : owner(owner) {}

    TTX_CONTRACT(Authority, Ttx::Concept::Abstract);
    TTX_NAME("static"_view);
    TTX_EMPTY_DOCUMENTATION();

    auto resolve_concept(Perimortem::Core::View::Bytes name) const
        -> const Ttx::Concept::Abstract& override;
    auto visit_concepts(Ttx::Concept::Abstract::Visitor visitor) const
        -> void override;

   private:
    const Enumeration& owner;
  };

  Authority static_scope;
  enum class Stage : ::U8 {
    Authored,
    StorageLinked,
    Finalized,
  };

  Tetrodotoxin::Language::Definition& definition;
  Perimortem::Memory::Allocator::Arena& domain;
  TypeReference storage_reference;
  Perimortem::Memory::Managed::Vector<Case> source_cases;
  Perimortem::Core::Option<Ttx::Concept::Reference<const Model::Type>>
      storage_type;
  Perimortem::Memory::Managed::Vector<
      Ttx::Concept::Reference<const Ttx::Concept::Abstract>>
      cases;
  Perimortem::Core::Option<Ttx::Concept::Reference<const Model::Memory>>
      generated_size;
  Stage stage = Stage::Authored;
};

}  // namespace Tetrodotoxin::Library::Language::Types
