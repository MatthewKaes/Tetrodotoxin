// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/render/language/alias.hpp"

#include "tetrodotoxin/render/language/declarations.hpp"

using namespace Perimortem::Memory;
using namespace Ttx::Concept;
using Ttx::Semantic::Binding;
using namespace Ttx::Lexical;
using namespace Tetrodotoxin::Render;

auto Language::Alias::create(
    Allocator::Arena& domain,
    Tetrodotoxin::Language::Definition& definition,
    Tetrodotoxin::Language::TypeReference target) -> Alias& {
  return domain.construct_from<Alias>(
      [&]() { return Alias(definition, target); });
}

auto Language::Alias::link(Cursor& cursor, const Abstract& context) -> Bool {
  const Abstract& root =
      Declarations::resolve_lexical_context(context, target.get_root());
  auto selected = target.resolve_selected(cursor, root);
  if (!selected) {
    return False;
  }
  if (selected_type && &selected_type->get() != &*selected) {
    return False;
  }
  selected_type = Reference<const Ttx::Model::Type>(*selected);
  return True;
}

auto Language::Alias::link_restored(const Abstract& context) -> Bool {
  const Abstract& root =
      Declarations::resolve_lexical_context(context, target.get_root());
  auto selected = target.resolve_restored_selected(root);
  if (!selected) {
    return False;
  }
  if (selected_type && &selected_type->get() != &*selected) {
    return False;
  }
  selected_type = Reference<const Ttx::Model::Type>(*selected);
  return True;
}

auto Language::Alias::resolve() const -> const Abstract& {
  return selected_type ? static_cast<const Abstract&>(selected_type->get())
                       : Unknown::get_unknown();
}

auto Language::Alias::get_type() const -> const Abstract& {
  return resolve();
}

auto Language::Alias::resolve_concept(Perimortem::Core::View::Bytes name) const
    -> const Abstract& {
  return resolve().resolve_concept(name);
}

auto Language::Alias::visit_concepts(Abstract::Visitor visitor) const -> void {
  resolve().visit_concepts(visitor);
}

auto Language::Alias::bind_interface(Perimortem::System::Uuid requested) const
    -> Perimortem::Utility::Result<Binding, Binding::Failure> {
  if (requested == Tetrodotoxin::Language::Definition::contract_id) {
    return Tetrodotoxin::Language::Definition::provide(*this);
  }
  return resolve().bind_interface(requested);
}
