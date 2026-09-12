// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/option.hpp"

#include "perimortem/memory/allocator/arena.hpp"

#include "tetrodotoxin/language/definition.hpp"
#include "tetrodotoxin/library/language/type_reference.hpp"
#include "ttx/concept/abstract.hpp"
#include "ttx/lexical/cursor.hpp"
#include "ttx/model/type.hpp"

namespace Tetrodotoxin::Library::Language {

// This declaration owns the authored name, documentation, and route selecting
// a Type. Those facts remain visible even before the selected Type completes.
// They cannot belong to transparent TTX Alias, whose entire surface is the
// referent's answer. The declaration therefore retains its own Definition and
// delegates other questions through its TypeReference, where the supplying
// policy remains visible. Its borrowed native Type is the separate answer
// needed by compilation, rather than a shortcut for that delegation.
class Alias : public Ttx::Concept::Abstract {
 private:
  constexpr Alias(
      Perimortem::Memory::Allocator::Arena& domain,
      Tetrodotoxin::Language::Definition& definition,
      TypeReference target_reference)
      : definition(definition),
        domain(domain),
        target_reference(target_reference) {}

 public:
  TTX_CONTRACT(Alias, Ttx::Concept::Abstract);
  TTX_NAME(definition.get_name());

  auto resolve() const -> const Ttx::Concept::Abstract& override;
  auto get_type() const -> const Ttx::Concept::Abstract& override;
  auto resolve_concept(Perimortem::Core::View::Bytes name) const
      -> const Ttx::Concept::Abstract& override;
  auto visit_concepts(Ttx::Concept::Abstract::Visitor visitor) const
      -> void override;
  auto bind_interface(Perimortem::System::Uuid requested) const
      -> Perimortem::Utility::Result<
          Ttx::Semantic::Binding,
          Ttx::Semantic::Binding::Failure> override;

  static auto create_authored(
      Perimortem::Memory::Allocator::Arena& domain,
      Tetrodotoxin::Language::Definition& definition,
      TypeReference target_reference) -> Alias&;

  static auto create(
      Perimortem::Memory::Allocator::Arena& domain,
      Tetrodotoxin::Language::Definition& definition,
      TypeReference target_reference) -> Alias&;

  auto get_documentation() const -> const Ttx::Concept::Documentation& override;

  constexpr auto get_definition() const
      -> const Tetrodotoxin::Language::Definition& {
    return definition;
  }

  constexpr auto get_anchor() const -> Ttx::Lexical::Anchor {
    return definition.get_anchor();
  }

  // Source orders Alias completion across its whole declaration tree. Alias
  // itself resolves its retained route and retains the resulting TTX Type. A
  // value consumer separately proves the narrower Library Type protocol.
  auto link() -> Bool;
  auto report_unresolved(Ttx::Lexical::Cursor& cursor) const -> void;

  constexpr auto get_target_reference() const -> const TypeReference& {
    return target_reference;
  }

  constexpr auto is_linked() const -> Bool { return linked; }

 private:
  Tetrodotoxin::Language::Definition& definition;
  Perimortem::Memory::Allocator::Arena& domain;
  TypeReference target_reference;
  Perimortem::Core::Option<const Ttx::Concept::Documentation&> documentation;
  Perimortem::Core::Option<Ttx::Concept::Reference<const Ttx::Model::Type>>
      target;
  Bool linked = False;
};

}  // namespace Tetrodotoxin::Library::Language
