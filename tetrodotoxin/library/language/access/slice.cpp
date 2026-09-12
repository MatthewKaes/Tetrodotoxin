// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/language/access/slice.hpp"

#include "perimortem/memory/managed/vector.hpp"

#include "tetrodotoxin/library/language/constants/bytes.hpp"
#include "tetrodotoxin/library/language/constants/signed.hpp"
#include "tetrodotoxin/library/language/constants/unsigned.hpp"
#include "tetrodotoxin/library/language/model/memory.hpp"
#include "tetrodotoxin/library/language/model/types/signed.hpp"
#include "tetrodotoxin/library/language/model/types/unsigned.hpp"
#include "tetrodotoxin/library/language/types/contiguous.hpp"
#include "ttx/concept/unknown.hpp"

using namespace Perimortem;
using namespace Tetrodotoxin::Library;
using namespace Ttx::Concept;
using namespace Ttx::Lexical;
using namespace Ttx::Model;

auto Language::Access::Slice::create_authored(
    Memory::Allocator::Arena& domain,
    Model::Pack& receiver,
    Model::Pack& index,
    Anchor anchor) -> Slice& {
  return Expression::create_authored<Slice>(
      domain, anchor, [&](auto source) -> Slice {
        return Slice(domain, receiver, index, source);
      });
}

auto Language::Access::Slice::create_authored(
    Memory::Allocator::Arena& domain,
    Model::Pack& receiver,
    Model::Pack& start,
    Model::Pack& count,
    Anchor anchor) -> Slice& {
  return Expression::create_authored<Slice>(
      domain, anchor, [&](auto source) -> Slice {
        return Slice(domain, receiver, start, count, source);
      });
}

Language::Access::Slice::Slice(
    Memory::Allocator::Arena& domain,
    Model::Pack& receiver,
    Model::Pack& index,
    Core::Option<Anchor> anchor)
    : Expression(anchor), domain(domain), receiver(receiver), first(index) {}

Language::Access::Slice::Slice(
    Memory::Allocator::Arena& domain,
    Model::Pack& receiver,
    Model::Pack& start,
    Model::Pack& count,
    Core::Option<Anchor> anchor)
    : Expression(anchor),
      domain(domain),
      receiver(receiver),
      first(start),
      count(Ttx::Model::PackReference<Model::Pack>(count)) {}

// Slice links one homogeneous receiver and folds retained values without
// manufacturing an aggregate Type. Compact Bytes and general folded Packs use
// the same safe selection and one default for each slot.
static auto get_byte_type(const Language::Model::Type& type) -> Core::Option<
    const Tetrodotoxin::Library::Language::Model::Types::Unsigned&> {
  return type.visit<Tetrodotoxin::Library::Language::Model::Types::Unsigned>(
      [](const Tetrodotoxin::Library::Language::Model::Types::Unsigned&
             selected)
          -> Core::Option<
              const Tetrodotoxin::Library::Language::Model::Types::Unsigned&> {
        if (selected.get_width() != 8 || selected.get_size() != 1) {
          return {};
        }

        return selected;
      },
      [](const Abstract&)
          -> Core::Option<
              const Tetrodotoxin::Library::Language::Model::Types::Unsigned&> {
        return {};
      });
}

static auto is_integer(const Language::Model::Pack& value) -> Bool {
  const Abstract& type = value.get_type().resolve();
  return type.is<Tetrodotoxin::Library::Language::Model::Types::Signed>() ||
         type.is<Tetrodotoxin::Library::Language::Model::Types::Unsigned>();
}

// Option is the safe miss produced by an integer outside Count. Error remains
// reserved for a Constant that does not expose its promised integer domain.
static auto get_count(
    const Language::Constant& constant,
    const Language::Model::Pack& authored)
    -> Utility::Result<Core::Option<Count>, Language::Expression::Error> {
  return constant.visit<Language::Constants::Signed>(
      [&](const Language::Constants::Signed& value)
          -> Utility::Result<Core::Option<Count>, Language::Expression::Error> {
        if (value.get_value() < 0) {
          return Core::Option<Count>{};
        }

        U64 selected = U64(value.get_value());
        if (selected > U64(Count(-1))) {
          return Core::Option<Count>{};
        }

        return Core::Option<Count>(Count(selected));
      },
      [&](const Abstract& selected)
          -> Utility::Result<Core::Option<Count>, Language::Expression::Error> {
        return selected.visit<Language::Constants::Unsigned>(
            [&](const Language::Constants::Unsigned& value)
                -> Utility::Result<
                    Core::Option<Count>, Language::Expression::Error> {
              if (value.get_value() > U64(Count(-1))) {
                return Core::Option<Count>{};
              }

              return Core::Option<Count>(Count(value.get_value()));
            },
            [&](const Abstract&)
                -> Utility::Result<
                    Core::Option<Count>, Language::Expression::Error> {
              return Language::Expression::Error::from_pack(
                  Language::Expression::Error::Type::InvalidConstant, authored);
            });
      });
}

static auto select_scalar(Language::Model::Pack& pack)
    -> Core::Option<Language::Constant&> {
  return pack.select_identity<Language::Constant>();
}

static auto fold_pack(Language::Model::Pack& pack) -> Utility::
    Result<Core::Option<Language::Model::Pack&>, Language::Expression::Error> {
  if (pack.select_identity<Language::Constant>()) {
    return Core::Option<Language::Model::Pack&>(pack);
  }
  auto expression = pack.select_identity<Language::Expression>();
  return expression ? expression->fold()
                    : Utility::Result<
                          Core::Option<Language::Model::Pack&>,
                          Language::Expression::Error>(
                          Core::Option<Language::Model::Pack&>());
}

static auto pack_anchor(const Language::Model::Pack& pack)
    -> Core::Option<Ttx::Lexical::Anchor> {
  auto expression = pack.select_identity<Language::Expression>();
  if (expression) {
    return expression->get_anchor();
  }
  auto constant = pack.select_identity<Language::Constant>();
  return constant ? constant->get_anchor() : Core::Option<Anchor>();
}

static auto select_folded_entry(Language::Model::Pack& pack, Count index)
    -> Core::Option<Language::Model::Pack&> {
  auto producer = pack.get_layout().get_abstract(index);
  BAIL_IF(!producer);
  auto constant = producer->select<Language::Constant>();
  BAIL_IF(!constant || constant->get_layout().get_size() != 1);
  return const_cast<Language::Constant&>(*constant);
}

static auto select_required_type(const Abstract& candidate)
    -> Core::Option<const Language::Model::Type&> {
  auto direct = candidate.select<Language::Model::Type>();
  if (direct) {
    return *direct;
  }

  auto pack = Language::Model::Pack::from(candidate);
  if (pack) {
    direct = pack->get_type().select<Language::Model::Type>();
    if (direct) {
      return *direct;
    }
  }

  const Abstract& resolved = candidate.resolve();
  auto addressable = candidate.select<Language::Model::Memory>();
  if (!addressable) {
    addressable = resolved.select<Language::Model::Memory>();
  }
  const Abstract& selected = addressable ? addressable->get_type() : resolved;
  direct = selected.select<Language::Model::Type>();
  return direct ? direct : selected.resolve().select<Language::Model::Type>();
}

// A slice is homogeneous value flow, but the repeated source is still the one
// Slice expression that performs selection. Keeping this Layout subordinate to
// Slice avoids teaching host neutral Ranged how a Library Expression fits a
// required element Type, and avoids fabricating one proxy identity per slot.
static auto create_layout(
    Memory::Allocator::Arena& domain,
    const Language::Access::Slice& source,
    const Language::Model::Type& element,
    Count size) -> const Ttx::Concept::Layout& {
  class Layout final : public Ttx::Concept::Layout {
   public:
    constexpr Layout(
        const Language::Access::Slice& source,
        const Language::Model::Type& element,
        Count size)
        : source(source), element(element), size(size) {}

    constexpr auto get_size() const -> Count override { return size; }

    constexpr auto get_abstract(Count index) const
        -> Core::Option<const Abstract&> override {
      BAIL_IF(index >= size);
      return source;
    }

    auto fits_entry(
        const Ttx::Concept::Layout& target,
        Count source_index,
        Count target_index) const -> Bool override {
      BAIL_IF(source_index >= size || target_index >= target.get_size());
      return target.get_abstract(target_index)
          .visit(
              []() { return False; },
              [&](const Abstract& required) {
                return select_required_type(required).visit(
                    []() { return False; },
                    [&](const Language::Model::Type& type) {
                      return element.get_layout().fits(type.get_layout());
                    });
              });
    }

    auto fits_at(const Ttx::Concept::Layout& target, Count target_offset) const
        -> Bool override {
      BAIL_IF(!has_target_segment(target, target_offset));
      for (Count index = 0; index < size; index++) {
        BAIL_IF(!fits_entry(target, index, target_offset + index));
      }
      return True;
    }

    auto get_fitted_at(
        const Ttx::Concept::Layout& target,
        Count target_offset,
        Count target_index) const
        -> Utility::Result<const Abstract&, Errors> override {
      if (target_index >= size) {
        return Errors::IndexOutOfBounds;
      }

      if (!has_target_segment(target, target_offset)) {
        return Errors::SizeMismatch;
      }

      if (!fits_at(target, target_offset)) {
        return Errors::IncompatibleFit;
      }

      // Layout index retains which repeated value is consumed. Lowering can use
      // that index without replacing the one semantic producer with shadow
      // nodes.
      return source;
    }

   private:
    const Language::Access::Slice& source;
    const Language::Model::Type& element;
    Count size;
  };

  return domain.construct<Layout>(source, element, size);
}

auto Language::Access::Slice::link(
    Ttx::Lexical::Cursor& cursor,
    const Abstract& lexical_context,
    Core::Option<const Abstract&> access_scope) -> Bool {
  Bool failed = !receiver.link(cursor, lexical_context, access_scope);
  failed |= !first.link(cursor, lexical_context, access_scope);
  if (count) {
    failed |= !count->get().link(cursor, lexical_context, access_scope);
  }
  BAIL_IF(failed);

  auto contiguous =
      receiver.get_type().resolve().select<Language::Types::Contiguous>();
  if (!contiguous || !is_integer(first) ||
      (count && !is_integer(count->get()))) {
    cursor.create_expression_error(
        get_anchor(), "Slice rejects the linked operand Types."_view,
        "Use an indexable receiver and integer index, start, and count Expressions."_view);
    return False;
  }

  const Language::Model::Type& element = contiguous->get_element_type();
  if (element_type && &element_type->get() != &element) {
    cursor.create_expression_error(
        get_anchor(), "Slice cannot change its linked element Type."_view,
        "Keep one exact element Type on this authored access."_view);
    return False;
  }
  element_type = Reference<const Language::Model::Type>(element);

  if (!count) {
    auto selected_fallback = element.create_default(cursor.get_arena());
    BAIL_IF(!selected_fallback);
    fallback = Ttx::Model::PackReference<Model::Pack>(*selected_fallback);
    return Expression::link(cursor, lexical_context, access_scope);
  }

  // Range count determines the complete Pack shape and is therefore a link
  // fact, not a lowering payload detail. Start remains ordinary dynamic
  // input because it changes which values flow, never how many slots exist.
  Model::Pack& count_expression = count->get();

  Core::Option<Model::Pack&> folded_count;
  Core::Option<Expression::Error> fold_error;
  fold_pack(count_expression)
      .visit(
          [&](const Core::Option<Model::Pack&>& folded) {
            folded_count = folded;
          },
          [&](const Expression::Error& error) { fold_error = error; });
  if (fold_error || !folded_count) {
    cursor.create_expression_error(
        pack_anchor(count_expression),
        "Slice range count did not constant-fold during linking."_view,
        "Supply one nonnegative integer Constant for the range count."_view);
    return False;
  }

  auto folded_count_expression = select_scalar(*folded_count);
  if (!folded_count_expression) {
    cursor.create_expression_error(
        pack_anchor(count_expression),
        "Slice range count did not fold to one scalar Constant."_view,
        "Supply one nonnegative integer Constant for the range count."_view);
    return False;
  }

  Core::Option<Count> selected_count;
  Core::Option<Expression::Error> count_error;
  ::get_count(*folded_count_expression, count_expression)
      .visit(
          [&](const Core::Option<Count>& selected) {
            selected_count = selected;
          },
          [&](const Expression::Error& error) { count_error = error; });
  if (count_error || !selected_count) {
    cursor.create_expression_error(
        pack_anchor(count_expression),
        "Slice range count is outside the supported nonnegative range."_view,
        "Use a nonnegative integer Constant representable as Count."_view);
    return False;
  }

  if (range_layout) {
    Bool changed = !range_count || *range_count != *selected_count;
    if (changed) {
      cursor.create_expression_error(
          get_anchor(),
          "Slice range cannot change its linked output Layout."_view,
          "Keep one exact element Type and constant count for this access."_view);
      return False;
    }
    return True;
  }

  range_count = *selected_count;
  const auto& layout = create_layout(domain, *this, element, *selected_count);
  range_layout = layout;
  return True;
}

auto Language::Access::Slice::get_type() const -> const Abstract& {
  if (!element_type ||
      (count && (!range_layout || !range_count || *range_count != 1))) {
    return Unknown::get_unknown();
  }

  return element_type->get();
}

auto Language::Access::Slice::get_value_type(Count index) const
    -> const Abstract& {
  if (!element_type || index >= get_layout().get_size()) {
    return Unknown::get_unknown();
  }

  return element_type->get();
}

auto Language::Access::Slice::get_layout() const
    -> const Ttx::Concept::Layout& {
  if (!count) {
    return Expression::get_layout();
  }

  return *range_layout;
}

auto Language::Access::Slice::resolve() const -> const Abstract& {
  if (!count) {
    return Expression::resolve();
  }

  if (!range_layout) {
    return Unknown::get_unknown();
  }

  return *this;
}

auto Language::Access::Slice::fits(const Ttx::Model::Type& target) const
    -> Bool {
  if (!count) {
    return Expression::fits(target);
  }

  // A range Slice is not one element with a special carrier Type. Its complete
  // Pack must negotiate with the receiving descriptor so `Fixed[T, count]`,
  // structural Layouts, and the empty Layout all observe the same flow.
  return Model::Pack::fits(target);
}

auto Language::Access::Slice::finalize(Cursor& cursor) -> void {
  receiver.finalize(cursor);
  first.finalize(cursor);
  if (count) {
    count->get().finalize(cursor);
  }
  Expression::finalize(cursor);
}

auto Language::Access::Slice::evaluate()
    -> Utility::Result<Core::Option<Model::Pack&>, Expression::Error> {
  Core::Option<Model::Pack&> folded_receiver_pack;
  auto receiver_fold = fold_pack(receiver);
  auto receiver_error = Core::Option<Expression::Error>{};
  receiver_fold.visit(
      [&](const Core::Option<Model::Pack&>& selected) {
        folded_receiver_pack = selected;
      },
      [&](const Expression::Error& error) { receiver_error = error; });
  if (receiver_error) {
    return *receiver_error;
  }

  Core::Option<Model::Pack&> folded_first_pack;
  auto first_fold = fold_pack(first);
  auto first_error = Core::Option<Expression::Error>{};
  first_fold.visit(
      [&](const Core::Option<Model::Pack&>& selected) {
        folded_first_pack = selected;
      },
      [&](const Expression::Error& error) { first_error = error; });
  if (first_error) {
    return *first_error;
  }

  if (!folded_receiver_pack || !folded_first_pack) {
    return Core::Option<Model::Pack&>{};
  }

  auto folded_receiver = select_scalar(*folded_receiver_pack);
  auto folded_first = select_scalar(*folded_first_pack);
  if (!folded_first) {
    return Expression::Error(Expression::Error::Type::InvalidConstant, *this);
  }

  if (!element_type) {
    return Expression::Error(
        Expression::Error::Type::InvalidOperationType, *this);
  }
  const Language::Model::Type& element = element_type->get();

  auto selected_index = ::get_count(*folded_first, first);
  return selected_index.visit(
      [&](const Core::Option<Count>& index)
          -> Utility::Result<Core::Option<Model::Pack&>, Expression::Error> {
        if (!folded_receiver) {
          const Count available = folded_receiver_pack->get_layout().get_size();
          if (!count) {
            if (index && *index < available) {
              auto selected =
                  select_folded_entry(*folded_receiver_pack, *index);
              if (!selected) {
                return Expression::Error(
                    Expression::Error::Type::InvalidConstant, *this);
              }
              return Core::Option<Model::Pack&>(*selected);
            }

            if (!fallback) {
              return Expression::Error(
                  Expression::Error::Type::InvalidConstant, *this);
            }
            return fallback->get();
          }

          if (!range_count) {
            return Expression::Error(
                Expression::Error::Type::InvalidConstant, *this);
          }

          Memory::Managed::Vector<Ttx::Model::PackReference<Model::Pack>>
              entries(domain);
          entries.reset(*range_count);
          for (Count offset = 0; offset < *range_count; offset++) {
            Bool present = False;
            Count position = 0;
            if (index && offset <= Count(-1) - *index) {
              position = *index + offset;
              present = position < available;
            }

            if (present) {
              auto selected =
                  select_folded_entry(*folded_receiver_pack, position);
              if (!selected) {
                return Expression::Error(
                    Expression::Error::Type::InvalidConstant, *this);
              }
              entries.insert(*selected);
              continue;
            }

            auto selected_default = element.create_default(domain);
            if (!selected_default) {
              return Core::Option<Model::Pack&>{};
            }
            entries.insert(*selected_default);
          }
          return Model::Pack::create_folded(domain, entries.get_view());
        }

        return folded_receiver->visit<Constants::Bytes>(
            [&](const Constants::Bytes& bytes)
                -> Utility::Result<
                    Core::Option<Model::Pack&>, Expression::Error> {
              Core::View::Bytes value = bytes.get_value();
              if (!count) {
                if (!index || *index >= value.get_size()) {
                  // No payload element exists, so the exact element Type
                  // decides whether safe selection has a value or a
                  // failure.
                  if (!fallback) {
                    return Expression::Error(
                        Expression::Error::Type::InvalidConstant, *this);
                  }

                  Model::Pack& selected_fallback = fallback->get();
                  const Layout& fallback_layout =
                      selected_fallback.get_layout();
                  for (Count position = 0;
                       position < fallback_layout.get_size(); position++) {
                    auto fallback_value =
                        fallback_layout.get_abstract(position);
                    if (!fallback_value || !fallback_value->is<Constant>()) {
                      return Core::Option<Model::Pack&>{};
                    }
                  }
                  return selected_fallback;
                }

                auto byte_type = get_byte_type(element);
                if (!byte_type) {
                  return Expression::Error::from_pack(
                      Expression::Error::Type::InvalidConstant, receiver);
                }
                U64 selected = U64(value.get_data()[*index]);
                return static_cast<Model::Pack&>(
                    Constants::Unsigned::create_synthetic(
                        domain, *byte_type, selected));
              }

              if (!range_count) {
                return Expression::Error(
                    Expression::Error::Type::InvalidConstant, *this);
              }

              auto byte_type = get_byte_type(element);
              if (!byte_type) {
                return Expression::Error::from_pack(
                    Expression::Error::Type::InvalidConstant, receiver);
              }

              constexpr Count maximum_entries =
                  (Count(-1) - sizeof(U8*)) /
                  sizeof(Ttx::Model::PackReference<Model::Pack>);
              if (*range_count > maximum_entries) {
                // The semantic Pack can describe this count but no host Vector
                // can retain its folded producers without overflowing its byte
                // request. Leave the expression dynamic for another consumer.
                return Core::Option<Model::Pack&>{};
              }

              Memory::Managed::Vector<Ttx::Model::PackReference<Model::Pack>>
                  entries(domain);
              entries.reset(*range_count);
              for (Count offset = 0; offset < *range_count; offset++) {
                Bool has_position = False;
                Count position = 0;
                if (index && offset <= Count(-1) - *index) {
                  position = *index + offset;
                  has_position = position < value.get_size();
                }

                if (has_position) {
                  U64 selected = U64(value.get_data()[position]);
                  entries.insert(
                      static_cast<Model::Pack&>(
                          Constants::Unsigned::create_synthetic(
                              domain, *byte_type, selected)));
                  continue;
                }

                // Request a separate default for every missing slot. Reusing
                // one Pack would be observably wrong for Types whose default
                // creates a fresh value and would make a partial miss differ
                // from repeated scalar safe selection.
                auto selected_default = element.create_default(domain);
                if (!selected_default) {
                  return Core::Option<Model::Pack&>{};
                }

                Bool constant_default = True;
                const Layout& default_layout = selected_default->get_layout();
                for (Count default_index = 0;
                     default_index < default_layout.get_size();
                     default_index++) {
                  auto default_value =
                      default_layout.get_abstract(default_index);
                  constant_default &=
                      Bool(default_value && default_value->is<Constant>());
                }

                if (!constant_default) {
                  return Core::Option<Model::Pack&>{};
                }
                entries.insert(*selected_default);
              }

              return Model::Pack::create_folded(domain, entries.get_view());
            },
            [&](const Abstract&)
                -> Utility::Result<
                    Core::Option<Model::Pack&>, Expression::Error> {
              // Bytes is the live Constant payload domain. Another legal
              // Constant stays as Slice until its payload owner exists.
              return Core::Option<Model::Pack&>{};
            });
      },
      [](const Expression::Error& error)
          -> Utility::Result<Core::Option<Model::Pack&>, Expression::Error> {
        return error;
      });
}
