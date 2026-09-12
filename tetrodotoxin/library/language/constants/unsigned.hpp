// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/memory/managed/bytes.hpp"

#include "perimortem/serialization/stream/textual.hpp"

#include "tetrodotoxin/library/language/constant.hpp"
#include "tetrodotoxin/library/language/model/types/unsigned.hpp"
#include "tetrodotoxin/library/language/value.hpp"

namespace Tetrodotoxin::Library::Language::Constants {

// Unsigned is the Tetrodotoxin::Library::Language::Constant contract for a
// nonnegative integer value. The resolved Type supplies the authored width
// while the value remains wide enough to prove whether a narrower Unsigned
// target can represent it.
class Unsigned : public Tetrodotoxin::Library::Language::Constant {
 public:
  TTX_CONTRACT(Unsigned, Tetrodotoxin::Library::Language::Constant);
  using Value = U64;

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
      const Tetrodotoxin::Library::Language::Model::Types::Unsigned& type,
      Value value,
      Ttx::Lexical::Anchor anchor) -> Unsigned& {
    return Constant::create_authored<Unsigned>(
        domain, anchor, [&](auto source) -> Unsigned {
          return Unsigned(domain, type, value, source);
        });
  }

  static auto create_synthetic(
      Perimortem::Memory::Allocator::Arena& domain,
      const Tetrodotoxin::Library::Language::Model::Types::Unsigned& type,
      Value value) -> Unsigned& {
    return Constant::create_synthetic<Unsigned>(
        domain, [&](auto source) -> Unsigned {
          return Unsigned(domain, type, value, source);
        });
  }

  constexpr auto get_type() const -> const
      Tetrodotoxin::Library::Language::Model::Types::Unsigned& override {
    return type;
  }

  virtual constexpr auto get_value() const -> Value { return value; }

  auto get_name() const -> Perimortem::Core::View::Bytes override {
    return name.get_view();
  }

  constexpr auto equals(const Tetrodotoxin::Library::Language::Constant& rhs)
      const -> Bool override {
    return rhs.visit<Unsigned>(
        [this, &rhs](const Unsigned& selected) {
          return has_same_type(rhs) && get_value() == selected.get_value()
                     ? ::True
                     : ::False;
        },
        [](const Ttx::Concept::Abstract&) { return ::False; });
  }

  constexpr auto fits(const Ttx::Model::Type& target) const -> Bool override {
    if (!get_type()
             .resolve()
             .is<Tetrodotoxin::Library::Language::Model::Types::Unsigned>()) {
      return ::False;
    }

    const Ttx::Concept::Abstract& target_type = target.resolve();
    return target_type
        .visit<Tetrodotoxin::Library::Language::Model::Types::Unsigned>(
            [this](
                const Tetrodotoxin::Library::Language::Model::Types::Unsigned&
                    selected) {
              Count size = selected.get_size();
              if (size == 0) {
                return ::False;
              }

              if (size >= sizeof(U64)) {
                return ::True;
              }

              return get_value() < (U64(1) << (size * 8)) ? ::True : ::False;
            },
            [](const Ttx::Concept::Abstract&) { return ::False; });
  }

 private:
  Unsigned(
      Perimortem::Memory::Allocator::Arena& domain,
      const Tetrodotoxin::Library::Language::Model::Types::Unsigned& type,
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

  const Tetrodotoxin::Library::Language::Model::Types::Unsigned& type;
  Value value;
  Perimortem::Memory::Managed::Bytes name;
};

}  // namespace Tetrodotoxin::Library::Language::Constants
