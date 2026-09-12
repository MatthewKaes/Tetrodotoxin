// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/language/types/composite.hpp"

#include "perimortem/core/diagnostics/log.hpp"

#include "tetrodotoxin/library/language/alias.hpp"
#include "tetrodotoxin/library/language/field.hpp"
#include "tetrodotoxin/library/language/function.hpp"
#include "tetrodotoxin/library/language/model/callable.hpp"
#include "tetrodotoxin/library/language/model/memory.hpp"
#include "tetrodotoxin/library/language/types/enumeration.hpp"
#include "tetrodotoxin/library/language/types/object.hpp"
#include "tetrodotoxin/library/language/types/structure.hpp"
#include "ttx/concept/unknown.hpp"
#include "ttx/model/layouts/termination.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Perimortem::Utility;
using namespace Ttx::Concept;
using Ttx::Semantic::Binding;
using namespace Ttx::Lexical;
using namespace Tetrodotoxin::Library::Language;

using Tetrodotoxin::Language::Visibility;

static constexpr Ttx::Model::Layouts::Named empty_layout;

template <typename selected_type, typename visitor_type>
static auto visit_each(
    View::Vector<Reference<Abstract>> bindings,
    visitor_type visitor) -> Bool {
  // A category may retain an opaque Alias before its target closes. Only an
  // exact local contract receives this lifecycle step because the target owner
  // remains responsible for completing its own graph.
  Bool failed = False;
  for (const Reference<Abstract>& binding : bindings) {
    auto selected = binding.get().select<selected_type>();
    if (selected) {
      failed |= !visitor(*selected);
    }
  }

  return !failed;
}

template <typename selected_type>
static auto select_next(
    View::Vector<Reference<Abstract>> bindings,
    Count& index) -> Option<const selected_type&> {
  while (index < bindings.get_size()) {
    auto selected = bindings.get_data()[index].get().select<selected_type>();
    if (selected) {
      return *selected;
    }

    index++;
  }

  return {};
}

template <typename selected_type>
static auto declaration_offset(const Option<const selected_type&>& selected)
    -> Count {
  if (!selected) {
    return Count(-1);
  }

  Option<Anchor> anchor;
  selected->template bind<Tetrodotoxin::Source::Declaration>().visit(
      [&](const Tetrodotoxin::Source::Declaration::Handle& declaration) {
        anchor = declaration.get_anchor();
      },
      [](Binding::Failure) {});
  return anchor ? Count(anchor->get_span().get_offset()) : Count(-1);
}

// Source barriers apply to the declaration's published completion service.
// A generated value may have no source work, while Pending or Rejected must
// stop the barrier instead of selecting another path to the same object.
static auto complete_declaration(
    Abstract::Handle subject,
    Tetrodotoxin::Source::Declaration::Phase phase,
    Cursor* cursor = nullptr) -> Bool {
  Bool succeeded = True;
  subject.bind<Tetrodotoxin::Source::Declaration>().visit(
      [&](const Tetrodotoxin::Source::Declaration::Handle& declaration) {
        declaration.complete(phase, cursor)
            .visit(
                [&](Bool completed) { succeeded &= completed; },
                [&](Binding::Failure) { succeeded = False; });
      },
      [&](Binding::Failure failure) {
        if (failure != Binding::Failure::Unsupported) {
          succeeded = False;
        }
      });
  return succeeded;
}

static auto complete_declarations(
    View::Vector<Reference<Abstract>> declarations,
    Tetrodotoxin::Source::Declaration::Phase phase,
    Cursor* cursor = nullptr) -> Bool {
  Bool succeeded = True;
  for (const auto& subject : declarations) {
    succeeded &=
        complete_declaration(subject.get().get_interface(), phase, cursor);
  }
  return succeeded;
}

Types::Composite::Composite(
    Allocator::Arena& domain,
    Tetrodotoxin::Language::Definition& definition)
    : Model::Type(domain),
      definition(definition),
      domain(domain),
      static_authority(edit_static_authority()),
      instance_authority(edit_instance_authority()),
      addressables(domain),
      published_addressables(domain),
      types(domain),
      published_types(domain),
      declarations(domain) {}

auto Types::Composite::has_private_access_to(const Model::Type& owner) const
    -> Bool {
  if (this == &owner) {
    return True;
  }

  // Authority walks outward from the caller. Asking the owner to walk its own
  // host would admit parents, siblings, and unrelated Aliases.
  auto enclosing = get_host().select<Model::Type>();
  return enclosing && enclosing->has_private_access_to(owner);
}

auto Types::Composite::retain_authored_definition(
    Abstract& binding,
    Tetrodotoxin::Language::Definition& definition,
    Category category,
    Cursor& cursor) -> Bool {
  if (!retain_binding(binding, definition, category, cursor)) {
    if (category == Category::Addressable) {
      cursor.create_expression_error(
          definition.get_name_anchor(),
          "Library Addressable name is already occupied in this Composite."_view,
          "Static and state Fields share one Addressable namespace. Choose a "
          "unique name."_view);
    } else {
      cursor.create_expression_error(
          definition.get_name_anchor(),
          "Library member collides with an occupied Composite category."_view);
    }
    return False;
  }

  return True;
}

auto Types::Composite::retain_definition(
    Abstract& binding,
    Category category,
    Bool published) -> Bool {
  return publish_binding(binding, category, published);
}

auto Types::Composite::can_accept_definition() const -> Bool {
  return stage == Stage::Authored;
}

auto Types::Composite::can_bind_definition(
    const Abstract& binding,
    Category category) const -> Bool {
  switch (category) {
  case Category::Type:
    return static_authority.can_bind(binding);
  case Category::Callable: {
    auto callable = binding.select<Model::Callable>();
    BAIL_IF(!callable);
    return callable->declares_self() ? instance_authority.can_bind(binding)
                                     : static_authority.can_bind(binding);
  }
  case Category::Addressable: {
    auto addressable = binding.select<Model::Memory>();
    BAIL_IF(!addressable);
    return addressable->contributes_to_instance_layout()
               ? instance_authority.can_bind(binding)
               : static_authority.can_bind(binding);
  }
  }
  return False;
}

auto Types::Composite::retain_binding(
    Abstract& binding,
    Tetrodotoxin::Language::Definition& definition,
    Category category,
    Cursor& cursor) -> Bool {
  BAIL_IF(!can_accept_definition() || !can_bind_definition(binding, category));

  BAIL_IF(!publish_binding(binding, category, definition.is_published()));
  cursor.get_associations().create(definition.get_name_anchor(), binding);
  return True;
}

auto Types::Composite::publish_binding(
    Abstract& binding,
    Category category,
    Bool published,
    Bool persistent,
    Bool prepend) -> Bool {
  // The parser or importing provider supplies the category before publication.
  // Alias resolution is deliberately absent here: delayed graph completion
  // cannot change which namespace owns the local name.
  BAIL_IF(!can_bind_definition(binding, category));
  Bool bound = False;
  switch (category) {
  case Category::Type:
    bound = static_authority.bind(binding, published);
    break;
  case Category::Callable: {
    auto callable = binding.select<Model::Callable>();
    BAIL_IF(!callable);
    bound = callable->declares_self()
                ? instance_authority.bind(binding, published)
                : static_authority.bind(binding, published);
    break;
  }
  case Category::Addressable: {
    auto addressable = binding.select<Model::Memory>();
    BAIL_IF(!addressable);
    bound = addressable->contributes_to_instance_layout()
                ? instance_authority.bind(binding, published)
                : static_authority.bind(binding, published);
    break;
  }
  }
  BAIL_IF(!bound);
  switch (category) {
  case Category::Addressable:
    if (prepend) {
      addressables.prepend(binding);
    } else {
      addressables.insert(binding);
    }
    if (persistent) {
      declarations.insert(binding);
    }
    if (published) {
      if (prepend) {
        published_addressables.prepend(binding);
      } else {
        published_addressables.insert(binding);
      }
    }
    return True;
  case Category::Callable:
    BAIL_IF(prepend);
    publish_callable(domain, binding, published);
    if (persistent) {
      declarations.insert(binding);
    }
    return True;
  case Category::Type:
    BAIL_IF(prepend);
    types.insert(binding);
    if (persistent) {
      declarations.insert(binding);
    }
    if (published) {
      published_types.insert(binding);
    }
    return True;
  }

  return False;
}

auto Types::Composite::is_published(const Abstract& declaration) const -> Bool {
  return static_authority.is_published(declaration) ||
         instance_authority.is_published(declaration);
}

auto Types::Composite::link_aliases() -> Count {
  Count linked = 0;
  for (const Reference<Abstract>& binding : types.get_view()) {
    auto alias = binding.get().select<Alias>();
    if (alias && !alias->is_linked() && alias->link()) {
      linked++;
    }

    auto type = binding.get().select<Model::Type>();
    if (type) {
      linked += type->link_aliases();
    }
  }
  return linked;
}

auto Types::Composite::validate_aliases(Cursor& cursor) const -> Bool {
  Bool valid = True;
  for (const Reference<Abstract>& binding : types.get_view()) {
    auto alias = binding.get().select<Alias>();
    if (alias && !alias->is_linked()) {
      alias->report_unresolved(cursor);
      valid = False;
    }

    auto type = binding.get().select<Model::Type>();
    if (type && !type->validate_aliases(cursor)) {
      valid = False;
    }
  }
  return valid;
}

auto Types::Composite::link_types(Cursor& cursor) -> Bool {
  if (stage >= Stage::TypesLinked) {
    return True;
  }

  if (stage != Stage::Authored) {
    cursor.create_expression_error(
        get_anchor(),
        "Composite declaration Types cannot link from this lifecycle stage."_view,
        "Begin with the complete authored Composite declaration."_view);
    return False;
  }

  Bool failed = !visit_each<Model::Type>(
      types.get_view(),
      [&](Model::Type& type) { return type.link_types(cursor); });

  BAIL_IF(failed);

  stage = Stage::TypesLinked;
  return True;
}

auto Types::Composite::link_fields(Cursor& cursor) -> Bool {
  if (stage >= Stage::FieldsLinked) {
    return True;
  }

  if (stage != Stage::CallableSignaturesLinked) {
    cursor.create_expression_error(
        get_anchor(),
        "Composite Fields require linked Callable signatures."_view,
        "Settle every signature before a Field Expression can invoke it."_view);
    return False;
  }

  Bool failed = !visit_each<Model::Type>(
      types.get_view(),
      [&](Model::Type& type) { return type.link_fields(cursor); });
  BAIL_IF(failed);

  // Authored Type routes settle without evaluating initializers. Completing
  // those declarations first gives inference every exact Type while each
  // Addressable decides whether it owns an authored route.
  failed |= !complete_declarations(
      addressables.get_view(), Tetrodotoxin::Source::Declaration::Phase::Type,
      &cursor);

  // Explicit declarations are now safe lookup targets. Each inferred owner
  // then authenticates its final context, while only completed candidates
  // become visible to later inference.
  failed |= !complete_declarations(
      addressables.get_view(),
      Tetrodotoxin::Source::Declaration::Phase::InferredType, &cursor);

  BAIL_IF(failed);

  stage = Stage::FieldsLinked;
  return True;
}

auto Types::Composite::validate_layout(Cursor& cursor) const -> Bool {
  Bool valid = True;
  for (const Reference<Abstract>& binding : types.get_view()) {
    auto type = binding.get().select<Model::Type>();
    if (type && !type->validate_layout(cursor)) {
      valid = False;
    }
  }

  const Layout& selected_layout = get_layout();
  if (selected_layout.is_empty() ||
      Ttx::Model::Layouts::is_terminating(*this)) {
    return valid;
  }

  // Every stored Addressable Type is settled before this closure check.
  // Rejecting an endless shape here keeps default construction and lowering
  // free from separate defensive cycle protocols.
  cursor.create_expression_error(
      get_anchor(), "Library Type Layout does not terminate."_view,
      "Break recursive value storage with one terminal Type Layout."_view);
  return False;
}

auto Types::Composite::validate_layout_restored() const -> Bool {
  for (const Reference<Abstract>& binding : types.get_view()) {
    auto composite = binding.get().select<Composite>();
    BAIL_IF(composite && !composite->validate_layout_restored());
  }

  const Layout& selected_layout = get_layout();
  return selected_layout.is_empty() ||
         Ttx::Model::Layouts::is_terminating(*this);
}

auto Types::Composite::complete_field_layout() -> void {
  Managed::Vector<Reference<const Abstract>> fields(domain);
  fields.reset(addressables.get_size());
  for (const Reference<Abstract>& binding : addressables.get_view()) {
    auto addressable = binding.get().select<Model::Memory>();
    if (addressable && addressable->contributes_to_instance_layout()) {
      fields.insert(*addressable);
    }
  }
  layout = domain.construct<Ttx::Model::Layouts::Named>(fields.get_view());
  static_authority.complete();
  instance_authority.complete();
}

auto Types::Composite::link_initializers(Cursor& cursor) -> Bool {
  if (stage >= Stage::InitializersLinked) {
    return True;
  }

  if (stage != Stage::FieldsLinked) {
    cursor.create_expression_error(
        get_anchor(), "Composite initializers require linked Fields."_view,
        "Complete every Field Type before linking its initializer."_view);
    return False;
  }

  Bool failed = !visit_each<Model::Type>(
      types.get_view(),
      [&](Model::Type& type) { return type.link_initializers(cursor); });
  failed |= !complete_declarations(
      addressables.get_view(),
      Tetrodotoxin::Source::Declaration::Phase::Initializer, &cursor);

  BAIL_IF(failed);

  // Every initializer Expression is linked before const folding begins. A
  // const declaration may therefore depend on any other acyclic const
  // declaration in this Composite without source order becoming semantic.
  failed |= !complete_declarations(
      addressables.get_view(),
      Tetrodotoxin::Source::Declaration::Phase::Constant, &cursor);

  BAIL_IF(failed);

  stage = Stage::InitializersLinked;
  return True;
}

auto Types::Composite::link_callable_signatures(Cursor& cursor) -> Bool {
  if (stage >= Stage::CallableSignaturesLinked) {
    return True;
  }

  if (stage != Stage::TypesLinked) {
    cursor.create_expression_error(
        get_anchor(),
        "Composite Callable signatures require linked declaration Types."_view,
        "Settle every nested Type before completing Callable signatures."_view);
    return False;
  }

  Bool failed = !visit_each<Model::Type>(
      types.get_view(),
      [&](Model::Type& type) { return type.link_callable_signatures(cursor); });
  failed |= !complete_declarations(
      get_callable_bindings(),
      Tetrodotoxin::Source::Declaration::Phase::Signature, &cursor);

  BAIL_IF(failed);

  stage = Stage::CallableSignaturesLinked;
  return True;
}

auto Types::Composite::link_callable_bodies(Cursor& cursor) -> Bool {
  if (stage >= Stage::CallablesLinked) {
    return True;
  }

  if (stage != Stage::InitializersLinked) {
    cursor.create_expression_error(
        get_anchor(),
        "Composite Callable bodies require linked initializers."_view,
        "Complete every Field Expression before linking Callable bodies."_view);
    return False;
  }

  Bool failed = !visit_each<Model::Type>(
      types.get_view(),
      [&](Model::Type& type) { return type.link_callable_bodies(cursor); });
  failed |= !complete_declarations(
      get_callable_bindings(), Tetrodotoxin::Source::Declaration::Phase::Body,
      &cursor);

  BAIL_IF(failed);

  stage = Stage::CallablesLinked;
  return True;
}

auto Types::Composite::finalize(Cursor& cursor) -> Bool {
  if (stage == Stage::Finalized) {
    return True;
  }

  if (stage != Stage::CallablesLinked) {
    cursor.create_expression_error(
        get_anchor(), "An incomplete Composite cannot enter finalization."_view,
        "Link every Field, initializer, and Callable before finalizing."_view);
    return False;
  }

  Bool failed = !visit_each<Model::Type>(
      types.get_view(),
      [&](Model::Type& type) { return type.finalize(cursor); });

  failed |= !complete_declarations(
      addressables.get_view(),
      Tetrodotoxin::Source::Declaration::Phase::Finalize, &cursor);

  // Callable folding still runs when publication fails. Independent cache and
  // diagnostic facts therefore remain observable without admitting the Type.
  failed |= !complete_declarations(
      get_callable_bindings(),
      Tetrodotoxin::Source::Declaration::Phase::Finalize, &cursor);

  BAIL_IF(failed);

  stage = Stage::Finalized;
  return True;
}

auto Types::Composite::link_restored_types() -> Bool {
  if (stage >= Stage::TypesLinked) {
    return True;
  }
  BAIL_IF(stage != Stage::Authored);

  while (link_aliases() != 0) {
  }
  for (const Reference<Abstract>& binding : types.get_view()) {
    auto alias = binding.get().select<Language::Alias>();
    BAIL_IF(alias && !alias->is_linked());
  }
  BAIL_IF(!visit_each<Model::Type>(types.get_view(), [](Model::Type& type) {
    return type.link_restored_types();
  }));

  stage = Stage::TypesLinked;
  return True;
}

auto Types::Composite::link_restored_callable_signatures() -> Bool {
  if (stage >= Stage::CallableSignaturesLinked) {
    return True;
  }
  BAIL_IF(stage != Stage::TypesLinked);

  BAIL_IF(!visit_each<Model::Type>(types.get_view(), [](Model::Type& type) {
    return type.link_restored_callable_signatures();
  }));
  BAIL_IF(!complete_declarations(
      get_callable_bindings(),
      Tetrodotoxin::Source::Declaration::Phase::RestoredSignature));

  stage = Stage::CallableSignaturesLinked;
  return True;
}

auto Types::Composite::link_restored_fields() -> Bool {
  if (stage >= Stage::FieldsLinked) {
    return True;
  }
  BAIL_IF(stage != Stage::CallableSignaturesLinked);

  BAIL_IF(!visit_each<Model::Type>(types.get_view(), [](Model::Type& type) {
    return type.link_restored_fields();
  }));
  BAIL_IF(!complete_declarations(
      addressables.get_view(),
      Tetrodotoxin::Source::Declaration::Phase::RestoredType));

  complete_field_layout();
  stage = Stage::FieldsLinked;
  return True;
}

auto Types::Composite::link_restored_initializers() -> Bool {
  if (stage >= Stage::InitializersLinked) {
    return True;
  }
  BAIL_IF(stage != Stage::FieldsLinked);

  for (const Reference<Abstract>& binding : types.get_view()) {
    auto type = binding.get().select<Model::Type>();
    if (type && !type->link_restored_initializers()) {
      Diagnostics::Log::Message<256> message(Diagnostics::Log::Level::Error);
      message << "Restored Type initializer closure failed for '"_view
              << type->get_name() << "'."_view;
      return False;
    }
  }

  for (const Reference<Abstract>& binding : addressables.get_view()) {
    if (!complete_declaration(
            binding.get().get_interface(),
            Tetrodotoxin::Source::Declaration::Phase::RestoredInitializer)) {
      Diagnostics::Log::Message<256> message(Diagnostics::Log::Level::Error);
      message << "Restored Addressable initializer failed for '"_view
              << binding.get().get_name() << "'."_view;
      return False;
    }
  }

  stage = Stage::CallablesLinked;
  return True;
}

auto Types::Composite::finalize_restored() -> Bool {
  if (stage == Stage::Finalized) {
    return True;
  }
  BAIL_IF(stage != Stage::CallablesLinked);

  BAIL_IF(!visit_each<Model::Type>(types.get_view(), [](Model::Type& type) {
    return type.finalize_restored();
  }));
  stage = Stage::Finalized;
  return True;
}

auto Types::Composite::resolve() const -> const Abstract& {
  if (stage < Stage::FieldsLinked) {
    return Unknown::get_unknown();
  }

  return *this;
}

auto Types::Composite::resolve_binding(
    View::Bytes route,
    Category category,
    Visibility visibility) const -> const Abstract& {
  const Abstract& selected = [&]() -> const Abstract& {
    return visibility == Visibility::Private
               ? static_authority.resolve_concept(route)
               : static_authority.resolve_published(route);
  }();

  if (selected.is<Unknown>() || selected.is<None>()) {
    return selected;
  }
  switch (category) {
  case Category::Addressable:
    return selected.is<Model::Memory>() ? selected : Unknown::get_unknown();
  case Category::Callable:
    return selected.is<Model::Callable>() ? selected : Unknown::get_unknown();
  case Category::Type:
    return selected.is<Model::Type>() || selected.is<Alias>() ||
                   selected.is<Tetrodotoxin::Language::Import>()
               ? selected
               : Unknown::get_unknown();
  }
  return Unknown::get_unknown();
}

auto Types::Composite::resolve_concept(View::Bytes route) const
    -> const Abstract& {
  if (route == "static"_view) {
    return static_authority;
  }
  if (route == "instance"_view) {
    return instance_authority;
  }

  const Abstract& local = resolve_public_context(route);
  return !local.is<Unknown>() && !local.is<None>()
             ? local
             : get_host().resolve_concept(route);
}

auto Types::Composite::visit_concepts(
    Ttx::Concept::Abstract::Visitor visitor) const -> void {
  visitor("static"_view, static_authority);
  visitor("instance"_view, instance_authority);
}

auto Types::Composite::resolve_public_context(View::Bytes route) const
    -> const Abstract& {
  return resolve_binding(route, Category::Type, Visibility::Public);
}

auto Types::Composite::resolve_lexical_context(View::Bytes route) const
    -> const Abstract& {
  const Abstract& local = static_authority.resolve_concept(route);
  if (!local.is<Unknown>() && !local.is<None>()) {
    return local;
  }

  auto enclosing = get_host().select<Model::Type>();
  return enclosing ? enclosing->resolve_lexical_context(route)
                   : get_host().resolve_concept(route);
}

auto Types::Composite::is_externally_reachable(const Model::Type& type) const
    -> Bool {
  const Abstract& local =
      resolve_binding(type.get_name(), Category::Type, Visibility::Public);
  if (!local.is<Unknown>() && !local.is<None>()) {
    return &local.resolve() == &type;
  }

  auto enclosing = get_host().select<Model::Type>();
  if (enclosing) {
    return enclosing->is_externally_reachable(type);
  }

  return &get_host().resolve_concept(type.get_name()).resolve() == &type;
}

auto Types::Composite::get_layout() const -> const Ttx::Model::Layouts::Named& {
  return layout.visit(
      []() -> const Ttx::Model::Layouts::Named& { return empty_layout; },
      [](const Ttx::Model::Layouts::Named& selected)
          -> const Ttx::Model::Layouts::Named& { return selected; });
}
