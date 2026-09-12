// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/system/uuid.hpp"

#include "ttx/concept/abstract.hpp"
#include "ttx/semantic/bound.hpp"

namespace Ttx::Concept {

// Layout describes the shape of semantic flow without creating a graph
// identity. Its slots borrow the real Abstracts that supply that flow, while
// the concrete owner decides how their shapes fit a receiving Layout.
class Layout {
 public:
  static constexpr Perimortem::System::Uuid contract_id{
    0x01a084b0c85e7be3,
    0x9067f72064c8876f,
  };

  // The observational interface retains slot identity, order and names.
  // Fitting is a separate operation with its own domain rules. A stored
  // description can supply these answers without constructing a native
  // Layout or borrowing its producer's implementation.
  struct Operations {
    auto (*get_size)(const void*) -> Count;
    auto (*get_abstract)(const void*, Count)
        -> Perimortem::Core::Option<Abstract::Handle>;
    auto (*get_name)(const void*, Count)
        -> Perimortem::Core::Option<Perimortem::Core::View::Bytes>;
  };

  class Handle : public Semantic::Bound<Operations> {
   public:
    using Bound::Bound;

    auto get_size() const -> Count { return operations.get_size(source); }

    auto get_abstract(Count index) const
        -> Perimortem::Core::Option<Abstract::Handle> {
      return operations.get_abstract(source, index);
    }

    auto get_name(Count index) const
        -> Perimortem::Core::Option<Perimortem::Core::View::Bytes> {
      return operations.get_name(source, index);
    }
  };

  virtual auto get_interface() const -> Handle {
    static const Operations operations = {
      [](const void* source) -> Count {
        return static_cast<const Layout*>(source)->get_size();
      },
      [](const void* source,
         Count index) -> Perimortem::Core::Option<Abstract::Handle> {
        auto selected = static_cast<const Layout*>(source)->get_abstract(index);
        if (!selected) {
          return {};
        }
        return selected->get_interface();
      },
      [](const void* source, Count index)
          -> Perimortem::Core::Option<Perimortem::Core::View::Bytes> {
        return static_cast<const Layout*>(source)->get_name(index);
      },
    };
    return Handle(this, operations);
  }

  enum class Errors : U8 {
    IndexOutOfBounds,
    SizeMismatch,
    IncompatibleFit,
  };

  constexpr virtual ~Layout() = default;

  virtual constexpr auto get_size() const -> Count = 0;
  virtual constexpr auto get_abstract(Count index) const
      -> Perimortem::Core::Option<const Abstract&> = 0;
  virtual constexpr auto get_name(Count index) const
      -> Perimortem::Core::Option<Perimortem::Core::View::Bytes> {
    return {};
  }
  virtual constexpr auto fits_entry(
      const Layout& target,
      Count source_index,
      Count target_index) const -> Bool = 0;
  constexpr auto fits(const Layout& target) const -> Bool {
    return get_size() == target.get_size() && fits_at(target, 0);
  }
  virtual constexpr auto fits_at(const Layout& target, Count target_offset)
      const -> Bool = 0;
  constexpr auto get_fitted(const Layout& target, Count target_index) const
      -> Perimortem::Utility::Result<const Abstract&, Errors> {
    if (target_index >= get_size()) {
      return Errors::IndexOutOfBounds;
    }

    if (get_size() != target.get_size()) {
      return Errors::SizeMismatch;
    }

    return get_fitted_at(target, 0, target_index);
  }
  virtual constexpr auto get_fitted_at(
      const Layout& target,
      Count target_offset,
      Count target_index) const
      -> Perimortem::Utility::Result<const Abstract&, Errors> = 0;

  constexpr auto is_empty() const -> Bool { return get_size() == 0; }

 protected:
  constexpr auto has_target_segment(const Layout& target, Count target_offset)
      const -> Bool {
    return target_offset <= target.get_size() &&
           get_size() <= target.get_size() - target_offset;
  }
};

}  // namespace Ttx::Concept
