// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/bytes.hpp"
#include "perimortem/core/option.hpp"

#include "perimortem/memory/managed/bytes.hpp"

#include "tetrodotoxin/language/resource.hpp"
#include "tetrodotoxin/library/language/constant.hpp"
#include "tetrodotoxin/library/language/value.hpp"
#include "ttx/concept/reference.hpp"

namespace Tetrodotoxin::Library::Language::Constants {

// Bytes is the Tetrodotoxin::Library::Language::Constant contract for immutable
// byte array data. Quoted source, hexadecimal byte literals, and embedded files
// may all produce this value. Library defines no native String constant. A
// concrete owner may materialize its own String Type from these bytes through
// an ordinary Callable. The graph owner keeps the immutable backing storage
// alive for the Tetrodotoxin::Library::Language::Constant.
class Bytes : public Tetrodotoxin::Library::Language::Constant {
 public:
  TTX_CONTRACT(Bytes, Tetrodotoxin::Library::Language::Constant);
  using Value = Perimortem::Core::View::Bytes;

  auto bind_interface(Perimortem::System::Uuid requested) const
      -> Perimortem::Utility::Result<
          Ttx::Semantic::Binding,
          Ttx::Semantic::Binding::Failure> override {
    using Contract = Tetrodotoxin::Library::Language::Value;
    if (requested != Contract::contract_id) {
      return Constant::bind_interface(requested);
    }

    static const Contract::Operations operations = {
      [](const void* source) -> Ttx::Concept::Abstract::Handle {
        return static_cast<const Bytes*>(source)->get_type().get_interface();
      },
      [](const void* source, Perimortem::Memory::Allocator::Arena&)
          -> Perimortem::Core::View::Bytes {
        return static_cast<const Bytes*>(source)->get_value();
      },
      [](const void*) -> Count { return 1; },
    };
    return Ttx::Semantic::Binding::provide<Contract>(this, operations);
  }

  static auto create_authored(
      Perimortem::Memory::Allocator::Arena& domain,
      const Model::Type& type,
      Value value,
      Ttx::Lexical::Anchor anchor,
      Perimortem::Core::Option<const Tetrodotoxin::Language::Resource&>
          resource = {}) -> Bytes& {
    return Constant::create_authored<Bytes>(
        domain, anchor, [&](auto source) -> Bytes {
          return Bytes(domain, type, value, source, resource);
        });
  }

  static auto create_synthetic(
      Perimortem::Memory::Allocator::Arena& domain,
      const Model::Type& type,
      Value value,
      Perimortem::Core::Option<const Tetrodotoxin::Language::Resource&>
          resource = {}) -> Bytes& {
    return Constant::create_synthetic<Bytes>(domain, [&](auto source) -> Bytes {
      return Bytes(domain, type, value, source, resource);
    });
  }

  constexpr auto get_type() const -> const Model::Type& override {
    return type;
  }

  virtual constexpr auto get_value() const -> Value { return value; }

  auto get_name() const -> Perimortem::Core::View::Bytes override {
    return name.get_view();
  }

  auto resolve_concept(Perimortem::Core::View::Bytes name) const
      -> const Ttx::Concept::Abstract& override;
  auto visit_concepts(Ttx::Concept::Abstract::Visitor visitor) const
      -> void override;

  constexpr auto get_resource() const
      -> Perimortem::Core::Option<const Tetrodotoxin::Language::Resource&> {
    return resource.visit(
        []() -> Perimortem::Core::Option<
                 const Tetrodotoxin::Language::Resource&> { return {}; },
        [](const Ttx::Concept::Reference<
            const Tetrodotoxin::Language::Resource>& selected)
            -> Perimortem::Core::Option<
                const Tetrodotoxin::Language::Resource&> {
          return selected.get();
        });
  }

  constexpr auto equals(const Tetrodotoxin::Library::Language::Constant& rhs)
      const -> Bool override {
    return rhs.visit<Bytes>(
        [this, &rhs](const Bytes& selected) {
          return has_same_type(rhs) && get_value() == selected.get_value()
                     ? ::True
                     : ::False;
        },
        [](const Ttx::Concept::Abstract&) { return ::False; });
  }

 private:
  Bytes(
      Perimortem::Memory::Allocator::Arena& domain,
      const Model::Type& type,
      Value value,
      Perimortem::Core::Option<Ttx::Lexical::Anchor> anchor,
      Perimortem::Core::Option<const Tetrodotoxin::Language::Resource&>
          resource)
      : Tetrodotoxin::Library::Language::Constant(anchor),
        type(type),
        value(value),
        resource(resource.visit(
            []() -> Perimortem::Core::Option<Ttx::Concept::Reference<
                     const Tetrodotoxin::Language::Resource>> { return {}; },
            [](const Tetrodotoxin::Language::Resource& selected)
                -> Perimortem::Core::Option<Ttx::Concept::Reference<
                    const Tetrodotoxin::Language::Resource>> {
              return Ttx::Concept::Reference<
                  const Tetrodotoxin::Language::Resource>(selected);
            })),
        name(domain, "$["_view) {
    static constexpr U8 digits[] = "0123456789ABCDEF";
    for (Count index = 0; index < value.get_size(); index++) {
      if (index != 0) {
        name.append(' ');
      }
      U8 byte = value[index];
      name.append(digits[byte >> 4]);
      name.append(digits[byte & 0x0f]);
    }
    name.append(']');
  }

  const Model::Type& type;
  Value value;
  Perimortem::Core::Option<
      Ttx::Concept::Reference<const Tetrodotoxin::Language::Resource>>
      resource;
  Perimortem::Memory::Managed::Bytes name;
};

}  // namespace Tetrodotoxin::Library::Language::Constants
