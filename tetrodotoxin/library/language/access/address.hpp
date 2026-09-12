// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/option.hpp"

#include "perimortem/memory/allocator/arena.hpp"

#include "tetrodotoxin/library/language/expression.hpp"
#include "tetrodotoxin/library/language/model/memory.hpp"
#include "ttx/concept/reference.hpp"
#include "ttx/lexical/cursor.hpp"

namespace Tetrodotoxin::Library::Language::Access {

// Address evaluates one receiver and selects one Addressable from the category
// supplied by that exact result. The selected address may use Static storage or
// a receiver base, while target generation owns its concrete representation.
class Address : public Expression {
 public:
  TTX_CONTRACT(Address, Expression);

  static auto create_authored(
      Perimortem::Memory::Allocator::Arena& domain,
      Model::Pack& receiver,
      Ttx::Lexical::Token name_token,
      Perimortem::Core::View::Bytes name,
      Ttx::Lexical::Anchor anchor) -> Address&;

  static auto create_synthetic(
      Perimortem::Memory::Allocator::Arena& domain,
      Model::Pack& receiver,
      const Model::Memory& addressable) -> Address&;

  auto link(
      Ttx::Lexical::Cursor& cursor,
      const Ttx::Concept::Abstract& lexical_context,
      Perimortem::Core::Option<const Ttx::Concept::Abstract&> access_scope = {})
      -> Bool override;

  TTX_NAME(name);

  auto get_documentation() const -> const Ttx::Concept::Documentation& override;
  auto get_type() const -> const Ttx::Concept::Abstract& override;
  auto get_result() const -> const Ttx::Concept::Abstract& override;
  auto finalize(Ttx::Lexical::Cursor& cursor) -> void override;

  constexpr auto get_receiver() const -> const Model::Pack& { return receiver; }

  constexpr auto get_name_token() const -> Ttx::Lexical::Token {
    return name_token;
  }

 private:
  constexpr Address(
      Model::Pack& receiver,
      Ttx::Lexical::Token name_token,
      Perimortem::Core::View::Bytes name,
      Perimortem::Core::Option<
          Ttx::Concept::Reference<const Model::Memory>> addressable,
      Perimortem::Core::Option<Ttx::Lexical::Anchor> anchor)
      : Expression(anchor),
        receiver(receiver),
        name_token(name_token),
        name(name),
        addressable(addressable) {}

  Model::Pack& receiver;
  Ttx::Lexical::Token name_token;
  Perimortem::Core::View::Bytes name;
  Perimortem::Core::Option<Ttx::Concept::Reference<const Model::Memory>>
      addressable;
};

}  // namespace Tetrodotoxin::Library::Language::Access
