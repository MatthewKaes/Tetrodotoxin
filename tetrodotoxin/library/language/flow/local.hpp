// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/option.hpp"

#include "perimortem/memory/allocator/arena.hpp"

#include "tetrodotoxin/library/language/constant.hpp"
#include "tetrodotoxin/library/language/flow/block.hpp"
#include "tetrodotoxin/library/language/model/memory.hpp"
#include "tetrodotoxin/library/language/model/pack.hpp"
#include "tetrodotoxin/library/language/type_reference.hpp"
#include "tetrodotoxin/library/language/writability.hpp"
#include "ttx/concept/reference.hpp"
#include "ttx/lexical/anchor.hpp"
#include "ttx/lexical/cursor.hpp"

namespace Tetrodotoxin::Library::Language::Flow {

// Local is one Addressable declaration retained directly by its lexical Block.
// It owns its source name, declared or inferred Type, and initializer Pack
// without acquiring member Visibility or a shared Definition prefix. The
// declared Type receives the complete Pack Layout. Inference is deliberately
// limited to one scalar output because a Local cannot silently materialize a
// new aggregate Type for composed flow.
class Local : public Model::Memory {
 public:
  TTX_CONTRACT(Local, Model::Memory);

  static auto create_authored(
      Perimortem::Memory::Allocator::Arena& domain,
      Block& host,
      Ttx::Lexical::Token name_token,
      Perimortem::Core::View::Bytes name,
      Writability writability,
      Perimortem::Core::Option<TypeReference> type_reference,
      Perimortem::Core::Option<Model::Pack&> initializer,
      Ttx::Lexical::Anchor anchor) -> Local&;

  Local(const Local&) = delete;
  Local(Local&&) = delete;
  auto operator=(const Local&) -> Local& = delete;
  auto operator=(Local&&) -> Local& = delete;

  auto link(Ttx::Lexical::Cursor& cursor, const Model::Type& access_scope)
      -> Bool;

  auto finalize(Ttx::Lexical::Cursor& cursor) -> void;

  TTX_NAME(name);

  auto get_documentation() const -> const Ttx::Concept::Documentation& override;

  auto resolve() const -> const Ttx::Concept::Abstract& override;

  auto get_type() const -> const Ttx::Concept::Abstract& override;

  constexpr auto get_linked_type() const
      -> Perimortem::Core::Option<const Model::Type&> {
    return type.visit(
        []() -> Perimortem::Core::Option<const Model::Type&> { return {}; },
        [](const Ttx::Concept::Reference<const Model::Type>& selected)
            -> Perimortem::Core::Option<const Model::Type&> {
          return selected.get();
        });
  }

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

  constexpr auto permits_write_from(const Model::Type&) const -> Bool override {
    return writability == Writability::Full;
  }

  constexpr auto get_anchor() const -> Ttx::Lexical::Anchor { return anchor; }

  constexpr auto get_initializer() const
      -> Perimortem::Core::Option<const Model::Pack&> {
    return initializer.visit(
        []() -> Perimortem::Core::Option<const Model::Pack&> { return {}; },
        [](Model::Pack& selected)
            -> Perimortem::Core::Option<const Model::Pack&> {
          return selected;
        });
  }

  auto get_constant() const -> Perimortem::Core::Option<Model::Pack&> override;

 private:
  enum class ConstantState : U8 {
    Unresolved,
    Folding,
    Folded,
    Failed,
  };

  auto link_constant(Ttx::Lexical::Cursor& cursor) const -> Bool;
  auto cache_constant() const -> Bool;

  constexpr Local(
      Perimortem::Memory::Allocator::Arena& domain,
      Block& host,
      Ttx::Lexical::Token name_token,
      Perimortem::Core::View::Bytes name,
      Writability writability,
      Perimortem::Core::Option<TypeReference> type_reference,
      Perimortem::Core::Option<Model::Pack&> initializer,
      Ttx::Lexical::Anchor anchor)
      : domain(domain),
        host(host),
        name_token(name_token),
        name(name),
        writability(writability),
        type_reference(type_reference),
        initializer(initializer),
        anchor(anchor),
        initializer_linked(!initializer) {}

  Perimortem::Memory::Allocator::Arena& domain;
  Block& host;
  Ttx::Lexical::Token name_token;
  Perimortem::Core::View::Bytes name;
  Writability writability;
  Perimortem::Core::Option<TypeReference> type_reference;
  Perimortem::Core::Option<Model::Pack&> initializer;
  Perimortem::Core::Option<Ttx::Concept::Reference<const Model::Type>> type;
  mutable Perimortem::Core::Option<Ttx::Model::PackReference<Model::Pack>>
      constant;
  mutable ConstantState constant_state = ConstantState::Unresolved;
  Ttx::Lexical::Anchor anchor;
  Bool initializer_linked;
};

}  // namespace Tetrodotoxin::Library::Language::Flow
