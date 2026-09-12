// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "tetrodotoxin/language/definition.hpp"
#include "tetrodotoxin/language/type_reference.hpp"
#include "ttx/concept/abstract.hpp"
#include "ttx/concept/unknown.hpp"
#include "ttx/model/type.hpp"

namespace Tetrodotoxin::Render::Language {

// Render owns the authored declaration around this Type relationship. Its
// name and documentation survive independently of the reference's answer,
// so transparent Alias is composed behavior rather than a native base class.
class Alias : public Ttx::Concept::Abstract {
 public:
  TTX_CONTRACT(Alias, Ttx::Concept::Abstract);
  TTX_NAME(definition.get_name());
  TTX_DOCUMENTATION(definition.get_documentation());

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

  static auto create(
      Perimortem::Memory::Allocator::Arena& domain,
      Tetrodotoxin::Language::Definition& definition,
      Tetrodotoxin::Language::TypeReference target) -> Alias&;

  auto link(Ttx::Lexical::Cursor& cursor, const Ttx::Concept::Abstract& context)
      -> Bool;

  auto link_restored(const Ttx::Concept::Abstract& context) -> Bool;

  constexpr auto get_definition() const
      -> const Tetrodotoxin::Language::Definition& {
    return definition;
  }

  constexpr auto get_target_reference() const
      -> const Tetrodotoxin::Language::TypeReference& {
    return target;
  }

 private:
  Alias(
      Tetrodotoxin::Language::Definition& definition,
      Tetrodotoxin::Language::TypeReference target)
      : definition(definition), target(target) {}

  Tetrodotoxin::Language::Definition& definition;
  Tetrodotoxin::Language::TypeReference target;
  Perimortem::Core::Option<Ttx::Concept::Reference<const Ttx::Model::Type>>
      selected_type;
};

}  // namespace Tetrodotoxin::Render::Language
