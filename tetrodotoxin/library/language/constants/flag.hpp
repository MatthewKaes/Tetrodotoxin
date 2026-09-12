// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "tetrodotoxin/library/language/constant.hpp"
#include "tetrodotoxin/library/language/model/types/flag.hpp"
#include "tetrodotoxin/library/language/value.hpp"

namespace Tetrodotoxin::Library::Language::Constants {

// Flag is the Tetrodotoxin::Library::Language::Constant contract for a binary
// logical value. Either value fits every resolved Flag Type regardless of the
// toolchain's chosen storage width.
class Flag : public Tetrodotoxin::Library::Language::Constant {
 public:
  TTX_CONTRACT(Flag, Tetrodotoxin::Library::Language::Constant);
  using Value = Bool;

  auto bind_interface(Perimortem::System::Uuid requested) const
      -> Perimortem::Utility::Result<
          Ttx::Semantic::Binding,
          Ttx::Semantic::Binding::Failure> override {
    using Contract = Tetrodotoxin::Library::Language::Value;
    if (requested == Contract::contract_id) {
      return Contract::scalar(*this);
    }
    return Constant::bind_interface(requested);
  }

  static auto create_authored(
      Perimortem::Memory::Allocator::Arena& domain,
      const Tetrodotoxin::Library::Language::Model::Types::Flag& type,
      Value value,
      Ttx::Lexical::Anchor anchor) -> Flag& {
    return Constant::create_authored<Flag>(
        domain, anchor,
        [&](auto source) -> Flag { return Flag(type, value, source); });
  }

  static auto create_synthetic(
      Perimortem::Memory::Allocator::Arena& domain,
      const Tetrodotoxin::Library::Language::Model::Types::Flag& type,
      Value value) -> Flag& {
    return Constant::create_synthetic<Flag>(
        domain, [&](auto source) -> Flag { return Flag(type, value, source); });
  }

  constexpr auto get_type() const
      -> const Tetrodotoxin::Library::Language::Model::Types::Flag& override {
    return type;
  }

  virtual constexpr auto get_value() const -> Value { return value; }

  constexpr auto get_name() const -> Perimortem::Core::View::Bytes override {
    return value ? "true"_view : "false"_view;
  }

  constexpr auto equals(const Tetrodotoxin::Library::Language::Constant& rhs)
      const -> Bool override {
    return rhs.visit<Flag>(
        [this, &rhs](const Flag& selected) {
          return has_same_type(rhs) && get_value() == selected.get_value()
                     ? ::True
                     : ::False;
        },
        [](const Ttx::Concept::Abstract&) { return ::False; });
  }

  constexpr auto fits(const Ttx::Model::Type& target) const -> Bool override {
    return get_type()
               .resolve()
               .is<Tetrodotoxin::Library::Language::Model::Types::Flag>() &&
           target.resolve()
               .is<Tetrodotoxin::Library::Language::Model::Types::Flag>();
  }

 protected:
  constexpr Flag(
      const Tetrodotoxin::Library::Language::Model::Types::Flag& type,
      Value value,
      Perimortem::Core::Option<Ttx::Lexical::Anchor> anchor)
      : Tetrodotoxin::Library::Language::Constant(anchor),
        type(type),
        value(value) {}

 private:
  const Tetrodotoxin::Library::Language::Model::Types::Flag& type;
  Value value;
};

}  // namespace Tetrodotoxin::Library::Language::Constants
