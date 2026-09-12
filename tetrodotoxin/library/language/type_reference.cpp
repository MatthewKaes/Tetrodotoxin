// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/language/type_reference.hpp"

#include "perimortem/core/static/union.hpp"

#include "perimortem/memory/dynamic/vector.hpp"
#include "perimortem/memory/managed/vector.hpp"

#include "tetrodotoxin/language/import.hpp"
#include "tetrodotoxin/language/monograph.hpp"
#include "tetrodotoxin/library/language/constants/false.hpp"
#include "tetrodotoxin/library/language/constants/signed.hpp"
#include "tetrodotoxin/library/language/constants/true.hpp"
#include "tetrodotoxin/library/language/constants/unsigned.hpp"
#include "tetrodotoxin/library/language/expression.hpp"
#include "tetrodotoxin/library/language/generic.hpp"
#include "tetrodotoxin/library/language/model/type.hpp"
#include "tetrodotoxin/library/language/model/types/flag.hpp"
#include "tetrodotoxin/library/language/model/types/signed.hpp"
#include "tetrodotoxin/library/language/model/types/unsigned.hpp"
#include "ttx/concept/domain.hpp"
#include "ttx/concept/none.hpp"
#include "ttx/concept/reference.hpp"
#include "ttx/concept/unknown.hpp"
#include "ttx/model/layouts/fluid.hpp"

using namespace Perimortem;
using namespace Ttx::Concept;
using Ttx::Semantic::Binding;
using namespace Ttx::Lexical;
using namespace Tetrodotoxin::Library;

auto Language::TypeReference::get_interface() const -> Abstract::Handle {
  static const Abstract::Operations operations = {
    [](const void* source, perimortem_uuid requested,
       ttx_binding* result) -> ttx_binding_status {
      if (requested.high == TTX_ABSTRACT_ID_HIGH &&
          requested.low == TTX_ABSTRACT_ID_LOW) {
        *result = {source, &operations};
        return TTX_BINDING_SATISFIED;
      }
      return static_cast<const TypeReference*>(source)
          ->bind_interface(System::Uuid(requested))
          .visit(
              [&](const Binding& binding) -> ttx_binding_status {
                *result = binding.get_abi();
                return TTX_BINDING_SATISFIED;
              },
              [](Binding::Failure failure) -> ttx_binding_status {
                return static_cast<ttx_binding_status>(failure);
              });
    },
    [](const void* source) -> perimortem_view_bytes {
      const auto route = static_cast<const TypeReference*>(source)->get_route();
      return {route.get_data(), route.get_size()};
    },
    [](const void*) -> ttx_documentation {
      return Documentation::get_empty().get_interface().get_abi();
    },
    [](const void* source) -> ttx_abstract {
      return static_cast<const TypeReference*>(source)
          ->get_interface()
          .get_abi();
    },
    [](const void* source, perimortem_view_bytes name) -> ttx_abstract {
      return static_cast<const TypeReference*>(source)
          ->resolve_concept({name.data, name.size})
          .get_interface()
          .get_abi();
    },
    [](const void* source, ttx_concept_visitor visitor) {
      auto receive = [&](Core::View::Bytes name, const Abstract& value) {
        visitor.receive(
            visitor.source, {name.get_data(), name.get_size()},
            value.get_interface().get_abi());
      };
      static_cast<const TypeReference*>(source)->visit_concepts(
          Abstract::Visitor(receive));
    },
  };
  return Abstract::Handle(this, operations);
}

auto Language::TypeReference::bind_interface(Perimortem::System::Uuid requested)
    const -> Perimortem::Utility::Result<Binding, Binding::Failure> {
  if (requested == Domain::contract_id) {
    if (subject == nullptr) {
      return Binding::Failure::Pending;
    }
    if (!domain) {
      Core::Option<Binding::Failure> failure;
      subject->bind<Domain>().visit(
          [&](const Domain::Handle& selected) { domain = selected; },
          [&](Binding::Failure rejected) { failure = rejected; });
      if (failure) {
        return *failure;
      }
    }
    return Domain::provide(*this);
  }
  using Import = Tetrodotoxin::Language::Import;
  if (requested != Import::contract_id || !dependency) {
    if (!subject) {
      return Binding::Failure::Pending;
    }
    return subject->bind_interface(requested);
  }

  // An imported generator needs its argument recipe as well as a name path.
  // Until that projection is defined, declining it is the only answer that
  // does not misidentify the generated Type as the generator itself.
  if (arguments) {
    return Binding::Failure::Rejected;
  }

  static const Import::Operations operations = {
    [](const void* source) -> Import::Kind {
      return static_cast<const TypeReference*>(source)->dependency->get_kind();
    },
    [](const void* source) -> Core::View::Bytes {
      return static_cast<const TypeReference*>(source)
          ->dependency->get_locator();
    },
    [](const void* source) -> System::Version {
      return static_cast<const TypeReference*>(source)
          ->dependency->get_version();
    },
    [](const void* source) -> Count {
      const auto& reference = *static_cast<const TypeReference*>(source);
      return reference.dependency->get_access_count() + reference.get_size() -
             reference.dependency_suffix;
    },
    [](const void* source, Count index) -> Core::Option<Core::View::Bytes> {
      const auto& reference = *static_cast<const TypeReference*>(source);
      const Count imported = reference.dependency->get_access_count();
      if (index < imported) {
        return reference.dependency->get_access(index);
      }
      const Count suffix = index - imported;
      if (suffix >= reference.get_size() - reference.dependency_suffix) {
        return {};
      }
      return reference.get_name(reference.dependency_suffix + suffix);
    },
  };
  return Binding::provide<Import>(this, operations);
}

auto Language::TypeReference::get_domain() const -> Domain::Answer {
  return domain->get_domain().visit(
      [&](Abstract::Handle selected) -> Domain::Answer {
        // A self domain can keep this reference's dependency and access path.
        // A provider that supplies another domain owns that returned edge,
        // so its answer passes through without substituting our native Type.
        return selected.get_identity() ==
                       subject->get_interface().get_identity()
                   ? get_interface()
                   : selected;
      },
      [](Binding::Failure failure) -> Domain::Answer { return failure; });
}

auto Language::TypeReference::resolve_concept(Core::View::Bytes name) const
    -> const Abstract& {
  return subject ? subject->resolve_concept(name) : Unknown::get_unknown();
}

auto Language::TypeReference::visit_concepts(Abstract::Visitor visitor) const
    -> void {
  if (subject) {
    subject->visit_concepts(visitor);
  }
}

static auto is_missing(const Abstract& abstract) -> Bool {
  return abstract.is<Unknown>() || abstract.is<None>();
}

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

auto Language::TypeReference::get_size() const -> Count {
  if (route.is_empty()) {
    return 0;
  }

  Count segments = 1;
  for (Count index = 0; index + 1 < route.get_size(); index++) {
    if (route[index] == ':' && route[index + 1] == ':') {
      segments++;
      index++;
    }
  }
  return segments;
}

auto Language::TypeReference::get_name(Count requested) const
    -> Core::View::Bytes {
  Count segment = 0;
  Count start = 0;
  for (Count index = 0; index <= route.get_size(); index++) {
    Bool end = index == route.get_size();
    Bool separator = !end && index + 1 < route.get_size() &&
                     route[index] == ':' && route[index + 1] == ':';
    if (!end && !separator) {
      continue;
    }
    if (segment == requested) {
      return route.slice(start, index - start);
    }
    if (end) {
      return {};
    }
    segment++;
    index++;
    start = index + 1;
  }

  return {};
}

auto Language::TypeReference::get_token(Count requested) const -> Token {
  if (requested + 1 == get_size() && terminal) {
    return terminal;
  }
  Core::View::Bytes name = get_name(requested);
  Token first = anchor.get_token();
  if (!first || name.is_empty()) {
    return {};
  }

  Count offset = Count(name.get_data() - route.get_data());
  return Token(
      U16(Count(first.get_offset()) + offset), first.get_line(),
      U16(Count(first.get_column()) + offset), U8(name.get_size()),
      first.get_code());
}

auto Language::TypeReference::matches_route(const TypeReference& other) const
    -> Bool {
  return route == other.route;
}

auto Language::TypeReference::get_argument_reference(Count index) const
    -> Core::Option<const TypeReference&> {
  BAIL_IF(!arguments || index >= arguments->get_size());
  const TypeReference* reference =
      arguments->get_data()[index].find<const TypeReference&>();
  BAIL_IF(!reference);
  return *reference;
}

auto Language::TypeReference::get_argument(Count index) const
    -> Core::Option<const Argument&> {
  BAIL_IF(!arguments || index >= arguments->get_size());
  return arguments->get_data()[index];
}

static auto map_failure(
    Anchor anchor,
    const Language::Generic::Failure& failure)
    -> Language::TypeReference::Failure {
  switch (failure.get_type()) {
  case Language::Generic::Failure::Type::Unavailable:
    return Language::TypeReference::Failure(
        Language::TypeReference::Failure::Type::Unavailable, anchor);
  case Language::Generic::Failure::Type::Arity:
    return Language::TypeReference::Failure(
        Language::TypeReference::Failure::Type::Arity, anchor);
  case Language::Generic::Failure::Type::Parameter:
    return Language::TypeReference::Failure(
        Language::TypeReference::Failure::Type::Parameter, anchor,
        failure.get_argument());
  case Language::Generic::Failure::Type::Recursive:
    return Language::TypeReference::Failure(
        Language::TypeReference::Failure::Type::Recursive, anchor);
  case Language::Generic::Failure::Type::Formula:
    return Language::TypeReference::Failure(
        Language::TypeReference::Failure::Type::Formula, anchor);
  }

  return Language::TypeReference::Failure(
      Language::TypeReference::Failure::Type::Formula, anchor);
}

auto Language::TypeReference::resolve_with_root(
    const Abstract& context,
    Root root,
    Core::Option<Cursor&> cursor) const -> Resolution {
  Core::Option<Tetrodotoxin::Language::Import::Handle> encountered;
  Core::Option<Binding::Failure> boundary_failure;
  Count suffix = 0;
  auto retain_boundary = [&](const Abstract& candidate, Count next) {
    if (encountered) {
      return;
    }
    candidate.bind<Tetrodotoxin::Language::Import>().visit(
        [&](const Tetrodotoxin::Language::Import::Handle& import) {
          encountered = import;
          suffix = next;
        },
        [&](Binding::Failure failure) {
          if (failure != Binding::Failure::Unsupported) {
            boundary_failure = failure;
          }
        });
  };
  // The declaration context gives the root name its lexical authority. Each
  // explicit suffix then asks the identity selected by the preceding segment.
  const Abstract* selected = &context.resolve_concept(get_root());
  if (root == Root::Lexical) {
    auto type = context.select<Language::Model::Type>();
    if (type) {
      selected = &type->resolve_lexical_context(get_root());
    } else {
      auto monograph = context.select<Tetrodotoxin::Language::Monograph>();
      if (monograph) {
        selected = &monograph->resolve_lexical_context(get_root());
      }
    }
  }
  if (is_missing(*selected)) {
    return Failure(Failure::Type::Route, anchor, 0);
  }
  if (cursor && get_size() > 1) {
    Token token = get_token(0);
    cursor->get_associations().create(
        Anchor::create(token, Span(token)), *selected);
  }

  for (Count i = 1; i < get_size(); i++) {
    retain_boundary(*selected, i);
    if (boundary_failure) {
      return Failure(Failure::Type::Unavailable, anchor, i - 1);
    }
    // Navigation stays on the encountered subject so Import can apply its
    // policy. A native Type is selected only after the full access path has
    // answered, otherwise the compiler could bypass a restricted route.
    const Abstract& route_context = *selected;
    if (is_missing(route_context)) {
      return Failure(Failure::Type::Route, anchor, i - 1);
    }

    selected = &route_context.resolve_concept(get_name(i));
    if (is_missing(*selected)) {
      return Failure(Failure::Type::Route, anchor, i);
    }
    if (cursor && i + 1 < get_size()) {
      Token token = get_token(i);
      cursor->get_associations().create(
          Anchor::create(token, Span(token)), *selected);
    }
  }

  retain_boundary(*selected, get_size());
  if (boundary_failure) {
    return Failure(Failure::Type::Unavailable, anchor, get_size() - 1);
  }
  if (!arguments) {
    const Abstract& direct = select_native(*selected);
    const Abstract& resolved = direct;
    if (is_missing(resolved)) {
      return Failure(Failure::Type::Route, anchor, get_size() - 1);
    }

    if (cursor) {
      Token token = get_token(get_size() - 1);
      cursor->get_associations().create(
          Anchor::create(token, Span(token)), *selected);
    }
    if (target && (target != &resolved || subject != selected)) {
      return Failure(Failure::Type::Unavailable, anchor);
    }
    // Native selection can step past an Import or authored declaration. Keep
    // the subject that supplied that answer for all later semantic questions.
    subject = selected;
    target = &resolved;
    dependency = encountered;
    dependency_suffix = suffix;
    return resolved;
  }

  const Abstract& resolved = select_native(*selected);
  if (is_missing(resolved)) {
    return Failure(Failure::Type::Route, anchor, get_size() - 1);
  }
  auto generic = resolved.select<Generic>();
  if (!generic) {
    return Failure(Failure::Type::Generic, anchor);
  }
  if (cursor) {
    // The authored name still denotes the Generic even though applying its
    // arguments returns a materialized Type. Recording the terminal Token lets
    // editor tooling show that distinction with the same identity selected by
    // resolution.
    Token token = get_token(get_size() - 1);
    cursor->get_associations().create(
        Anchor::create(token, Span(token)), *generic);
  }

  // Resolution assembles one temporary Layout from the real argument
  // identities. Generic copies its normalized key into its own Arena before
  // this storage leaves, and nested routes follow the same root access policy.
  Memory::Dynamic::Vector<Reference<const Abstract>> linked(
      arguments->get_size());
  const auto* argument_data = arguments->get_data();
  for (Count i = 0; i < arguments->get_size(); i++) {
    const Argument& argument = argument_data[i];
    const TypeReference* reference = argument.find<const TypeReference&>();
    if (reference) {
      Core::Option<const Abstract&> nested;
      Core::Option<Failure> nested_failure;
      reference->resolve_with_root(context, root, cursor)
          .visit(
              [&](const Abstract& resolved) {
                nested = select_native(resolved);
              },
              [&](const Failure& failure) { nested_failure = failure; });
      if (nested_failure) {
        return *nested_failure;
      }
      if (!nested || !nested->is<Ttx::Model::Type>()) {
        return Failure(Failure::Type::Argument, anchor, i);
      }
      linked.insert(*nested);
      continue;
    }

    const Abstract* literal = argument.find<const Abstract&>();
    if (!literal) {
      return Failure(Failure::Type::Argument, anchor, i);
    }
    linked.insert(*literal);
  }

  Ttx::Model::Layouts::Fluid layout(linked.get_view());
  return generic->materialize(layout).visit(
      [&](const Language::Model::Type& type) -> Resolution {
        if (cursor) {
          cursor->get_associations().create(anchor, type);
        }
        if (target && target != &type) {
          return Failure(Failure::Type::Unavailable, anchor);
        }
        subject = &type;
        target = &type;
        dependency = encountered;
        dependency_suffix = suffix;
        return type;
      },
      [&](const Generic::Failure& failure) -> Resolution {
        // Generic knows which formula parameter failed, while TypeReference
        // knows where that argument was written. Joining those facts gives the
        // diagnostic the right authored Anchor.
        Anchor failure_anchor = anchor;
        Count index = failure.get_argument();
        if (failure.get_type() == Generic::Failure::Type::Parameter &&
            index < arguments->get_size()) {
          const Argument& argument = arguments->get_data()[index];
          const TypeReference* reference =
              argument.find<const TypeReference&>();
          if (reference) {
            failure_anchor = reference->get_anchor();
          } else {
            const Abstract* literal = argument.find<const Abstract&>();
            auto pack = literal ? Model::Pack::from(*literal)
                                : Core::Option<const Model::Pack&>();
            if (pack && pack->get_anchor()) {
              failure_anchor = *pack->get_anchor();
            }
          }
        }
        return map_failure(failure_anchor, failure);
      });
}

auto Language::TypeReference::resolve(const Abstract& context) const
    -> Resolution {
  return resolve_with_root(context, Root::Context);
}

auto Language::TypeReference::resolve_lexical(const Abstract& context) const
    -> Resolution {
  return resolve_with_root(context, Root::Lexical);
}

auto Language::TypeReference::resolve_authored(
    Cursor& cursor,
    const Abstract& context) const -> Core::Option<const Abstract&> {
  Core::Option<const Abstract&> selected;
  resolve_with_root(context, Root::Lexical, cursor)
      .visit(
          [&](const Abstract& resolved) { selected = resolved; },
          [&](const Failure& failure) { report(cursor, failure); });
  return selected;
}

auto Language::TypeReference::report(Cursor& cursor, const Failure& failure)
    const -> void {
  switch (failure.get_type()) {
  case Failure::Type::Route: {
    auto report = cursor.create_report(failure.get_anchor());
    report << "Library route segment "_view << U64(failure.get_index() + 1)
           << " did not resolve in its selected context."_view;
    report.get_hint()
        << "Publish that exact name before linking this declaration."_view;
    return;
  }
  case Failure::Type::Argument: {
    auto report = cursor.create_report(failure.get_anchor());
    report << "Library Generic argument "_view << U64(failure.get_index() + 1)
           << " did not resolve to one Library Type."_view;
    report.get_hint()
        << "Use a Type route or one literal accepted by this Generic."_view;
    return;
  }
  case Failure::Type::Generic:
    cursor.create_expression_error(
        failure.get_anchor(),
        "Library Type arguments require a Generic at the route terminal."_view,
        "Remove the arguments or select one named Generic."_view);
    return;
  case Failure::Type::Unavailable:
    cursor.create_expression_error(
        failure.get_anchor(),
        "Library Generic is not ready for materialization."_view,
        "Complete the selected Generic before applying arguments."_view);
    return;
  case Failure::Type::Arity:
    cursor.create_expression_error(
        failure.get_anchor(),
        "Library Generic application has the wrong number of arguments."_view,
        "Supply exactly the parameters declared by the selected Generic."_view);
    return;
  case Failure::Type::Parameter: {
    auto report = cursor.create_report(failure.get_anchor());
    report << "Library Generic argument "_view << U64(failure.get_index() + 1)
           << " does not satisfy its parameter category."_view;
    report.get_hint()
        << "Use the Type or scalar Constant category required at this position."_view;
    return;
  }
  case Failure::Type::Recursive:
    cursor.create_expression_error(
        failure.get_anchor(),
        "Library Generic application is recursively self dependent."_view,
        "Break the materialization cycle with one already completed Type."_view);
    return;
  case Failure::Type::Formula:
    cursor.create_expression_error(
        failure.get_anchor(),
        "Library Generic rejected this argument combination."_view,
        "Use values admitted by the selected Generic formula."_view);
    return;
  }
}
