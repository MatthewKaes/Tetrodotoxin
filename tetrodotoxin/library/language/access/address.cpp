// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/language/access/address.hpp"

#include "tetrodotoxin/library/language/diagnostics.hpp"
#include "tetrodotoxin/library/language/field.hpp"
#include "ttx/concept/unknown.hpp"

using namespace Perimortem;
using namespace Ttx::Concept;
using namespace Ttx::Lexical;
using namespace Ttx::Model;
using namespace Tetrodotoxin::Library;

auto Language::Access::Address::create_authored(
    Memory::Allocator::Arena& domain,
    Model::Pack& receiver,
    Token name_token,
    Core::View::Bytes name,
    Anchor anchor) -> Address& {
  return Expression::create_authored<Address>(
      domain, anchor, [&](auto source) -> Address {
        return Address(receiver, name_token, name, {}, source);
      });
}

auto Language::Access::Address::create_synthetic(
    Memory::Allocator::Arena& domain,
    Model::Pack& receiver,
    const Language::Model::Memory& selected) -> Address& {
  Core::Option<Reference<const Language::Model::Memory>> addressable{
    Reference<const Language::Model::Memory>(selected),
  };
  return Expression::create_synthetic<Address>(
      domain, [&](auto source) -> Address {
        return Address(receiver, {}, selected.get_name(), addressable, source);
      });
}

auto Language::Access::Address::link(
    Ttx::Lexical::Cursor& cursor,
    const Abstract& lexical_context,
    Core::Option<const Abstract&> access_scope) -> Bool {
  BAIL_IF(!receiver.link(cursor, lexical_context, access_scope));

  auto source_anchor = get_anchor();
  if (!name_token && addressable) {
    // Swizzle owns selection from arbitrary value flow. Its synthetic Address
    // already carries the exact selected Field, so it does not enter the
    // authored `.` receiver rules.
    return Expression::link(cursor, lexical_context, access_scope);
  }

  const Abstract& receiver_result = receiver.get_result();
  const Abstract& candidate = receiver_result.visit<Language::Model::Type>(
      [&](const Language::Model::Type& type) -> const Abstract& {
        return type.resolve_concept("static"_view).resolve_concept(name);
      },
      [&](const Abstract& receiver) -> const Abstract& {
        auto addressable = receiver.resolve().select<Ttx::Model::Addressable>();
        return addressable ? addressable->get_type()
                                 .resolve()
                                 .resolve_concept("instance"_view)
                                 .resolve_concept(name)
                           : receiver.resolve_concept("static"_view)
                                 .resolve_concept(name);
      });
  auto selected = candidate.resolve().select<Language::Model::Memory>();

  if (!selected) {
    auto report = cursor.create_report(source_anchor);
    report << "Receiver '"_view << receiver_result.get_name()
           << "' has no readable field named '"_view << name << "'. "_view
           << "Receiver type: "_view;
    Language::Diagnostics::write_type(report, receiver_result);
    report << "."_view;
    report.get_hint()
        << "Correct the field spelling or select a field exposed by this "
           "receiver."_view;
    return False;
  }

  auto field = selected->select<Language::Field>();
  if (field && field->get_definition().get_visibility() ==
                   Tetrodotoxin::Language::Visibility::Private) {
    const Abstract& caller = access_scope.visit(
        [&]() -> const Abstract& { return lexical_context; },
        [](const Abstract& selected) -> const Abstract& { return selected; });
    auto caller_type = caller.select<Language::Model::Type>();
    if (!caller_type ||
        !caller_type->has_private_access_to(field->get_host())) {
      cursor.create_expression_error(
          source_anchor, "Field is private to its declaring Type."_view,
          "Select the Field only from code hosted by that Type."_view);
      return False;
    }
  }

  if (addressable && &addressable->get() != &*selected) {
    auto report = cursor.create_report(source_anchor);
    report << "Internal semantic error: field access '"_view << name
           << "' changed identity from '"_view << addressable->get().get_name()
           << "' to '"_view << selected->get_name() << "'."_view;
    report.get_hint()
        << "The source is valid; report this unstable linking result."_view;
    return False;
  }

  addressable = Reference<const Language::Model::Memory>(*selected);
  if (source_anchor) {
    cursor.get_associations().create(*source_anchor, *selected);
  }
  return Expression::link(cursor, lexical_context, access_scope);
}

auto Language::Access::Address::get_documentation() const
    -> const Documentation& {
  return addressable.visit(
      []() -> const Documentation& { return Documentation::get_empty(); },
      [](const Reference<const Language::Model::Memory>& selected)
          -> const Documentation& {
        return selected.get().get_documentation();
      });
}

auto Language::Access::Address::get_type() const -> const Abstract& {
  return addressable.visit(
      []() -> const Abstract& { return Unknown::get_unknown(); },
      [](const Reference<const Language::Model::Memory>& selected)
          -> const Abstract& { return selected.get().get_type(); });
}

auto Language::Access::Address::get_result() const -> const Abstract& {
  return addressable.visit(
      []() -> const Abstract& { return Unknown::get_unknown(); },
      [](const Reference<const Language::Model::Memory>& selected)
          -> const Abstract& { return selected.get(); });
}

auto Language::Access::Address::finalize(Cursor& cursor) -> void {
  receiver.finalize(cursor);
  Expression::finalize(cursor);
}
