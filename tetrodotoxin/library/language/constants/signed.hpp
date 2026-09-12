// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/memory/managed/bytes.hpp"

#include "perimortem/serialization/stream/textual.hpp"

#include "tetrodotoxin/library/language/constant.hpp"
#include "tetrodotoxin/library/language/model/types/signed.hpp"
#include "tetrodotoxin/library/language/value.hpp"

namespace Tetrodotoxin::Library::Language::Constants {

// Signed is the Tetrodotoxin::Library::Language::Constant contract for a signed
// integer value. Its resolved Type remains part of identity while fitting may
// prove that the value is in range for another Signed width.
class Signed : public Tetrodotoxin::Library::Language::Constant {
 public:
  TTX_CONTRACT(Signed, Tetrodotoxin::Library::Language::Constant);
  using Value = S64;

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
      const Tetrodotoxin::Library::Language::Model::Types::Signed& type,
      Value value,
      Ttx::Lexical::Anchor anchor) -> Signed& {
    return Constant::create_authored<Signed>(
        domain, anchor, [&](auto source) -> Signed {
          return Signed(domain, type, value, source);
        });
  }

  static auto create_synthetic(
      Perimortem::Memory::Allocator::Arena& domain,
      const Tetrodotoxin::Library::Language::Model::Types::Signed& type,
      Value value) -> Signed& {
    return Constant::create_synthetic<Signed>(
        domain, [&](auto source) -> Signed {
          return Signed(domain, type, value, source);
        });
  }

  constexpr auto get_type() const
      -> const Tetrodotoxin::Library::Language::Model::Types::Signed& override {
    return type;
  }

  virtual constexpr auto get_value() const -> Value { return value; }

  auto get_name() const -> Perimortem::Core::View::Bytes override {
    return name.get_view();
  }

  constexpr auto equals(const Tetrodotoxin::Library::Language::Constant& rhs)
      const -> Bool override {
    return rhs.visit<Signed>(
        [this, &rhs](const Signed& selected) {
          return has_same_type(rhs) && get_value() == selected.get_value()
                     ? ::True
                     : ::False;
        },
        [](const Ttx::Concept::Abstract&) { return ::False; });
  }

  constexpr auto fits(const Ttx::Model::Type& target) const -> Bool override {
    if (!get_type()
             .resolve()
             .is<Tetrodotoxin::Library::Language::Model::Types::Signed>()) {
      return ::False;
    }

    const Ttx::Concept::Abstract& target_type = target.resolve();
    return target_type
        .visit<Tetrodotoxin::Library::Language::Model::Types::Signed>(
            [this](
                const Tetrodotoxin::Library::Language::Model::Types::Signed&
                    selected) {
              Count size = selected.get_size();
              if (size == 0) {
                return ::False;
              }

              if (size >= sizeof(S64)) {
                return ::True;
              }

              S64 limit = S64(1) << (size * 8 - 1);
              return get_value() >= -limit && get_value() < limit ? ::True
                                                                  : ::False;
            },
            [](const Ttx::Concept::Abstract&) { return ::False; });
  }

 private:
  Signed(
      Perimortem::Memory::Allocator::Arena& domain,
      const Tetrodotoxin::Library::Language::Model::Types::Signed& type,
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

  const Tetrodotoxin::Library::Language::Model::Types::Signed& type;
  Value value;
  Perimortem::Memory::Managed::Bytes name;
};

}  // namespace Tetrodotoxin::Library::Language::Constants
