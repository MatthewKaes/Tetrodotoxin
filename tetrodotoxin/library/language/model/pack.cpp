// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/language/model/pack.hpp"

#include "perimortem/core/static/vector.hpp"

#include "perimortem/memory/managed/vector.hpp"

#include "tetrodotoxin/library/language/constant.hpp"
#include "tetrodotoxin/library/language/constants/aggregate.hpp"
#include "tetrodotoxin/library/language/constants/bytes.hpp"
#include "tetrodotoxin/library/language/constants/enumeration.hpp"
#include "tetrodotoxin/library/language/constants/false.hpp"
#include "tetrodotoxin/library/language/constants/object.hpp"
#include "tetrodotoxin/library/language/constants/option.hpp"
#include "tetrodotoxin/library/language/constants/range.hpp"
#include "tetrodotoxin/library/language/constants/real.hpp"
#include "tetrodotoxin/library/language/constants/result.hpp"
#include "tetrodotoxin/library/language/constants/signed.hpp"
#include "tetrodotoxin/library/language/constants/true.hpp"
#include "tetrodotoxin/library/language/constants/unsigned.hpp"
#include "tetrodotoxin/library/language/expression.hpp"
#include "tetrodotoxin/library/language/generic.hpp"
#include "tetrodotoxin/library/language/model/type.hpp"
#include "tetrodotoxin/library/language/model/types/flag.hpp"
#include "tetrodotoxin/library/language/model/types/real.hpp"
#include "tetrodotoxin/library/language/model/types/signed.hpp"
#include "tetrodotoxin/library/language/model/types/unsigned.hpp"
#include "tetrodotoxin/library/language/types/object_storage.hpp"
#include "tetrodotoxin/library/language/types/option.hpp"
#include "tetrodotoxin/library/language/types/range.hpp"
#include "tetrodotoxin/library/language/types/result.hpp"
#include "ttx/concept/documentation.hpp"
#include "ttx/concept/reference.hpp"

using namespace Perimortem;
using namespace Ttx::Concept;
using namespace Ttx::Model;
using namespace Tetrodotoxin::Library;

auto Language::Model::Pack::from(Abstract& identity) -> Core::Option<Pack&> {
  auto expression = identity.select<Language::Expression>();
  if (expression) {
    return *expression;
  }
  auto constant = identity.select<Language::Constant>();
  if (constant) {
    return *constant;
  }
  auto aggregate = identity.select<Language::Constants::Aggregate>();
  return aggregate ? Core::Option<Pack&>(*aggregate) : Core::Option<Pack&>();
}

auto Language::Model::Pack::from(const Abstract& identity)
    -> Core::Option<const Pack&> {
  auto expression = identity.select<Language::Expression>();
  if (expression) {
    return *expression;
  }
  auto constant = identity.select<Language::Constant>();
  if (constant) {
    return *constant;
  }
  auto aggregate = identity.select<Language::Constants::Aggregate>();
  return aggregate ? Core::Option<const Pack&>(*aggregate)
                   : Core::Option<const Pack&>();
}

auto Language::Model::Pack::link_restored(
    const Abstract&,
    Core::Option<const Abstract&>) -> Bool {
  return False;
}

class Group final : public Language::Model::Pack {
 public:
  class Layout final : public Ttx::Concept::Layout {
   public:
    struct Selection {
      Count entry;
      Count value;
    };

    constexpr Layout(const Group& group) : group(group) {}

    auto get_size() const -> Count override;
    auto get_abstract(Count index) const
        -> Core::Option<const Abstract&> override;
    auto get_name(Count index) const
        -> Core::Option<Core::View::Bytes> override;
    auto fits_entry(
        const Ttx::Concept::Layout& target,
        Count source,
        Count target_index) const -> Bool override;
    auto fits_at(const Ttx::Concept::Layout& target, Count target_offset) const
        -> Bool override;
    auto get_fitted_at(
        const Ttx::Concept::Layout& target,
        Count target_offset,
        Count target_index) const
        -> Utility::Result<const Abstract&, Errors> override;

    auto select(Count index) const -> Core::Option<Selection>;

   private:
    const Group& group;
  };

  Group(
      Memory::Allocator::Arena& domain,
      Core::View::Vector<Ttx::Model::PackReference<Language::Model::Pack>>
          source_entries,
      Core::View::Vector<Core::View::Bytes> source_names,
      Core::Option<Ttx::Lexical::Anchor> anchor,
      Bool linked = False)
      : entries(domain),
        names(domain),
        anchor(anchor),
        layout(*this),
        linked(linked) {
    entries.reset(source_entries.get_size());
    for (const Ttx::Model::PackReference<Language::Model::Pack>& entry :
         source_entries) {
      entries.insert(entry);
    }

    names.reset(source_names.get_size());
    for (Core::View::Bytes name : source_names) {
      names.insert(name);
    }
  }

  auto link(
      Ttx::Lexical::Cursor& cursor,
      const Abstract& lexical_context,
      Core::Option<const Abstract&> access_scope) -> Bool override {
    Bool failed = False;
    for (Ttx::Model::PackReference<Language::Model::Pack> entry :
         entries.get_view()) {
      failed |= !entry.get().link(cursor, lexical_context, access_scope);
    }
    BAIL_IF(failed);

    // Type selection links here so a following access can query that identity.
    // A group is a value consumer, so it rejects the same result before Layout
    // observation turns the missing value output into a process failure.
    for (Ttx::Model::PackReference<Language::Model::Pack> entry :
         entries.get_view()) {
      if (!entry.get().is_complete()) {
        cursor.create_expression_error(
            anchor, "Library Pack entry did not produce value flow."_view,
            "Use a Type result only as an access receiver."_view);
        return False;
      }
    }

    if (!names.is_empty()) {
      for (Ttx::Model::PackReference<Language::Model::Pack> entry :
           entries.get_view()) {
        if (entry.get().get_layout().get_size() != 1) {
          cursor.create_expression_error(
              anchor,
              "A named Library Pack entry must produce exactly one value."_view,
              "Name each scalar value separately or use positional flow."_view);
          return False;
        }
      }
    }

    linked = True;
    return True;
  }

  auto link_restored(
      const Abstract& lexical_context,
      Core::Option<const Abstract&> access_scope) -> Bool override {
    if (linked) {
      return True;
    }

    for (Ttx::Model::PackReference<Language::Model::Pack> entry :
         entries.get_view()) {
      BAIL_IF(!entry.get().link_restored(lexical_context, access_scope));
      BAIL_IF(!entry.get().is_complete());
    }
    linked = True;
    return True;
  }

  auto get_layout() const -> const Ttx::Concept::Layout& override {
    return layout;
  }

  auto get_value_type(Count index) const -> const Abstract& override {
    auto selected = layout.select(index);
    if (!selected) {
      return Unknown::get_unknown();
    }
    return entries.at(selected->entry).get().get_value_type(selected->value);
  }

  auto is_complete() const -> Bool override { return linked; }

  auto get_anchor() const -> Core::Option<Ttx::Lexical::Anchor> override {
    return anchor;
  }

  auto get_identity() const -> Core::Option<const Abstract&> override {
    return {};
  }

  auto finalize(Ttx::Lexical::Cursor& cursor) -> void override {
    for (Ttx::Model::PackReference<Language::Model::Pack> entry :
         entries.get_view()) {
      entry.get().finalize(cursor);
    }
  }

  constexpr auto get_entries() const -> Core::View::Vector<
      Ttx::Model::PackReference<Language::Model::Pack>> override {
    return entries;
  }

  Memory::Managed::Vector<Ttx::Model::PackReference<Language::Model::Pack>>
      entries;
  Memory::Managed::Vector<Core::View::Bytes> names;
  Core::Option<Ttx::Lexical::Anchor> anchor;
  Layout layout;
  Bool linked = False;
};

auto Group::Layout::get_size() const -> Count {
  if (!group.names.is_empty()) {
    return group.entries.get_size();
  }

  Count size = 0;
  for (Ttx::Model::PackReference<Language::Model::Pack> entry :
       group.entries.get_view()) {
    size += entry.get().get_layout().get_size();
  }
  return size;
}

auto Group::Layout::select(Count index) const -> Core::Option<Selection> {
  if (!group.names.is_empty()) {
    BAIL_IF(index >= group.entries.get_size());
    return Selection{index, 0};
  }

  Count offset = 0;
  for (Count entry = 0; entry < group.entries.get_size(); entry++) {
    Count size = group.entries.at(entry).get().get_layout().get_size();
    if (index < offset + size) {
      return Selection{entry, index - offset};
    }
    offset += size;
  }
  return {};
}

auto Group::Layout::get_abstract(Count index) const
    -> Core::Option<const Abstract&> {
  auto selected = select(index);
  BAIL_IF(!selected);
  return group.entries.at(selected->entry)
      .get()
      .get_layout()
      .get_abstract(selected->value);
}

auto Group::Layout::get_name(Count index) const
    -> Core::Option<Core::View::Bytes> {
  BAIL_IF(group.names.is_empty() || index >= group.names.get_size());
  return group.names.at(index);
}

static auto get_target_name(const Ttx::Concept::Layout& target, Count index)
    -> Core::Option<Core::View::Bytes> {
  auto name = target.get_name(index);
  if (name) {
    return name;
  }
  return target.get_abstract(index).visit(
      []() -> Core::Option<Core::View::Bytes> { return {}; },
      [](const Abstract& entry) -> Core::Option<Core::View::Bytes> {
        Core::View::Bytes name = entry.get_name();
        return name.is_empty() ? Core::Option<Core::View::Bytes>() : name;
      });
}

auto Group::Layout::fits_entry(
    const Ttx::Concept::Layout& target,
    Count source,
    Count target_index) const -> Bool {
  BAIL_IF(source >= get_size() || target_index >= target.get_size());
  auto selected = select(source);
  BAIL_IF(!selected);

  if (!group.names.is_empty()) {
    auto source_name = get_name(source);
    auto target_name = get_target_name(target, target_index);
    BAIL_IF(!source_name || !target_name || *source_name != *target_name);
  }

  return group.entries.at(selected->entry)
      .get()
      .fits_entry(target, selected->value, target_index);
}

auto Group::Layout::fits_at(
    const Ttx::Concept::Layout& target,
    Count target_offset) const -> Bool {
  BAIL_IF(!has_target_segment(target, target_offset));

  if (group.names.is_empty()) {
    for (Count source = 0; source < get_size(); source++) {
      BAIL_IF(!fits_entry(target, source, target_offset + source));
    }
    return True;
  }

  for (Count source = 0; source < get_size(); source++) {
    auto source_name = get_name(source);
    BAIL_IF(!source_name);

    Count selected = 0;
    Count matches = 0;
    for (Count candidate = 0; candidate < get_size(); candidate++) {
      auto target_name = get_target_name(target, target_offset + candidate);
      if (target_name && *target_name == *source_name) {
        selected = candidate;
        matches++;
      }
    }
    BAIL_IF(
        matches != 1 || !fits_entry(target, source, target_offset + selected));
  }
  return True;
}

auto Group::Layout::get_fitted_at(
    const Ttx::Concept::Layout& target,
    Count target_offset,
    Count target_index) const -> Utility::Result<const Abstract&, Errors> {
  if (target_index >= get_size()) {
    return Errors::IndexOutOfBounds;
  }
  if (!has_target_segment(target, target_offset)) {
    return Errors::SizeMismatch;
  }
  if (!fits_at(target, target_offset)) {
    return Errors::IncompatibleFit;
  }

  Count source = target_index;
  if (!group.names.is_empty()) {
    auto target_name = get_target_name(target, target_offset + target_index);
    if (!target_name) {
      return Errors::IncompatibleFit;
    }
    for (Count candidate = 0; candidate < get_size(); candidate++) {
      auto source_name = get_name(candidate);
      if (source_name && *source_name == *target_name) {
        source = candidate;
        break;
      }
    }
  }

  auto selected = select(source);
  if (!selected) {
    return Errors::IncompatibleFit;
  }
  return group.entries.at(selected->entry)
      .get()
      .get_layout()
      .get_abstract(selected->value)
      .visit(
          []() -> Utility::Result<const Abstract&, Errors> {
            return Errors::IncompatibleFit;
          },
          [](const Abstract& entry)
              -> Utility::Result<const Abstract&, Errors> { return entry; });
}

auto Language::Model::Pack::get_type() const -> const Abstract& {
  const Ttx::Concept::Layout& layout = get_layout();
  if (layout.get_size() != 1) {
    return Unknown::get_unknown();
  }

  return get_value_type(0);
}

auto Language::Model::Pack::get_result() const -> const Abstract& {
  return get_identity().visit(
      []() -> const Abstract& { return Unknown::get_unknown(); },
      [](const Abstract& identity) -> const Abstract& { return identity; });
}

auto Language::Model::Pack::get_identity() const
    -> Core::Option<const Abstract&> {
  const Ttx::Concept::Layout& layout = get_layout();
  BAIL_IF(layout.get_size() != 1);
  return layout.get_abstract(0);
}

static auto select_target_type(const Abstract& target)
    -> Core::Option<const Language::Model::Type&> {
  auto direct = target.select<Language::Model::Type>();
  if (direct) {
    return *direct;
  }

  const Abstract& resolved = target.resolve();
  auto addressable = resolved.select<Ttx::Model::Addressable>();
  const Abstract& selected = addressable ? addressable->get_type() : resolved;
  direct = selected.select<Language::Model::Type>();
  return direct ? direct : selected.resolve().select<Language::Model::Type>();
}

auto Language::Model::Pack::fits_entry(
    const Ttx::Concept::Layout& target,
    Count source_index,
    Count target_index) const -> Bool {
  const Ttx::Concept::Layout& source = get_layout();
  BAIL_IF(
      source_index >= source.get_size() || target_index >= target.get_size());

  // The concrete Layout gets first refusal because it owns named selection,
  // ranged provenance, and any composed source mapping. A scalar producer is
  // the only fallback: its Library Pack contract can admit a contextual value
  // fit that raw Abstract identity cannot express.
  if (source.fits_entry(target, source_index, target_index)) {
    return True;
  }
  auto source_name = source.get_name(source_index);
  auto target_name = get_target_name(target, target_index);
  BAIL_IF(source_name && (!target_name || *source_name != *target_name));
  auto target_entry = target.get_abstract(target_index);
  BAIL_IF(!target_entry);
  auto target_type = select_target_type(*target_entry);
  BAIL_IF(!target_type);

  auto producer = source.get_abstract(source_index);
  auto supplied =
      producer ? Pack::from(*producer) : Core::Option<const Pack&>();
  return supplied ? supplied->fits_into(*target_type) : False;
}

auto Language::Model::Pack::fits_at(
    const Ttx::Concept::Layout& target,
    Count target_offset) const -> Bool {
  const Ttx::Concept::Layout& source = get_layout();
  BAIL_IF(
      target_offset > target.get_size() ||
      source.get_size() > target.get_size() - target_offset);

  // Named flow owns target reordering, while this Pack keeps contextual fitting
  // on each real producer. Delegating the complete operation to Named would
  // compare only raw identities and lose cross source scalar admission.
  for (Count index = 0; index < source.get_size(); index++) {
    if (source.get_name(index)) {
      for (Count source_index = 0; source_index < source.get_size();
           source_index++) {
        auto source_name = source.get_name(source_index);
        BAIL_IF(!source_name);
        Count selected = 0;
        Count matches = 0;
        for (Count target_index = 0; target_index < source.get_size();
             target_index++) {
          auto target_name =
              get_target_name(target, target_offset + target_index);
          if (target_name && *target_name == *source_name) {
            selected = target_index;
            matches++;
          }
        }
        BAIL_IF(
            matches != 1 ||
            !fits_entry(target, source_index, target_offset + selected));
      }
      return True;
    }
  }

  for (Count index = 0; index < source.get_size(); index++) {
    BAIL_IF(!fits_entry(target, index, target_offset + index));
  }
  return True;
}

auto Language::Model::Pack::fits(const Ttx::Concept::Layout& target) const
    -> Bool {
  BAIL_IF(!is_complete());
  if (get_layout().get_size() == target.get_size() && fits_at(target, 0)) {
    return True;
  }

  BAIL_IF(target.get_size() != 1);
  BAIL_IF(get_layout().get_size() == 1 && get_layout().get_name(0));
  auto target_entry = target.get_abstract(0);
  BAIL_IF(!target_entry);
  auto target_type = select_target_type(*target_entry);
  return target_type && target_type->accepts(*this);
}

auto Language::Model::Pack::get_fitted_at(
    const Ttx::Concept::Layout& target,
    Count target_offset,
    Count target_index) const
    -> Utility::Result<const Abstract&, Ttx::Concept::Layout::Errors> {
  const Ttx::Concept::Layout& source = get_layout();
  if (target_index >= source.get_size()) {
    return Ttx::Concept::Layout::Errors::IndexOutOfBounds;
  }
  if (target_offset > target.get_size() ||
      source.get_size() > target.get_size() - target_offset) {
    return Ttx::Concept::Layout::Errors::SizeMismatch;
  }
  if (!fits_at(target, target_offset)) {
    return Ttx::Concept::Layout::Errors::IncompatibleFit;
  }

  for (Count index = 0; index < source.get_size(); index++) {
    if (source.get_name(index)) {
      return source.get_fitted_at(target, target_offset, target_index);
    }
  }
  return source.get_abstract(target_index)
      .visit(
          []() -> Utility::Result<
                   const Abstract&, Ttx::Concept::Layout::Errors> {
            return Ttx::Concept::Layout::Errors::IncompatibleFit;
          },
          [](const Abstract& entry)
              -> Utility::Result<
                  const Abstract&, Ttx::Concept::Layout::Errors> {
            return entry;
          });
}

auto Language::Model::Pack::fits(const Ttx::Model::Type& target) const -> Bool {
  BAIL_IF(!is_complete());
  const Ttx::Concept::Layout& target_layout = target.get_layout();
  return get_layout().get_size() == target_layout.get_size() &&
         fits_at(target_layout, 0);
}

auto Language::Model::Pack::fits_into(const Ttx::Model::Type& target) const
    -> Bool {
  if (fits(target)) {
    return True;
  }

  auto library_target = target.select<Language::Model::Type>();
  return library_target && library_target->accepts(*this);
}

auto Language::Model::Pack::create_empty(
    Perimortem::Memory::Allocator::Arena& domain,
    Core::Option<Ttx::Lexical::Anchor> anchor) -> Pack& {
  return domain.construct<Group>(
      domain, Core::View::Vector<Ttx::Model::PackReference<Pack>>(),
      Core::View::Vector<Core::View::Bytes>(), anchor);
}

auto Language::Model::Pack::create_group(
    Perimortem::Memory::Allocator::Arena& domain,
    Core::View::Vector<Ttx::Model::PackReference<Pack>> entries,
    Core::View::Vector<Core::View::Bytes> names,
    Core::Option<Ttx::Lexical::Anchor> anchor) -> Pack& {
  return domain.construct<Group>(domain, entries, names, anchor);
}

auto Language::Model::Pack::create_folded(
    Perimortem::Memory::Allocator::Arena& domain,
    Core::View::Vector<Ttx::Model::PackReference<Pack>> entries) -> Pack& {
  auto aggregate = Language::Constants::Aggregate::create(domain, entries);
  return aggregate ? static_cast<Pack&>(*aggregate)
                   : create_completed(domain, entries);
}

auto Language::Model::Pack::create_completed(
    Perimortem::Memory::Allocator::Arena& domain,
    Core::View::Vector<Ttx::Model::PackReference<Pack>> entries,
    Core::View::Vector<Core::View::Bytes> names) -> Pack& {
  return domain.construct<Group>(
      domain, entries, names, Core::Option<Ttx::Lexical::Anchor>(), True);
}
