// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/language/flow/match.hpp"

#include "tetrodotoxin/library/language/expressions/identifier.hpp"
#include "tetrodotoxin/library/language/model/types/flag.hpp"
#include "tetrodotoxin/library/language/types/option.hpp"
#include "ttx/concept/none.hpp"
#include "ttx/concept/unknown.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Ttx::Concept;
using namespace Ttx::Lexical;
using namespace Ttx::Model;
using namespace Tetrodotoxin::Library;

class Payload final : public Language::Model::Memory {
 public:
  TTX_CONTRACT(Payload, Language::Model::Memory);

  constexpr Payload(View::Bytes name) : name(name) {}

  TTX_NAME(name);
  TTX_EMPTY_DOCUMENTATION();

  auto bind(const Language::Model::Type& selected) -> Bool {
    if (type && &type->get() != &selected) {
      return False;
    }
    type = Reference<const Language::Model::Type>(selected);
    return True;
  }

  auto resolve() const -> const Abstract& override {
    return type ? static_cast<const Abstract&>(*this)
                : static_cast<const Abstract&>(Unknown::get_unknown());
  }

  auto get_type() const -> const Language::Model::Type& override {
    return type->get();
  }

 private:
  View::Bytes name;
  Option<Reference<const Language::Model::Type>> type;
};

class PatternContext final : public Abstract {
 public:
  TTX_CONTRACT(PatternContext, Abstract);

  constexpr PatternContext(const Abstract& parent, Payload& payload)
      : parent(parent), payload(payload) {}

  TTX_NAME("Option pattern"_view);
  TTX_EMPTY_DOCUMENTATION();

  auto resolve_concept(View::Bytes route) const -> const Abstract& override {
    if (route == payload.get_name() &&
        &payload.resolve() != &Unknown::get_unknown()) {
      return payload;
    }

    return parent.resolve_concept(route);
  }

 private:
  const Abstract& parent;
  Payload& payload;
};

auto Language::Flow::Match::create_authored(
    Allocator::Arena& domain,
    Model::Pack& input,
    Anchor anchor) -> Match& {
  return domain.construct_from<Match>(
      [&]() -> Match { return Match(domain, input, anchor); });
}

auto Language::Flow::Match::create_pattern(
    Allocator::Arena& domain,
    const Abstract& parent,
    View::Bytes name) -> Pattern {
  auto& payload = domain.construct<Payload>(name);
  auto& context = domain.construct<PatternContext>(parent, payload);
  return Pattern(context, payload);
}

auto Language::Flow::Match::retain_value_case(
    Model::Pack& value,
    Block& body,
    Model::Memory& payload,
    Anchor anchor) -> void {
  cases.insert({
    .kind = CaseKind::Value,
    .value = Ttx::Model::PackReference<Model::Pack>(value),
    .body = Reference<Block>(body),
    .payload = Reference<Model::Memory>(payload),
    .anchor = anchor,
    .constant = {},
  });
}

auto Language::Flow::Match::retain_constant_case(
    Model::Pack& value,
    Block& body,
    Anchor anchor) -> void {
  cases.insert({
    .kind = CaseKind::Constant,
    .value = Ttx::Model::PackReference<Model::Pack>(value),
    .body = Reference<Block>(body),
    .payload = {},
    .anchor = anchor,
    .constant = {},
  });
}

auto Language::Flow::Match::complete_default(Block& body) -> Bool {
  if (default_body) {
    return False;
  }
  default_body = Reference<Block>(body);
  return True;
}

auto Language::Flow::Match::complete_anchor(Anchor selected) -> void {
  anchor = selected;
}

auto Language::Flow::Match::link(
    Ttx::Lexical::Cursor& cursor,
    const Abstract& lexical_context,
    const Language::Model::Type& access_scope) -> Bool {
  if (linked) {
    return True;
  }

  Model::Pack& retained_input = input.get();
  BAIL_IF(!retained_input.link(cursor, lexical_context, access_scope));
  // Match consumes one value domain. Type selection can link for contextual
  // access but cannot lend a fabricated value merely to enter pattern flow.
  if (!retained_input.is_complete()) {
    cursor.create_expression_error(
        retained_input.get_anchor(),
        "Library match input did not produce value flow."_view,
        "Use a Type result only as an access receiver."_view);
    return False;
  }
  const Abstract& resolved_input_type = retained_input.get_type().resolve();
  auto input_type = resolved_input_type.select<Language::Model::Type>();
  if (!input_type || retained_input.get_layout().get_size() != 1) {
    cursor.create_expression_error(
        retained_input.get_anchor(),
        "Library match input did not produce one scalar value."_view,
        "Use one Expression with one exact completed Type."_view);
    return False;
  }

  Bool failed = False;
  auto option_type = input_type->select<Language::Types::Option>();
  if (option_type) {
    // Option elimination owns exactly one branch local payload and one absent
    // branch. The input Type never becomes an empty Layout in either case.
    if (cases.get_size() != 1 || !default_body) {
      cursor.create_expression_error(
          anchor, "Option match requires one value case and one `_` case."_view,
          "Bind the present value first and keep `_` as the final absent branch."_view);
      failed = True;
    }

    for (Count index = 0; index < cases.get_size(); index++) {
      Case& entry = cases[index];
      if (entry.kind != CaseKind::Value || !entry.payload) {
        cursor.create_expression_error(
            entry.anchor,
            "Option match value case requires one local name."_view,
            "Use `case value:` to bind the exact payload."_view);
        failed = True;
        continue;
      }

      auto binding = entry.payload->get().select<Payload>();
      const Abstract& shadowed =
          binding ? lexical_context.resolve_concept(binding->get_name())
                  : Unknown::get_unknown();
      if (!shadowed.is<Unknown>() && !shadowed.is<None>()) {
        auto report = cursor.create_report(entry.anchor);
        report
            << "Library match payload shadows a reachable lexical binding."_view;
        auto& note = report.get_hint();
        note << "Rename this payload so every enclosing name remains "
                "unambiguous."_view;
        auto original = cursor.get_associations().find(shadowed);
        if (original) {
          Token focus = original->get_token();
          if (!focus) {
            focus = original->get_span().get_start();
          }
          if (focus) {
            note << " Original declaration: "_view << cursor.get_source_path()
                 << ":"_view << focus.get_line() << ":"_view
                 << focus.get_column() << "."_view;
          }
        }
        failed = True;
      }
      if (!binding || !binding->bind(option_type->get_element_type())) {
        cursor.create_expression_error(
            entry.anchor,
            "Option match payload selected a different Type."_view,
            "Repeat linking with the same completed Option Type."_view);
        failed = True;
        continue;
      }

      cursor.get_associations().create(entry.anchor, *binding);
      failed |= !entry.body.get().link(cursor);
    }

    default_body.visit(
        []() {},
        [&](Reference<Block>& selected) {
          failed |= !selected.get().link(cursor);
        });
    BAIL_IF(failed);

    complete_coverage = True;
    linked = True;
    return True;
  }

  // Case Expressions settle before bodies so every
  // Tetrodotoxin::Library::Language::Constant and duplicate is known before
  // Match publishes any control coverage. The authored Expression remains the
  // fold owner while Match retains only the resulting exact value.
  for (Count index = 0; index < cases.get_size(); index++) {
    Case& entry = cases[index];
    entry.kind = CaseKind::Constant;

    Model::Pack& value = entry.value->get();
    Bool case_failed = !value.link(cursor, lexical_context, access_scope);
    // Cases obey the same value boundary as the input before constant folding
    // inspects any Layout or payload.
    if (!case_failed && !value.is_complete()) {
      cursor.create_expression_error(
          value.get_anchor(),
          "Library match case did not produce value flow."_view,
          "Use a runtime value for each constant case."_view);
      case_failed = True;
    }

    if (!case_failed) {
      const Abstract& case_type = value.get_type().resolve();
      if (!case_type.is<Language::Model::Type>() ||
          &case_type != &*input_type || value.get_layout().get_size() != 1) {
        cursor.create_expression_error(
            value.get_anchor(),
            "Library match case must have the input's exact scalar Type."_view,
            "Keep every case in the same completed value domain."_view);
        case_failed = True;
      }
    }

    Option<Tetrodotoxin::Library::Language::Constant&> selected;
    if (!case_failed) {
      Expression::fold(value).visit(
          [&](const Option<Model::Pack&>& folded) {
            if (folded) {
              selected = folded->select_identity<
                  Tetrodotoxin::Library::Language::Constant>();
            }
          },
          [&](const Expression::Error&) { case_failed = True; });
      if (!selected) {
        cursor.create_expression_error(
            value.get_anchor(),
            "Library match case did not fold to one Tetrodotoxin::Library::Language::Constant."_view,
            "Use a complete immutable case value."_view);
        case_failed = True;
      }
    }

    if (selected) {
      if (entry.constant && &entry.constant->get() != &*selected) {
        cursor.create_expression_error(
            value.get_anchor(),
            "Library match case selected a different Tetrodotoxin::Library::Language::Constant identity."_view,
            "Repeat linking with the same completed declaration graph."_view);
        case_failed = True;
      }

      for (Count previous = 0; previous < index; previous++) {
        const Case& retained = cases[previous];
        if (retained.constant && retained.constant->get() == *selected) {
          cursor.create_expression_error(
              value.get_anchor(),
              "Library match cannot retain one Tetrodotoxin::Library::Language::Constant case twice."_view,
              "Remove the later equal case."_view);
          case_failed = True;
          break;
        }
      }
      entry.constant =
          Reference<const Tetrodotoxin::Library::Language::Constant>(*selected);
    }

    case_failed |= !entry.body.get().link(cursor);
    failed |= case_failed;
  }

  default_body.visit(
      []() {},
      [&](Reference<Block>& selected) {
        failed |= !selected.get().link(cursor);
      });
  BAIL_IF(failed);

  auto has_complete_flag_coverage = [&]() -> Bool {
    auto flag_type =
        input_type->resolve()
            .select<Tetrodotoxin::Library::Language::Model::Types::Flag>();
    if (!flag_type) {
      return False;
    }

    Bool has_false = False;
    Bool has_true = False;
    for (const Case& entry : cases.get_view()) {
      BAIL_IF(!entry.constant);
      auto validity = flag_type->get_validity(entry.constant->get());
      BAIL_IF(!validity);
      if (*validity) {
        has_true = True;
      } else {
        has_false = True;
      }
    }

    return has_false && has_true;
  };
  complete_coverage = Bool(default_body) || has_complete_flag_coverage();
  linked = True;
  return True;
}

auto Language::Flow::Match::finalize(Cursor& cursor) -> void {
  input.get().finalize(cursor);
  for (Count index = 0; index < cases.get_size(); index++) {
    Case& entry = cases[index];
    entry.value.visit(
        []() {},
        [&](Ttx::Model::PackReference<Model::Pack>& selected) {
          selected.get().finalize(cursor);
        });
    entry.body.get().finalize(cursor);
  }
  default_body.visit(
      []() {},
      [&](Reference<Block>& selected) { selected.get().finalize(cursor); });
}

auto Language::Flow::Match::reaches_next_statement() const -> Bool {
  if (!complete_coverage) {
    return True;
  }

  for (const Case& entry : cases.get_view()) {
    if (entry.body.get().reaches_next_statement()) {
      return True;
    }
  }

  return default_body.visit(
      []() { return False; },
      [](const Reference<Block>& selected) {
        return selected.get().reaches_next_statement();
      });
}

auto Language::Flow::Match::get_case_constant(Count index) const
    -> Option<const Tetrodotoxin::Library::Language::Constant&> {
  if (index >= cases.get_size()) {
    return {};
  }

  return cases.get_view().get_data()[index].constant.visit(
      []() -> Option<const Tetrodotoxin::Library::Language::Constant&> {
        return {};
      },
      [](const Reference<const Tetrodotoxin::Library::Language::Constant>&
             selected)
          -> Option<const Tetrodotoxin::Library::Language::Constant&> {
        return selected.get();
      });
}

auto Language::Flow::Match::get_case_kind(Count index) const
    -> Option<CaseKind> {
  if (index >= cases.get_size()) {
    return {};
  }

  return cases.get_view().get_data()[index].kind;
}

auto Language::Flow::Match::get_case_payload(Count index) const
    -> Option<const Language::Model::Memory&> {
  if (index >= cases.get_size()) {
    return {};
  }

  return cases.get_view().get_data()[index].payload.visit(
      []() -> Option<const Language::Model::Memory&> { return {}; },
      [](const Reference<Language::Model::Memory>& selected)
          -> Option<const Language::Model::Memory&> {
        return selected.get();
      });
}

auto Language::Flow::Match::get_case_body(Count index) const
    -> Option<const Block&> {
  if (index >= cases.get_size()) {
    return {};
  }

  return cases.get_view().get_data()[index].body.get();
}

auto Language::Flow::Match::get_case_anchor(Count index) const
    -> Option<Ttx::Lexical::Anchor> {
  if (index >= cases.get_size()) {
    return {};
  }

  return cases.get_view().get_data()[index].anchor;
}
