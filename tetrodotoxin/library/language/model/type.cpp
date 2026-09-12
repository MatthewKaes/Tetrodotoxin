// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/language/model/type.hpp"

#include "perimortem/core/diagnostics/log.hpp"

#include "tetrodotoxin/library/language/initialization.hpp"
#include "tetrodotoxin/library/language/model/callable.hpp"
#include "tetrodotoxin/library/language/value.hpp"
#include "ttx/concept/unknown.hpp"

using namespace Perimortem;
using namespace Ttx::Concept;
using Ttx::Semantic::Binding;
using namespace Tetrodotoxin::Library;

auto Language::Model::Type::complete_source(
    Tetrodotoxin::Source::Declaration::Phase phase,
    Ttx::Lexical::Cursor* cursor)
    -> Tetrodotoxin::Source::Declaration::Completion {
  using Phase = Tetrodotoxin::Source::Declaration::Phase;
  switch (phase) {
  case Phase::Type:
    return link_types(*cursor);
  case Phase::Initializer:
    return link_initializers(*cursor);
  case Phase::Signature:
    return link_callable_signatures(*cursor);
  case Phase::Body:
    return link_callable_bodies(*cursor);
  case Phase::Finalize:
    return finalize(*cursor);
  case Phase::RestoredType:
    return link_restored_types();
  case Phase::RestoredInitializer:
    return link_restored_initializers();
  case Phase::RestoredSignature:
    return link_restored_callable_signatures();
  default:
    return True;
  }
}

auto Language::Model::Type::bind_interface(Perimortem::System::Uuid requested)
    const -> Perimortem::Utility::Result<Binding, Binding::Failure> {
  if (requested == Tetrodotoxin::Source::Declaration::contract_id) {
    return Tetrodotoxin::Source::Declaration::provide(*this);
  }
  if (requested != Language::Initialization::contract_id) {
    return Ttx::Model::Type::bind_interface(requested);
  }
  if (resolve().is<Unknown>()) {
    return Binding::Failure::Pending;
  }

  static const Language::Initialization::Operations operations = {
    [](const void* source, Perimortem::Memory::Allocator::Arena& arena)
        -> Language::Initialization::Answer {
      const auto& type = *static_cast<const Type*>(source);
      auto produced = type.create_default(arena);
      if (!produced) {
        if (type.get_layout().is_empty()) {
          return Core::Option<Abstract::Handle>();
        }
        return Binding::Failure::Unsupported;
      }
      auto identity = produced->get_identity();
      if (!identity) {
        return Binding::Failure::Unsupported;
      }

      // A source expression still needs a terminal implementation before it
      // can serve as a stored initializer. Only an actual materialized Value
      // is admitted by this native provider until that publication exists.
      return identity->bind<Language::Value>().visit(
          [&](const Language::Value::Handle&)
              -> Language::Initialization::Answer {
            return Core::Option<Abstract::Handle>(identity->get_interface());
          },
          [](Binding::Failure failure) -> Language::Initialization::Answer {
            return failure;
          });
    },
  };
  return Binding::provide<Language::Initialization>(this, operations);
}

Language::Model::Type::Type(Perimortem::Memory::Allocator::Arena& domain) {
  initialize_authorities(domain);
}

auto Language::Model::Type::initialize_authorities(
    Perimortem::Memory::Allocator::Arena& domain) -> void {
  if (static_authority) {
    return;
  }

  static_authority = Core::Option<Reference<Language::Access::Static>>(
      Reference<Language::Access::Static>(
          domain.construct<Language::Access::Static>(domain)));
  instance_authority = Core::Option<Reference<Language::Access::Instance>>(
      Reference<Language::Access::Instance>(
          domain.construct<Language::Access::Instance>(domain)));
}

auto Language::Model::Type::edit_static_authority()
    -> Language::Access::Static& {
  return static_authority->get();
}

auto Language::Model::Type::edit_instance_authority()
    -> Language::Access::Instance& {
  return instance_authority->get();
}

auto Language::Model::Type::get_static_authority() const
    -> const Language::Access::Static& {
  return static_authority->get();
}

auto Language::Model::Type::get_instance_authority() const
    -> const Language::Access::Instance& {
  return instance_authority->get();
}

auto Language::Model::Type::resolve_concept(Core::View::Bytes route) const
    -> const Abstract& {
  if (route == "static"_view) {
    return static_authority
               ? static_cast<const Abstract&>(static_authority->get())
               : static_cast<const Abstract&>(None::get_none());
  }
  if (route == "instance"_view) {
    return instance_authority
               ? static_cast<const Abstract&>(instance_authority->get())
               : static_cast<const Abstract&>(None::get_none());
  }
  return Ttx::Model::Type::resolve_concept(route);
}

auto Language::Model::Type::visit_concepts(
    Ttx::Concept::Abstract::Visitor visitor) const -> void {
  if (!static_authority) {
    return;
  }
  visitor("static"_view, static_authority->get());
  visitor("instance"_view, instance_authority->get());
}

auto Language::Model::Type::get_callable_bindings(
    Tetrodotoxin::Language::Visibility visibility) const -> CallableBindings {
  const auto& selected =
      visibility == Tetrodotoxin::Language::Visibility::Private
          ? callables
          : published_callables;
  return selected ? selected->get_view() : CallableBindings();
}

auto Language::Model::Type::get_callables(
    Tetrodotoxin::Language::Visibility visibility) const -> Callables {
  return Callables(get_callable_bindings(visibility));
}

auto Language::Model::Type::can_publish_callable(
    const Abstract& candidate) const -> Bool {
  auto callable = candidate.select<Language::Model::Callable>();
  if (!callable || candidate.get_name().is_empty()) {
    return False;
  }

  Bool self = callable->declares_self();
  const Bool route_available =
      self ? (!instance_authority ||
              instance_authority->get().can_bind(candidate))
           : (!static_authority || static_authority->get().can_bind(candidate));
  BAIL_IF(!route_available);
  for (const Reference<Abstract>& retained : get_callables()) {
    auto existing = retained.get().select<Language::Model::Callable>();
    if (&retained.get() == &candidate ||
        (retained.get().get_name() == candidate.get_name() && existing &&
         existing->declares_self() == self)) {
      return False;
    }
  }

  return True;
}

auto Language::Model::Type::publish_callable(
    Perimortem::Memory::Allocator::Arena& domain,
    Abstract& callable,
    Bool published) -> void {
  if (!can_publish_callable(callable)) {
    Core::Diagnostics::Log::fatal(
        "Library Type cannot publish a duplicate or invalid Callable."_view);
  }

  if (!callables) {
    initialize_authorities(domain);
    callables =
        Perimortem::Memory::Managed::Vector<Reference<Abstract>>(domain);
    published_callables =
        Perimortem::Memory::Managed::Vector<Reference<Abstract>>(domain);
  }

  auto selected = callable.select<Language::Model::Callable>();
  if (!selected) {
    Core::Diagnostics::Log::fatal(
        "Library Type cannot publish a non Callable identity."_view);
  }
  Bool bound = selected->declares_self()
                   ? edit_instance_authority().bind(callable, published)
                   : edit_static_authority().bind(callable, published);
  if (!bound) {
    Core::Diagnostics::Log::fatal(
        "Library Type cannot publish an occupied Callable concept."_view);
  }

  callables->insert(callable);
  if (published) {
    published_callables->insert(callable);
  }
}
