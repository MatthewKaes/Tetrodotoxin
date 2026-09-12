// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/language/expressions/identifier.hpp"

#include "tetrodotoxin/language/import.hpp"
#include "tetrodotoxin/library/language/diagnostics.hpp"
#include "tetrodotoxin/library/language/flow/block.hpp"
#include "tetrodotoxin/library/language/model/memory.hpp"
#include "ttx/concept/none.hpp"
#include "ttx/concept/unknown.hpp"

using namespace Perimortem;
using namespace Ttx::Concept;
using namespace Ttx::Model;
using namespace Tetrodotoxin::Library;

// A native Type identity can be useful before its full resolve answer becomes
// factual. Import supplies that Type through its own operation, while other
// declarations and transparent references follow ordinary resolution.
static auto select_native(const Abstract& candidate) -> const Abstract& {
  if (candidate.is<Ttx::Model::Type>()) {
    return candidate;
  }
  const Abstract& resolved = candidate.resolve();
  return resolved.is<Tetrodotoxin::Language::Import>() ? resolved.get_type()
                                                       : resolved;
}

auto Language::Expressions::Identifier::link(
    Ttx::Lexical::Cursor& cursor,
    const Abstract& lexical_context,
    Core::Option<const Abstract&> access_scope) -> Bool {
  (void)token;
  (void)access_scope;
  const Abstract& candidate =
      select_native(lexical_context.resolve_concept(name));
  const Abstract& selected = candidate.is<Language::Model::Type>() ||
                                     candidate.is<Ttx::Model::Addressable>()
                                 ? candidate
                                 : candidate.resolve();
  auto source_anchor = get_anchor();

  if (selected.is<Unknown>() || selected.is<None>()) {
    auto report = cursor.create_report(source_anchor);
    report << "Identifier '"_view << name
           << "' is not available in lexical context '"_view
           << lexical_context.get_name() << "'."_view;
    report.get_hint()
        << "Declare '"_view << name
        << "' before this use or correct the authored spelling."_view;
    return False;
  }

  // A later pass may fill an unresolved name, but a successful edge is
  // permanent. Repeating the same exact link remains harmless.
  if (result && &result->get() != &selected) {
    auto report = cursor.create_report(source_anchor);
    report << "Internal semantic error: Identifier '"_view << name
           << "' changed identity from '"_view << result->get().get_name()
           << "' to '"_view << selected.get_name() << "'."_view;
    report.get_hint()
        << "The source is valid; report this unstable linking result."_view;
    return False;
  }

  result = Reference<const Abstract>(selected);
  if (source_anchor) {
    cursor.get_associations().create(*source_anchor, selected);
  }
  return True;
}

auto Language::Expressions::Identifier::link_restored(
    const Abstract& lexical_context,
    Core::Option<const Abstract&>) -> Bool {
  const Abstract& candidate =
      select_native(lexical_context.resolve_concept(name));
  const Abstract& selected = candidate.is<Language::Model::Type>() ||
                                     candidate.is<Ttx::Model::Addressable>()
                                 ? candidate
                                 : candidate.resolve();
  BAIL_IF(selected.is<Unknown>() || selected.is<None>());
  result = Reference<const Abstract>(selected);
  return True;
}

auto Language::Expressions::Identifier::get_documentation() const
    -> const Documentation& {
  return result.visit(
      []() -> const Documentation& { return Documentation::get_empty(); },
      [](const Reference<const Abstract>& selected) -> const Documentation& {
        return selected.get().get_documentation();
      });
}

auto Language::Expressions::Identifier::get_type() const -> const Abstract& {
  return result.visit(
      []() -> const Abstract& { return Unknown::get_unknown(); },
      [&](const Reference<const Abstract>& selected) -> const Abstract& {
        const Abstract& direct = selected.get();
        auto pack = Language::Model::Pack::from(direct);
        if (pack) {
          return pack->get_type();
        }
        return direct.visit<Language::Model::Type>(
            [](const Language::Model::Type&) -> const Abstract& {
              return Unknown::get_unknown();
            },
            [](const Abstract& addressable) -> const Abstract& {
              return addressable.visit<Ttx::Model::Addressable>(
                  [](const Ttx::Model::Addressable& selected)
                      -> const Abstract& { return selected.get_type(); },
                  [](const Abstract&) -> const Abstract& {
                    return Unknown::get_unknown();
                  });
            });
      });
}

auto Language::Expressions::Identifier::get_result() const -> const Abstract& {
  return result.visit(
      []() -> const Abstract& { return Unknown::get_unknown(); },
      [](const Reference<const Abstract>& selected) -> const Abstract& {
        return selected.get();
      });
}

auto Language::Expressions::Identifier::resolve_authored() const
    -> const Abstract& {
  if (result) {
    return result->get();
  }

  const Abstract& context = lexical_context.get();
  auto block = context.select<Language::Flow::Block>();
  const Abstract& candidate =
      block && token
          ? block->resolve_authored_context(name, Count(token.get_offset()))
          : context.resolve_concept(name);
  return select_native(candidate);
}
