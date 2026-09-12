// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/language/alias.hpp"

#include "ttx/concept/unknown.hpp"
#include "ttx/model/documentations/merged.hpp"
#include "ttx/model/type.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Ttx::Concept;
using Ttx::Semantic::Binding;
using namespace Ttx::Lexical;
using namespace Tetrodotoxin::Library::Language;

auto Tetrodotoxin::Library::Language::Alias::create_authored(
    Allocator::Arena& domain,
    Tetrodotoxin::Language::Definition& definition,
    TypeReference target_reference) -> Alias& {
  return create(domain, definition, target_reference);
}

auto Tetrodotoxin::Library::Language::Alias::create(
    Allocator::Arena& domain,
    Tetrodotoxin::Language::Definition& definition,
    TypeReference target_reference) -> Alias& {
  return domain.construct_from<Alias>(
      [&]() -> Alias { return Alias(domain, definition, target_reference); });
}

auto Alias::link() -> Bool {
  if (linked) {
    return True;
  }
  Option<const Abstract&> selected;
  target_reference.resolve_lexical(definition.get_host())
      .visit(
          [&](const Abstract& resolved) { selected = resolved; },
          [](const TypeReference::Failure&) {});
  BAIL_IF(!selected);

  auto target = selected->select<Ttx::Model::Type>();
  BAIL_IF(!target);

  this->target = Reference<const Ttx::Model::Type>(*target);

  // The declaration can explain why this name was introduced without changing
  // the selected Type's own documentation. Borrowing a merged view preserves
  // both explanations without moving that policy into transparent Alias.
  const Documentation& local = get_definition().get_documentation();
  if (local.is_empty()) {
    documentation = target->get_documentation();
  } else {
    documentation = domain.construct<Ttx::Model::Documentations::Merged>(
        local, target->get_documentation());
  }

  linked = True;
  return True;
}

auto Alias::report_unresolved(Cursor& cursor) const -> void {
  Option<TypeReference::Failure> failure;
  Option<const Abstract&> selected;
  target_reference.resolve_lexical(definition.get_host())
      .visit(
          [&](const Abstract& resolved) { selected = resolved; },
          [&](const TypeReference::Failure& rejected) { failure = rejected; });
  if (failure) {
    target_reference.report(cursor, *failure);
    return;
  }

  if (selected && selected->is<Ttx::Model::Type>()) {
    return;
  }
  cursor.create_expression_error(
      target_reference.get_anchor(),
      "Library Alias Type reference did not resolve to one Type."_view,
      "Publish the selected Type and remove any Alias cycle before linking."_view);
}

auto Alias::get_documentation() const -> const Documentation& {
  return documentation.visit(
      [&]() -> const Documentation& {
        return get_definition().get_documentation();
      },
      [](const Documentation& selected) -> const Documentation& {
        return selected;
      });
}

// Source closes declaration routes before all Types have linked their bodies.
// Retaining the native Type here preserves that construction order. A caller
// asking whether the Type itself is complete still uses the Type's resolve.
auto Alias::resolve() const -> const Abstract& {
  return target ? static_cast<const Abstract&>(target->get())
                : Unknown::get_unknown();
}

auto Alias::get_type() const -> const Abstract& {
  return resolve();
}

auto Alias::resolve_concept(Perimortem::Core::View::Bytes name) const
    -> const Abstract& {
  return target_reference.resolve_concept(name);
}

auto Alias::visit_concepts(Abstract::Visitor visitor) const -> void {
  target_reference.visit_concepts(visitor);
}

auto Alias::bind_interface(Perimortem::System::Uuid requested) const
    -> Perimortem::Utility::Result<Binding, Binding::Failure> {
  if (requested == Tetrodotoxin::Language::Definition::contract_id) {
    return Tetrodotoxin::Language::Definition::provide(*this);
  }
  return target_reference.bind_interface(requested);
}
