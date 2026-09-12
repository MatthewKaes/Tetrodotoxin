// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/language/expression.hpp"

#include "tetrodotoxin/library/language/constant.hpp"
#include "tetrodotoxin/library/language/diagnostics.hpp"
#include "tetrodotoxin/library/language/model/memory.hpp"
#include "ttx/concept/constant.hpp"
#include "ttx/concept/domain.hpp"
#include "ttx/concept/none.hpp"
#include "ttx/concept/reference.hpp"
#include "ttx/model/layouts/fluid.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Concept;
using Ttx::Semantic::Binding;
using namespace Tetrodotoxin::Library;

auto Language::Expression::bind_interface(Perimortem::System::Uuid requested)
    const -> Perimortem::Utility::Result<Binding, Binding::Failure> {
  if (requested == Domain::contract_id) {
    static const Domain::Operations operations = {
      [](const void* source, ttx_abstract* output) -> ttx_binding_status {
        const auto& expression = *static_cast<const Expression*>(source);
        const Abstract& result = expression.get_result();
        if (&result != &expression) {
          return result.bind<Domain>().visit(
              [&](const Domain::Handle& domain) -> ttx_binding_status {
                return domain.get_domain().visit(
                    [&](Abstract::Handle answer) -> ttx_binding_status {
                      *output = answer.get_abi();
                      return TTX_BINDING_SATISFIED;
                    },
                    [](Binding::Failure failure) -> ttx_binding_status {
                      return static_cast<ttx_binding_status>(failure);
                    });
              },
              [](Binding::Failure failure) -> ttx_binding_status {
                return static_cast<ttx_binding_status>(failure);
              });
        }
        const Abstract& type = expression.get_type();
        if (&type == &Unknown::get_unknown()) {
          return TTX_BINDING_PENDING;
        }
        *output = type.get_interface().get_abi();
        return TTX_BINDING_SATISFIED;
      },
    };
    return Binding::provide<Domain>(this, operations);
  }
  return Abstract::bind_interface(requested);
}

static auto select_output_type(const Abstract& candidate)
    -> Option<const Language::Model::Type&> {
  auto direct = candidate.select<Language::Model::Type>();
  return direct ? direct : candidate.resolve().select<Language::Model::Type>();
}

static auto select_value_type(const Abstract& candidate)
    -> Option<const Language::Model::Type&> {
  auto type = select_output_type(candidate);
  BAIL_IF(!type || type->get_layout().is_empty());
  return *type;
}

static auto select_layout_type(const Abstract& candidate)
    -> Option<const Language::Model::Type&> {
  auto type = candidate.select<Language::Model::Type>();
  if (type) {
    return *type;
  }

  const Abstract& resolved = candidate.resolve();
  auto addressable = resolved.select<Ttx::Model::Addressable>();
  return addressable ? addressable->get_type().select<Language::Model::Type>()
                     : resolved.select<Language::Model::Type>();
}

static auto has_exact_representation(
    const Language::Model::Pack& representation,
    const Language::Model::Type& type) -> Bool {
  const Layout& required = type.get_layout();
  const Layout& supplied = representation.get_layout();
  BAIL_IF(required.get_size() != supplied.get_size());

  for (Count index = 0; index < required.get_size(); index++) {
    auto required_entry = required.get_abstract(index);
    BAIL_IF(!required_entry);
    auto required_type = select_layout_type(*required_entry);
    auto supplied_type =
        select_layout_type(representation.get_value_type(index));
    BAIL_IF(
        !required_type || !supplied_type || &*required_type != &*supplied_type);
  }
  return True;
}

static constexpr Ttx::Model::Layouts::Fluid empty_expression_layout;

auto Language::Expression::Error::from_pack(
    Type type,
    const Language::Model::Pack& pack) -> Error {
  auto identity = pack.get_identity();
  return Error(type, identity ? *identity : Unknown::get_unknown());
}

auto Language::Expression::get_layout() const -> const Layout& {
  auto type = select_value_type(get_type());
  // Layout inspection is total even when this Expression has no value output.
  // Pack resolution remains the separate admission proof, so this empty shape
  // cannot fit storage or masquerade as completed zero value flow.
  if (!type || type->get_layout().is_empty()) {
    return empty_expression_layout;
  }
  return output_layout;
}

auto Language::Expression::resolve() const -> const Abstract& {
  auto type = select_value_type(get_type());
  if (!type) {
    return Unknown::get_unknown();
  }

  return *this;
}

auto Language::Expression::resolve_concept(View::Bytes name) const
    -> const Abstract& {
  if (name != "folded"_view) {
    return Abstract::resolve_concept(name);
  }

  return folded.visit(
      []() -> const Abstract& { return Unknown::get_unknown(); },
      [](const Language::Model::Pack& representation) -> const Abstract& {
        auto identity = representation.get_identity();
        return identity && identity->is<Ttx::Concept::Constant>()
                   ? *identity
                   : static_cast<const Abstract&>(None::get_none());
      },
      [](const Error&) -> const Abstract& { return None::get_none(); });
}

auto Language::Expression::visit_concepts(
    Ttx::Concept::Abstract::Visitor visitor) const -> void {
  const Abstract& folded = resolve_concept("folded"_view);
  visitor("folded"_view, folded);
}

auto Language::Expression::get_value_type(Count index) const
    -> const Abstract& {
  if (index != 0) {
    return Unknown::get_unknown();
  }
  return select_value_type(get_type())
      .visit(
          []() -> const Abstract& { return Unknown::get_unknown(); },
          [](const Language::Model::Type& type) -> const Abstract& {
            return type;
          });
}

auto Language::Expression::finalize(Ttx::Lexical::Cursor&) -> void {
  fold();
}

auto Language::Expression::link(
    Ttx::Lexical::Cursor& cursor,
    const Abstract&,
    Option<const Abstract&>) -> Bool {
  auto source_anchor = get_anchor();

  // A declaration Type's identity and immutable Layout are available before
  // its recursive closure settles. Admitting that exact edge here preserves
  // forward references while the Monograph barrier still prevents an invalid
  // Type or Expression from escaping the source transaction.
  auto type = select_value_type(get_type());
  if (type) {
    return True;
  }

  cursor.create_expression_error(
      source_anchor, "Expression did not resolve one nonempty Type."_view,
      "Complete its semantic inputs or keep empty output as Pack flow."_view);
  return False;
}

auto Language::Expression::Error::get_name() const -> View::Bytes {
  switch (type) {
  case Type::InvalidOperationType:
    return "invalid operation Type"_view;
  case Type::InvalidInput:
    return "invalid operation input"_view;
  case Type::InvalidConstant:
    return "invalid Tetrodotoxin::Library::Language::Constant domain"_view;
  case Type::ResultTypeMismatch:
    return "changed result Type"_view;
  case Type::ArithmeticOverflow:
    return "arithmetic overflow"_view;
  case Type::DivisionByZero:
    return "division by zero"_view;
  case Type::Unknown:
    return "unknown fold failure"_view;
  }

  return "unknown fold failure"_view;
}

auto Language::Expression::fold() -> Perimortem::Utility::
    Result<Perimortem::Core::Option<Language::Model::Pack&>, Error> {
  if (!folded.is_null()) {
    return folded.visit(
        []() -> Perimortem::Utility::Result<
                 Perimortem::Core::Option<Language::Model::Pack&>, Error> {
          return Perimortem::Core::Option<Language::Model::Pack&>{};
        },
        [](Language::Model::Pack& representation)
            -> Perimortem::Utility::Result<
                Perimortem::Core::Option<Language::Model::Pack&>, Error> {
          return representation;
        },
        [](const Error& error)
            -> Perimortem::Utility::Result<
                Perimortem::Core::Option<Language::Model::Pack&>, Error> {
          return error;
        });
  }

  if (&resolve() == &Unknown::get_unknown()) {
    return Perimortem::Core::Option<Language::Model::Pack&>{};
  }

  auto result = evaluate();
  return result.visit(
      [&](const Perimortem::Core::Option<Language::Model::Pack&>& selected)
          -> Perimortem::Utility::Result<
              Perimortem::Core::Option<Language::Model::Pack&>, Error> {
        if (!selected) {
          return Perimortem::Core::Option<Language::Model::Pack&>{};
        }

        Language::Model::Pack& representation = *selected;
        const Layout& representation_layout = representation.get_layout();
        Bool constants = True;
        for (Count index = 0; index < representation_layout.get_size();
             index++) {
          auto entry = representation_layout.get_abstract(index);
          constants &= Bool(
              entry && entry->is<Tetrodotoxin::Library::Language::Constant>());
        }

        if (!constants) {
          Error error(Error::Type::InvalidConstant, *this);
          folded = error;
          return error;
        }

        const Layout& source_layout = get_layout();
        Bool exact_shape = False;
        if (source_layout.get_size() == 1) {
          auto expression_type =
              get_type().resolve().select<Language::Model::Type>();
          if (expression_type && representation_layout.get_size() == 1) {
            const Abstract& result_type = representation.get_type().resolve();
            exact_shape = result_type.is<Language::Model::Type>() &&
                          &*expression_type == &result_type;
          } else if (expression_type) {
            exact_shape =
                has_exact_representation(representation, *expression_type);
          }
        } else if (
            source_layout.get_size() == representation_layout.get_size()) {
          // The authored Layout owns the output promise. A folded Pack may
          // replace a repeated producer (such as one Slice identity) with its
          // concrete Tetrodotoxin::Library::Language::Constant entries, so the
          // reverse fit is not meaningful: it would ask those Constants to
          // reproduce the source owner.
          exact_shape = source_layout.fits(representation_layout);
        }

        if (!exact_shape) {
          Error error(Error::Type::ResultTypeMismatch, *this);
          folded = error;
          return error;
        }

        folded = representation;
        return Perimortem::Core::Option<Language::Model::Pack&>(representation);
      },
      [&](const Error& error)
          -> Perimortem::Utility::Result<
              Perimortem::Core::Option<Language::Model::Pack&>, Error> {
        folded = error;
        return error;
      });
}

auto Language::Expression::fold(Language::Model::Pack& pack) -> Perimortem::
    Utility::Result<Perimortem::Core::Option<Language::Model::Pack&>, Error> {
  if (pack.select_identity<Language::Constant>()) {
    return Perimortem::Core::Option<Language::Model::Pack&>(pack);
  }
  auto expression = pack.select_identity<Expression>();
  return expression
             ? expression->fold()
             : Perimortem::Utility::Result<
                   Perimortem::Core::Option<Language::Model::Pack&>, Error>(
                   Perimortem::Core::Option<Language::Model::Pack&>());
}

auto Language::Expression::get_write_type(
    const Language::Model::Type& access_scope) const
    -> Option<const Language::Model::Type&> {
  auto addressable = get_result().resolve().select<Language::Model::Memory>();
  auto type = addressable
                  ? addressable->get_type().select<Language::Model::Type>()
                  : Option<const Language::Model::Type&>();
  return addressable && type && addressable->permits_write_from(access_scope)
             ? type
             : Option<const Language::Model::Type&>();
}

auto Language::Expression::link_write(
    Ttx::Lexical::Cursor& cursor,
    const Abstract& lexical_context,
    const Language::Model::Type& access_scope,
    Language::Model::Pack& source) -> Bool {
  Bool failed = !link_write_target(cursor, lexical_context, access_scope);
  failed |= !source.link(cursor, lexical_context, access_scope);
  BAIL_IF(failed);

  if (!source.is_complete()) {
    auto report = cursor.create_report(get_anchor());
    auto source_identity = source.get_identity();
    report << "Cannot write incomplete source '"_view
           << (source_identity ? source_identity->get_name() : "Pack"_view)
           << "' to target '"_view << get_name() << "'."_view;
    report.get_hint()
        << "Fix the source expression before assigning its value."_view;
    return False;
  }

  if (!accepts_write(source, access_scope)) {
    auto report = cursor.create_report(get_anchor());
    report << "Cannot write source values to target '"_view << get_name()
           << "'.\nSource produces: "_view;
    Language::Diagnostics::write_pack(report, source);
    auto target_type = get_write_type(access_scope);
    if (target_type) {
      report << "\nTarget '"_view << target_type->get_name()
             << "' accepts: "_view;
      Language::Diagnostics::write_layout(report, target_type->get_layout());
    }
    report.get_hint()
        << "Supply values with the exact count and Types shown for the target."_view;
    return False;
  }

  return True;
}

auto Language::Expression::link_write_restored(
    const Abstract& lexical_context,
    const Language::Model::Type& access_scope,
    Language::Model::Pack& source) -> Bool {
  Bool failed = !link_write_target_restored(lexical_context, access_scope);
  failed |= !source.link_restored(lexical_context, access_scope);
  BAIL_IF(failed || !source.is_complete());
  return accepts_write(source, access_scope);
}

auto Language::Expression::link_write_target(
    Ttx::Lexical::Cursor& cursor,
    const Abstract& lexical_context,
    const Language::Model::Type& access_scope) -> Bool {
  return link(cursor, lexical_context, access_scope);
}

auto Language::Expression::link_write_target_restored(
    const Abstract& lexical_context,
    const Language::Model::Type& access_scope) -> Bool {
  return link_restored(lexical_context, access_scope);
}

auto Language::Expression::accepts_write(
    const Language::Model::Pack& source,
    const Language::Model::Type& access_scope) const -> Bool {
  auto target_type = get_write_type(access_scope);
  return target_type && source.fits_into(*target_type);
}

auto Language::Expression::get_folded()
    -> Perimortem::Core::Option<Language::Model::Pack&> {
  return folded.visit(
      []() -> Perimortem::Core::Option<Language::Model::Pack&> { return {}; },
      [](Language::Model::Pack& representation)
          -> Perimortem::Core::Option<Language::Model::Pack&> {
        return representation;
      },
      [](const Error&) -> Perimortem::Core::Option<Language::Model::Pack&> {
        return {};
      });
}

auto Language::Expression::get_folded() const
    -> Perimortem::Core::Option<const Language::Model::Pack&> {
  return folded.visit(
      []() -> Perimortem::Core::Option<const Language::Model::Pack&> {
        return {};
      },
      [](const Language::Model::Pack& representation)
          -> Perimortem::Core::Option<const Language::Model::Pack&> {
        return representation;
      },
      [](const Error&)
          -> Perimortem::Core::Option<const Language::Model::Pack&> {
        return {};
      });
}

auto Language::Expression::evaluate() -> Perimortem::Utility::
    Result<Perimortem::Core::Option<Language::Model::Pack&>, Error> {
  if (is<Tetrodotoxin::Library::Language::Constant>()) {
    return static_cast<Language::Model::Pack&>(*this);
  }

  const Abstract& result = get_result();
  auto constant =
      result.resolve().select<Tetrodotoxin::Library::Language::Constant>();
  if (constant) {
    return static_cast<Language::Model::Pack&>(
        const_cast<Tetrodotoxin::Library::Language::Constant&>(*constant));
  }

  auto addressable = result.resolve().select<Language::Model::Memory>();
  return addressable ? addressable->get_constant()
                     : Option<Language::Model::Pack&>();
}
