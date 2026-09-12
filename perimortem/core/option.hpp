// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/perimortem.hpp"

namespace Perimortem::Core {

// Option represents either one owned value or None. The value lives directly
// inside the Option so a function can safely return an object created on its
// stack. Construction and destruction follow the selected value's lifetime,
// including for types that can only move or lack default construction.
//
// A boolean check establishes the selected state before pointer shaped access.
// Visit remains available when both outcomes need their own behavior and return
// one common value.
template <typename value_type>
class Option {
 private:
  template <typename candidate_type>
  constexpr auto construct(candidate_type&& candidate) -> void {
    new (&value, Placement::Construct)
        value_type(static_cast<candidate_type&&>(candidate));
    set = true;
  }

  constexpr auto clear() -> void {
    if (set) {
      value.~value_type();
      set = false;
    }
  }

  union {
    value_type value;
  };
  Bool set = false;

 public:
  constexpr Option() : set(false) {}

  constexpr Option(const value_type& selected)
    requires(__is_constructible(value_type, const value_type&))
      : value(selected), set(true) {}

  constexpr Option(value_type&& selected)
    requires(__is_constructible(value_type, value_type &&))
      : value(static_cast<value_type&&>(selected)), set(true) {}

  constexpr Option(const Option& source)
    requires(__is_constructible(value_type, const value_type&))
      : set(false) {
    if (source.set) {
      construct(source.value);
    }
  }

  constexpr Option(Option&& source)
    requires(__is_constructible(value_type, value_type &&))
      : set(false) {
    if (source.set) {
      construct(static_cast<value_type&&>(source.value));
    }
  }

  constexpr ~Option() { clear(); }

  constexpr auto operator=(const Option& source) -> Option&
    requires(__is_constructible(value_type, const value_type&))
  {
    if (this == &source) {
      return *this;
    }

    clear();
    if (source.set) {
      construct(source.value);
    }
    return *this;
  }

  constexpr auto operator=(Option&& source) -> Option&
    requires(__is_constructible(value_type, value_type &&))
  {
    if (this == &source) {
      return *this;
    }

    clear();
    if (source.set) {
      construct(static_cast<value_type&&>(source.value));
    }
    return *this;
  }

  constexpr explicit operator bool() const { return bool(set); }
  constexpr explicit operator Bool() const { return set; }

  constexpr auto operator*() -> value_type& { return value; }
  constexpr auto operator*() const -> const value_type& { return value; }
  constexpr auto operator->() -> value_type* { return &value; }
  constexpr auto operator->() const -> const value_type* { return &value; }

  template <typename RejectCallback, typename ValueVisitor>
  constexpr auto visit(
      RejectCallback reject_callback,
      ValueVisitor value_visitor) -> decltype(auto) {
    if (!set) {
      return reject_callback();
    }

    return value_visitor(value);
  }

  template <typename RejectCallback, typename ValueVisitor>
  constexpr auto visit(
      RejectCallback reject_callback,
      ValueVisitor value_visitor) const -> decltype(auto) {
    if (!set) {
      return reject_callback();
    }

    return value_visitor(value);
  }
};

// A reference Option borrows its selected object and remains pointer sized. A
// const Option does not change the referent's type because constness belongs to
// the reference declared at the API boundary.
template <typename value_type>
class Option<value_type&> {
 public:
  constexpr Option() = default;
  constexpr Option(value_type& value) : value(&value) {}

  // A const reference can otherwise bind a temporary and leave Option holding
  // a dangling borrow. Requiring an lvalue makes the lifetime decision visible
  // at construction.
  Option(value_type&&) = delete;

  constexpr explicit operator bool() const { return value != nullptr; }
  constexpr explicit operator Bool() const { return value != nullptr; }

  constexpr auto operator*() const -> value_type& { return *value; }
  constexpr auto operator->() const -> value_type* { return value; }

  template <typename RejectCallback, typename ReferenceVisitor>
  constexpr auto visit(
      RejectCallback reject_callback,
      ReferenceVisitor reference_visitor) const -> decltype(auto) {
    if (value == nullptr) {
      return reject_callback();
    }

    return reference_visitor(*value);
  }

 private:
  value_type* value = nullptr;
};

}  // namespace Perimortem::Core
