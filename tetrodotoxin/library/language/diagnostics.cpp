// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/language/diagnostics.hpp"

#include "tetrodotoxin/library/language/model/memory.hpp"
#include "tetrodotoxin/library/language/model/type.hpp"
#include "ttx/concept/unknown.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Concept;
using namespace Tetrodotoxin::Library;

auto Language::Diagnostics::write_type(
    Ttx::Lexical::Errors::Report& report,
    const Abstract& abstract) -> void {
  const Abstract& resolved = abstract.is<Language::Model::Type>() ||
                                     abstract.is<Ttx::Model::Addressable>()
                                 ? abstract
                                 : abstract.resolve();
  const Language::Model::Type* type = nullptr;
  auto direct = resolved.select<Language::Model::Type>();
  if (direct) {
    type = &*direct;
  } else {
    auto addressable = resolved.select<Ttx::Model::Addressable>();
    if (addressable && !addressable->resolve().is<Unknown>()) {
      auto selected = addressable->get_type().select<Language::Model::Type>();
      if (selected) {
        type = &*selected;
      }
    }
  }

  if (!type) {
    auto pack = Language::Model::Pack::from(resolved);
    if (pack && pack->get_layout().get_size() == 1) {
      const Abstract& value_type = pack->get_value_type(0);
      if (&value_type != &resolved) {
        write_type(report, value_type);
        return;
      }
    }
  }

  if (type == nullptr || type->is<Unknown>()) {
    report << "<invalid>"_view;
    return;
  }

  report << type->get_name();
}

auto Language::Diagnostics::write_layout(
    Ttx::Lexical::Errors::Report& report,
    const Layout& layout) -> void {
  report << "["_view;
  for (Count index = 0; index < layout.get_size(); index++) {
    if (index != 0) {
      report << ", "_view;
    }

    auto name = layout.get_name(index);
    if (name) {
      report << "."_view << *name << " = "_view;
    }

    layout.get_abstract(index).visit(
        [&]() { report << "<invalid>"_view; },
        [&](const Abstract& selected) { write_type(report, selected); });
  }
  report << "]"_view;
}

auto Language::Diagnostics::write_pack(
    Ttx::Lexical::Errors::Report& report,
    const Model::Pack& pack) -> void {
  const Layout& layout = pack.get_layout();
  report << "["_view;
  for (Count index = 0; index < layout.get_size(); index++) {
    if (index != 0) {
      report << ", "_view;
    }

    auto name = layout.get_name(index);
    if (name) {
      report << "."_view << *name << " = "_view;
    }
    layout.get_abstract(index).visit(
        [&]() { report << "<invalid>"_view; },
        [&](const Abstract& selected) { write_type(report, selected); });
  }
  report << "]"_view;
}
