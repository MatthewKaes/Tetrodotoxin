// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/memory/managed/bytes.hpp"

#include "perimortem/serialization/stream/textual.hpp"

#include "tetrodotoxin/library/language/constant.hpp"
#include "tetrodotoxin/library/language/model/types/real.hpp"
#include "tetrodotoxin/library/language/value.hpp"

namespace Tetrodotoxin::Library::Language::Constants {

// Real is an evaluated floating point
// Tetrodotoxin::Library::Language::Constant. Source decimal text may remain a
// Dialect owned literal Expression until a receiving Type selects a format, so
// constructing this contract never silently narrows an exact source literal.
// NaN values compare as one semantic value so
// Tetrodotoxin::Library::Language::Constant equality remains an equivalence
// relation suitable for Generic materialization keys.
class Real : public Tetrodotoxin::Library::Language::Constant {
 public:
  TTX_CONTRACT(Real, Tetrodotoxin::Library::Language::Constant);
  using Value = R64;

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
      const Tetrodotoxin::Library::Language::Model::Types::Real& type,
      Value value,
      Ttx::Lexical::Anchor anchor) -> Real& {
    return Constant::create_authored<Real>(
        domain, anchor,
        [&](auto source) -> Real { return Real(domain, type, value, source); });
  }

  static auto create_synthetic(
      Perimortem::Memory::Allocator::Arena& domain,
      const Tetrodotoxin::Library::Language::Model::Types::Real& type,
      Value value) -> Real& {
    return Constant::create_synthetic<Real>(domain, [&](auto source) -> Real {
      return Real(domain, type, value, source);
    });
  }

  constexpr auto get_type() const
      -> const Tetrodotoxin::Library::Language::Model::Types::Real& override {
    return type;
  }

  virtual constexpr auto get_value() const -> Value { return value; }

  auto get_name() const -> Perimortem::Core::View::Bytes override {
    return name.get_view();
  }

  constexpr auto equals(const Tetrodotoxin::Library::Language::Constant& rhs)
      const -> Bool override {
    return rhs.visit<Real>(
        [this, &rhs](const Real& selected) {
          if (!has_same_type(rhs)) {
            return ::False;
          }

          Value lhs_value = get_value();
          Value rhs_value = selected.get_value();
          return lhs_value == rhs_value || (__builtin_isnan(lhs_value) &&
                                            __builtin_isnan(rhs_value))
                     ? ::True
                     : ::False;
        },
        [](const Ttx::Concept::Abstract&) { return ::False; });
  }

  constexpr auto fits(const Ttx::Model::Type& target) const -> Bool override {
    const Ttx::Concept::Abstract& source_type = get_type().resolve();
    const Ttx::Concept::Abstract& target_type = target.resolve();
    return source_type
               .is<Tetrodotoxin::Library::Language::Model::Types::Real>() &&
           target_type
               .is<Tetrodotoxin::Library::Language::Model::Types::Real>() &&
           &source_type == &target_type;
  }

 private:
  Real(
      Perimortem::Memory::Allocator::Arena& domain,
      const Tetrodotoxin::Library::Language::Model::Types::Real& type,
      Value value,
      Perimortem::Core::Option<Ttx::Lexical::Anchor> anchor)
      : Tetrodotoxin::Library::Language::Constant(anchor),
        type(type),
        value(value),
        name(domain) {
    Perimortem::Serialization::Stream::Textual<
        Perimortem::Memory::Managed::Bytes>
        output(name);
    output << value;
  }

  const Tetrodotoxin::Library::Language::Model::Types::Real& type;
  Value value;
  Perimortem::Memory::Managed::Bytes name;
};

}  // namespace Tetrodotoxin::Library::Language::Constants
