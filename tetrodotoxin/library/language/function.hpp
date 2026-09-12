// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/option.hpp"

#include "perimortem/memory/allocator/arena.hpp"

#include "tetrodotoxin/language/definition.hpp"
#include "tetrodotoxin/library/language/flow/block.hpp"
#include "tetrodotoxin/library/language/model/callable.hpp"
#include "tetrodotoxin/library/language/monograph.hpp"
#include "tetrodotoxin/library/language/signature.hpp"
#include "ttx/lexical/cursor.hpp"

namespace Tetrodotoxin::Library::Language {

// Function is one completely parsed Library Callable. Definition supplies its
// exact host context, Signature supplies its callable shape, and one authored
// Block supplies its body. A leading self parameter records receiver
// invocation and has that exact host Type.
class Function : public Model::Callable {
 private:
  Function(
      Tetrodotoxin::Language::Definition& definition,
      Signature& signature);

 public:
  TTX_CONTRACT(Function, Model::Callable);

  auto bind_interface(Perimortem::System::Uuid requested) const
      -> Perimortem::Utility::Result<
          Ttx::Semantic::Binding,
          Ttx::Semantic::Binding::Failure> override {
    if (requested == Tetrodotoxin::Language::Definition::contract_id) {
      return Tetrodotoxin::Language::Definition::provide(*this);
    }
    return Model::Callable::bind_interface(requested);
  }

  static auto create_authored(
      Perimortem::Memory::Allocator::Arena& domain,
      Tetrodotoxin::Language::Definition& definition,
      Signature& signature) -> Function&;

  static auto create(
      Perimortem::Memory::Allocator::Arena& domain,
      Tetrodotoxin::Language::Definition& definition,
      Signature& signature) -> Function&;

  // Function identity is required while its Block is interpreted because the
  // body uses that exact Callable for result and receiver context. Completion
  // binds the one resulting Block without retaining parser state.
  auto complete_body(Flow::Block& selected) -> Bool;

  Function(const Function&) = delete;
  Function(Function&&) = delete;
  auto operator=(const Function&) -> Function& = delete;
  auto operator=(Function&&) -> Function& = delete;

  auto link_declaration_signature(Ttx::Lexical::Cursor& cursor)
      -> Bool override;

  auto link_declaration_body(Ttx::Lexical::Cursor& cursor) -> Bool override;

  auto link_restored_declaration_signature() -> Bool override;

  auto finalize_declaration(Ttx::Lexical::Cursor& cursor) -> Bool override;

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
      -> Perimortem::Core::Option<Ttx::Lexical::Anchor> override {
    return definition.get_declaration_anchor();
  }

  auto resolve() const -> const Ttx::Concept::Abstract& override;

  auto resolve_concept(Perimortem::Core::View::Bytes route) const
      -> const Ttx::Concept::Abstract& override;

  auto get_parameters() const -> const Ttx::Concept::Layout& override;

  auto get_results() const -> const Ttx::Concept::Layout& override;

  constexpr auto get_parameter_anchor(Count index) const {
    return signature.get_parameters().get_slot_anchor(index);
  }

  constexpr auto get_host() const -> const Model::Type& {
    return static_cast<const Model::Type&>(get_definition().get_host());
  }

  constexpr auto get_signature() const -> const Signature& { return signature; }
  constexpr auto edit_signature() -> Signature& { return signature; }

  // Before Signature linking publishes the receiver Addressable, registration
  // still needs the authored receiver role. This query derives it from the
  // retained Signature shape. Callable::is_type_bound() becomes authoritative
  // once the parameter Layout exists.
  auto declares_self() const -> Bool override;

  auto get_body() const -> Perimortem::Core::Option<const Flow::Block&>;

 private:
  auto is_signature_linked() const -> Bool;

  Tetrodotoxin::Language::Definition& definition;
  Signature& signature;
  Perimortem::Core::Option<Flow::Block&> body;
};

}  // namespace Tetrodotoxin::Library::Language
