// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/bytes.hpp"
#include "perimortem/core/view/vector.hpp"
#include "perimortem/core/option.hpp"

#include "perimortem/memory/allocator/arena.hpp"

#include "perimortem/system/uuid.hpp"

#include "tetrodotoxin/language/attribute.hpp"
#include "tetrodotoxin/language/visibility.hpp"
#include "ttx/concept/abstract.hpp"
#include "ttx/lexical/anchor.hpp"
#include "ttx/lexical/cursor.hpp"
#include "ttx/semantic/bound.hpp"

namespace Tetrodotoxin::Language {

// Definition owns the common declaration facts shared by concrete
// Tetrodotoxin languages. Authored definitions begin with a retained prefix and
// complete their Anchor after the concrete grammar succeeds. Synthetic
// definitions retain the same semantic contract without fabricating lexical
// evidence.
class Definition {
 public:
  static constexpr Perimortem::System::Uuid contract_id{
    0x01a084b0c85e7c80,
    0xa249b2ca85b10a3d,
  };

  // Consumers need declaration answers without acquiring the machinery used
  // to author a declaration. Binding can therefore attach these operations to
  // this Definition member or to a stored representation of the same facts.
  struct Operations {
    auto (*get_name)(const void*) -> Perimortem::Core::View::Bytes;
    auto (*get_documentation)(const void*)
        -> const Ttx::Concept::Documentation&;
    auto (*get_visibility)(const void*) -> Visibility;
    auto (*get_attributes)(const void*)
        -> Perimortem::Core::View::Vector<Attribute>;
    auto (*get_symbol_name)(const void*)
        -> Perimortem::Core::Option<Perimortem::Core::View::Bytes>;
    auto (*get_abi)(const void*)
        -> Perimortem::Core::Option<Perimortem::Core::View::Bytes>;
  };

  class Handle : public Ttx::Semantic::Bound<Operations> {
   public:
    using Bound::Bound;

    auto get_name() const -> Perimortem::Core::View::Bytes {
      return operations.get_name(source);
    }

    auto get_documentation() const -> const Ttx::Concept::Documentation& {
      return operations.get_documentation(source);
    }

    auto get_visibility() const -> Visibility {
      return operations.get_visibility(source);
    }

    auto get_attributes() const -> Perimortem::Core::View::Vector<Attribute> {
      return operations.get_attributes(source);
    }

    // A name is available only after its owner has a linkage identity. Package
    // supplies the namespace for generated definitions before compilation.
    // Foreign definitions can already supply their external symbol. Neither
    // this query nor its consumer invents a replacement from a display name.
    auto get_symbol_name() const
        -> Perimortem::Core::Option<Perimortem::Core::View::Bytes> {
      return operations.get_symbol_name(source);
    }

    auto get_abi() const
        -> Perimortem::Core::Option<Perimortem::Core::View::Bytes> {
      return operations.get_abi(source);
    }
  };

  // Native owners share these member access thunks. A stored provider can
  // supply the same operation table directly without owning a Definition or
  // implementing any of its authoring methods.
  template <typename Provider>
  static auto provide(const Provider& provider) -> Ttx::Semantic::Binding {
    static const Operations operations = {
      [](const void* source) -> Perimortem::Core::View::Bytes {
        return static_cast<const Provider*>(source)
            ->get_definition()
            .get_name();
      },
      [](const void* source) -> const Ttx::Concept::Documentation& {
        return static_cast<const Provider*>(source)
            ->get_definition()
            .get_documentation();
      },
      [](const void* source) -> Visibility {
        return static_cast<const Provider*>(source)
            ->get_definition()
            .get_visibility();
      },
      [](const void* source) -> Perimortem::Core::View::Vector<Attribute> {
        return static_cast<const Provider*>(source)
            ->get_definition()
            .get_attributes();
      },
      [](const void* source)
          -> Perimortem::Core::Option<Perimortem::Core::View::Bytes> {
        if constexpr (requires(const Provider& owner) { owner.get_symbol(); }) {
          return static_cast<const Provider*>(source)->get_symbol();
        }
        return {};
      },
      [](const void* source)
          -> Perimortem::Core::Option<Perimortem::Core::View::Bytes> {
        if constexpr (requires(const Provider& owner) { owner.get_abi(); }) {
          return static_cast<const Provider*>(source)->get_abi();
        }
        return {};
      },
    };
    return Ttx::Semantic::Binding::provide<Definition>(&provider, operations);
  }

  // Definition ordinarily consumes its own Attributes. An embedding
  // interpreter can provide the view it already consumed, while an engaged
  // empty view records that parsing has happened without inventing a value.
  // This also lets that interpreter intentionally silence Attributes whose
  // meaning belongs to its outer form.
  static auto parse(
      Ttx::Lexical::Cursor& cursor,
      const Ttx::Concept::Documentation& documentation,
      Ttx::Concept::Abstract& host,
      Perimortem::Core::Option<Perimortem::Core::View::Vector<Attribute>>
          attributes = {}) -> Perimortem::Core::Option<Definition&>;

  static auto create_synthetic(
      Perimortem::Memory::Allocator::Arena& domain,
      const Ttx::Concept::Documentation& documentation,
      Ttx::Concept::Abstract& host,
      Perimortem::Core::View::Bytes reserved_name,
      Visibility visibility,
      Ttx::Lexical::Anchor anchor,
      Perimortem::Core::View::Vector<Attribute> attributes = {}) -> Definition&;

  static auto create_restored(
      Perimortem::Memory::Allocator::Arena& domain,
      const Ttx::Concept::Documentation& documentation,
      Ttx::Concept::Abstract& host,
      Perimortem::Core::View::Vector<Attribute> attributes,
      Perimortem::Core::View::Bytes name,
      Visibility visibility) -> Definition&;

  // Creates one complete Definition after a concrete grammar has accepted its
  // exact authored form. This keeps alternate declaration orders in their
  // concrete language without copying common declaration facts into the
  // resulting semantic identity.
  static auto create_authored(
      Ttx::Lexical::Cursor& cursor,
      const Ttx::Concept::Documentation& documentation,
      Ttx::Concept::Abstract& host,
      Perimortem::Core::View::Vector<Attribute> attributes,
      Perimortem::Core::View::Vector<Ttx::Lexical::Token> modifiers,
      Visibility visibility,
      Ttx::Lexical::Token visibility_token,
      Perimortem::Core::View::Bytes name,
      Ttx::Lexical::Token name_token,
      Ttx::Lexical::Token qualifier,
      Ttx::Lexical::Anchor anchor) -> Definition&;

  // Some concrete languages establish their declaration identity before an
  // executable body can be interpreted. This factory retains that accepted
  // prefix with the same authored evidence, while complete() later extends the
  // Anchor through the body selected by that language.
  static auto create_authored_prefix(
      Ttx::Lexical::Cursor& cursor,
      const Ttx::Concept::Documentation& documentation,
      Ttx::Concept::Abstract& host,
      Perimortem::Core::View::Vector<Attribute> attributes,
      Perimortem::Core::View::Vector<Ttx::Lexical::Token> modifiers,
      Visibility visibility,
      Ttx::Lexical::Token visibility_token,
      Perimortem::Core::View::Bytes name,
      Ttx::Lexical::Token name_token,
      Ttx::Lexical::Token qualifier,
      Ttx::Lexical::Anchor anchor) -> Definition&;

  // Completes one authored declaration with its concrete grammar range. A
  // rejected or repeated completion leaves the original source fact intact.
  auto complete(Ttx::Lexical::Token focus, Ttx::Lexical::Token closing) -> Bool;

  constexpr auto get_documentation() const
      -> const Ttx::Concept::Documentation& {
    return documentation;
  }

  constexpr auto get_attributes() const
      -> Perimortem::Core::View::Vector<Attribute> {
    return attributes;
  }

  constexpr auto get_modifiers() const
      -> Perimortem::Core::View::Vector<Ttx::Lexical::Token> {
    return modifiers;
  }

  constexpr auto get_visibility() const -> Visibility { return visibility; }

  constexpr auto get_visibility_token() const -> Ttx::Lexical::Token {
    return visibility_token;
  }

  // Host is the enclosing mutable definition transaction owner and access
  // authority. It is never universal semantic parentage or a required route
  // through the TTX graph.
  constexpr auto get_host() -> Ttx::Concept::Abstract& { return host; }

  constexpr auto get_host() const -> const Ttx::Concept::Abstract& {
    return host;
  }

  constexpr auto is_authored() const -> Bool { return Bool(name_token); }

  constexpr auto is_complete() const -> Bool { return anchor_complete; }

  constexpr auto is_published() const -> Bool {
    return visibility != Visibility::Private;
  }

  constexpr auto get_name() const -> Perimortem::Core::View::Bytes {
    return name;
  }

  constexpr auto get_name_token() const -> Ttx::Lexical::Token {
    return name_token;
  }

  constexpr auto get_qualifier() const -> Ttx::Lexical::Token {
    return qualifier;
  }

  constexpr auto get_anchor() const -> Ttx::Lexical::Anchor { return anchor; }

  constexpr auto get_name_anchor() const -> Ttx::Lexical::Anchor {
    return Ttx::Lexical::Anchor::create(Ttx::Lexical::Span(name_token));
  }

  constexpr auto get_declaration_anchor() const
      -> Perimortem::Core::Option<Ttx::Lexical::Anchor> {
    if (name_token) {
      return get_name_anchor();
    }
    if (anchor.get_span()) {
      return anchor;
    }

    return {};
  }

 private:
  constexpr Definition(
      const Ttx::Concept::Documentation& documentation,
      Perimortem::Core::View::Vector<Attribute> attributes,
      Perimortem::Core::View::Vector<Ttx::Lexical::Token> modifiers,
      Visibility visibility,
      Ttx::Lexical::Token visibility_token,
      Perimortem::Core::View::Bytes name,
      Ttx::Lexical::Token name_token,
      Ttx::Lexical::Token qualifier,
      Ttx::Concept::Abstract& host,
      Ttx::Lexical::Anchor anchor,
      Bool anchor_complete)
      : documentation(documentation),
        attributes(attributes),
        modifiers(modifiers),
        visibility(visibility),
        visibility_token(visibility_token),
        name(name),
        name_token(name_token),
        qualifier(qualifier),
        host(host),
        anchor(anchor),
        anchor_complete(anchor_complete) {}

  const Ttx::Concept::Documentation& documentation;
  Perimortem::Core::View::Vector<Attribute> attributes;
  Perimortem::Core::View::Vector<Ttx::Lexical::Token> modifiers;
  Visibility visibility;
  Ttx::Lexical::Token visibility_token;
  Perimortem::Core::View::Bytes name;
  Ttx::Lexical::Token name_token;
  Ttx::Lexical::Token qualifier;
  Ttx::Concept::Abstract& host;
  Ttx::Lexical::Anchor anchor;
  Bool anchor_complete;
};

}  // namespace Tetrodotoxin::Language
