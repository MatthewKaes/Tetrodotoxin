// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/language/types/source.hpp"

#include "perimortem/core/diagnostics/log.hpp"

#include "tetrodotoxin/language/import.hpp"
#include "tetrodotoxin/library/language/model/memory.hpp"
#include "tetrodotoxin/library/language/model/callable.hpp"
#include "tetrodotoxin/library/language/monograph.hpp"
#include "ttx/concept/none.hpp"
#include "ttx/concept/unknown.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Ttx::Concept;
using namespace Ttx::Lexical;
using namespace Tetrodotoxin::Library::Language;

using Tetrodotoxin::Language::Visibility;

auto Types::Source::create_synthetic(
    Allocator::Arena& domain,
    const Documentation& documentation,
    Abstract& host,
    const Anchor& source_anchor) -> Source& {
  // The root has no instance state, so its empty Layout exists before any
  // Function Definition names Source as its host. Static Fields cannot change
  // that Source owned value.
  auto& definition = Tetrodotoxin::Language::Definition::create_synthetic(
      domain, documentation, host, "<source>"_view, Visibility::Public,
      source_anchor);
  return domain.construct_from<Source>(
      [&]() -> Source { return Source(domain, definition); });
}

auto Types::Source::retain_import_route(Import import) -> Bool {
  if (imports_linked) {
    return False;
  }
  import_routes.insert(import);
  return True;
}

auto Types::Source::link_types(Cursor& cursor) -> Bool {
  while (link_aliases() != 0) {
  }
  BAIL_IF(!validate_aliases(cursor));
  return Composite::link_types(cursor) && foreign.link_types(cursor);
}

auto Types::Source::link_fields(Cursor& cursor) -> Bool {
  BAIL_IF(!Composite::link_fields(cursor));
  return validate_layout(cursor);
}

auto Types::Source::link_initializers(Cursor& cursor) -> Bool {
  return Composite::link_initializers(cursor);
}

auto Types::Source::link_callable_signatures(Cursor& cursor) -> Bool {
  return foreign.link_callables(cursor) &&
         Composite::link_callable_signatures(cursor);
}

auto Types::Source::link_callable_bodies(Cursor& cursor) -> Bool {
  return Composite::link_callable_bodies(cursor);
}

auto Types::Source::finalize(Cursor& cursor) -> Bool {
  return Composite::finalize(cursor) && foreign.finalize(cursor);
}

auto Types::Source::link(Cursor& cursor, Abstract& interpretation_context)
    -> Bool {
  // Source owns the closure sequence because every phase mutates the same
  // declaration tree and its one Foreign context. Monograph only supplies the
  // package context that precedes this source graph.
  BAIL_IF(!link_imports(cursor, interpretation_context));
  BAIL_IF(!link_types(cursor));
  BAIL_IF(!link_callable_signatures(cursor));
  BAIL_IF(!link_fields(cursor));
  BAIL_IF(!link_initializers(cursor));
  return link_callable_bodies(cursor);
}

auto Types::Source::link_restored(Abstract& interpretation_context) -> Bool {
  if (!imports_linked) {
    for (const Import& import : import_routes.get_view()) {
      Option<const Abstract&> selected;
      import.get_type_reference()
          .resolve_lexical(interpretation_context)
          .visit(
              [&](const Abstract& resolved) { selected = resolved; },
              [](const TypeReference::Failure&) {});
      if (!selected || !retain_import_context(selected->resolve())) {
        Diagnostics::Log::error(
            "Restored Library Import did not resolve in Package context."_view);
        return False;
      }
    }
    imports_linked = True;
  }

  if (!Composite::link_restored_types()) {
    Diagnostics::Log::error("Restored Library Types failed linking."_view);
    return False;
  }
  if (!foreign.link_restored()) {
    Diagnostics::Log::error("Restored Library Foreign failed linking."_view);
    return False;
  }
  if (!Composite::link_restored_callable_signatures()) {
    Diagnostics::Log::error(
        "Restored Library Callable signatures failed linking."_view);
    return False;
  }
  if (!Composite::link_restored_fields()) {
    Diagnostics::Log::error("Restored Library Fields failed linking."_view);
    return False;
  }
  if (!validate_layout_restored()) {
    Diagnostics::Log::error(
        "Restored Library value Layout does not terminate."_view);
    return False;
  }
  if (!Composite::link_restored_initializers()) {
    Diagnostics::Log::error(
        "Restored Library initializers failed linking."_view);
    return False;
  }
  return True;
}

auto Types::Source::finalize_restored() -> Bool {
  return Composite::finalize_restored() && foreign.finalize_restored();
}

auto Types::Source::can_bind_static(const Abstract& binding, Category category)
    const -> Bool {
  // This query proves namespace and publication collisions independently from
  // lifecycle. Import discovery can therefore preflight future Addressables
  // before publishing any Type or Callable from the same transaction.
  BAIL_IF(
      is_finalized() || binding.get_name() == "foreign"_view ||
      !can_bind_definition(binding, category));

  if (category != Category::Type) {
    return True;
  }
  if (binding.is<Tetrodotoxin::Language::Import>()) {
    return True;
  }

  auto monograph = get_host().select<Library::Language::Monograph>();
  return !monograph || monograph->can_bind_source_type(binding.get_name());
}

auto Types::Source::retain_import_context(const Abstract& imported) -> Bool {
  const Abstract& context = imported.resolve();
  BAIL_IF(context.is<Unknown>() || context.is<None>() || &context == this);

  if (imports.get_view().contains(
          [&](const Reference<const Abstract>& retained) -> Bool {
            return &retained.get() == &context;
          })) {
    return True;
  }

  auto has_conflict = [&](auto bindings) -> Bool {
    for (const Reference<Abstract>& binding : bindings) {
      const Abstract& visible = context.visit<Composite>(
          [&](const Composite& composite) -> const Abstract& {
            return composite.resolve_public_context(binding.get().get_name());
          },
          [&](const Abstract& selected) -> const Abstract& {
            return selected.resolve_concept(binding.get().get_name());
          });
      if (!visible.is<Unknown>() && !visible.is<None>()) {
        return True;
      }
    }
    return False;
  };
  BAIL_IF(
      has_conflict(get_addressables()) || has_conflict(get_types()) ||
      has_conflict(get_callables()));

  imports.insert(context);
  return True;
}

auto Types::Source::link_imports(
    Cursor& cursor,
    Abstract& interpretation_context) -> Bool {
  if (imports_linked) {
    return True;
  }

  if (import_routes.is_empty()) {
    imports_linked = True;
    return True;
  }

  // Each using contributes one fallback context. Resolving the authored route
  // against the package context prevents local declarations from selecting
  // themselves while the Source is still incomplete.
  Bool failed = False;
  for (Count import_index = 0; import_index < import_routes.get_size();
       import_index++) {
    const Import& import = import_routes[import_index];
    Bool duplicate = import_routes.get_view()
                         .slice(0, import_index)
                         .contains([&](const Import& earlier) -> Bool {
                           return earlier.matches(import);
                         });
    if (duplicate) {
      cursor.create_expression_error(
          Anchor::create(import.get_span()),
          "Library source repeats one exact Import route."_view,
          "Keep one authored Import for each contextual route."_view);
      failed = True;
      continue;
    }

    auto selected = import.get_type_reference().resolve_authored(
        cursor, interpretation_context);
    if (!selected || selected->resolve().is<Unknown>()) {
      if (selected) {
        cursor.create_expression_error(
            Anchor::create(import.get_span()),
            "Library Import route did not resolve to one contextual object."_view,
            "Publish the selected context before linking this source."_view);
      }
      failed = True;
      continue;
    }

    if (!retain_import_context(selected->resolve())) {
      cursor.create_expression_error(
          Anchor::create(import.get_span()),
          "Library Import conflicts with this source context."_view,
          "Keep each visible name owned by only one local or imported "
          "context."_view);
      failed = True;
    }
  }

  BAIL_IF(failed);
  imports_linked = True;
  return True;
}

auto Types::Source::bind_static(
    Abstract& binding,
    Category category,
    Bool published) -> Bool {
  // Types and Callables enter only while the source declaration is open.
  // Addressables also have one deliberate late phase after every provider
  // Field has settled, but before any initializer consumes source lookup.
  Bool addressable_phase = category == Category::Addressable && is_linked();
  BAIL_IF(
      (!can_accept_definition() && !addressable_phase) ||
      !can_bind_static(binding, category));

  // Synthetic bindings admitted through this path have no Definition and
  // therefore never enter this source's public lookup index.
  publish_binding(binding, category, published, False);
  return True;
}

auto Types::Source::retain_binding(
    Abstract& binding,
    Tetrodotoxin::Language::Definition& definition,
    Category category,
    Cursor& cursor) -> Bool {
  BAIL_IF(!can_accept_definition());

  if (category == Category::Addressable) {
    auto addressable = binding.select<Model::Memory>();
    BAIL_IF(!addressable);

    // Source has no instance value. The retained Addressable declares whether
    // it contributes storage so Field does not inspect its concrete host.
    if (addressable->contributes_to_instance_layout()) {
      cursor.create_token_error(
          definition.get_name_token(),
          "Library Source rejects instance state Fields."_view,
          "Use an ordinary Static Field or move state into a Structure or "
          "Object."_view);
      return False;
    }
  }

  if (category == Category::Callable) {
    auto callable = binding.select<Model::Callable>();
    BAIL_IF(!callable);
    if (callable->declares_self()) {
      Token name = definition.get_name_token();
      cursor.create_expression_error(
          name ? Option<Anchor>(Anchor::create(Span(name))) : Option<Anchor>(),
          "A top level Library Function cannot receive `self`."_view,
          "Remove `self` from the top level Function signature."_view);
      return False;
    }
  }

  BAIL_IF(!can_bind_static(binding, category));
  publish_binding(binding, category, definition.is_published());
  cursor.get_associations().create(definition.get_name_anchor(), binding);
  return True;
}

auto Types::Source::resolve_concept(View::Bytes route) const
    -> const Abstract& {
  // Foreign is one reserved receiver context, while authored Source names use
  // the ordinary public categories. The Monograph fallback supplies intrinsic
  // and source-local import names.
  if (route == "foreign"_view && foreign.is_authored()) {
    return foreign;
  }
  if (route == "static"_view || route == "instance"_view) {
    return Composite::resolve_concept(route);
  }

  const Abstract& local = resolve_public_context(route);
  return !local.is<Unknown>() && !local.is<None>()
             ? local
             : get_host().resolve_concept(route);
}

auto Types::Source::resolve_lexical_context(View::Bytes route) const
    -> const Abstract& {
  auto monograph = get_host().select<Language::Monograph>();
  return monograph ? monograph->resolve_lexical_context(route)
                   : Composite::resolve_lexical_context(route);
}

auto Types::Source::resolve_public_context(View::Bytes route) const
    -> const Abstract& {
  return resolve_local(route, Visibility::Public);
}

auto Types::Source::create_default(Allocator::Arena&) const
    -> Option<Model::Pack&> {
  // Source is an empty contextual root and never enters value flow. Absence
  // keeps that fact distinct from a completed Pack that produces zero values.
  return {};
}

auto Types::Source::resolve_imports(View::Bytes route) const
    -> const Abstract& {
  // Using contexts are composable query fallbacks, not an ordered shadowing
  // list. Context, access, and call queries all accept no answer as missing and
  // repeated answers only when they resolve to the same identity. Distinct
  // provider identities make the query ambiguous and therefore Unknown.
  Option<const Abstract&> selected;
  for (const Reference<const Abstract>& retained : imports.get_view()) {
    const Abstract& context = retained.get();
    const Abstract& candidate = context.visit<Composite>(
        [&](const Composite& composite) -> const Abstract& {
          return composite.resolve_public_context(route);
        },
        [&](const Abstract& provider) -> const Abstract& {
          return provider.resolve_concept(route);
        });
    if (candidate.is<Unknown>() || candidate.is<None>()) {
      continue;
    }
    if (selected && &selected->resolve() != &candidate.resolve()) {
      return Unknown::get_unknown();
    }
    selected = candidate;
  }

  return selected ? *selected : Unknown::get_unknown();
}

auto Types::Source::resolve_local(View::Bytes route, Visibility visibility)
    const -> const Abstract& {
  return visibility == Visibility::Private
             ? get_static_authority().resolve_concept(route)
             : get_static_authority().resolve_published(route);
}
