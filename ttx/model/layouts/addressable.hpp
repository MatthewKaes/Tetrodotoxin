// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/memory/allocator/arena.hpp"

#include "ttx/model/addressable.hpp"

namespace Ttx::Model::Layouts {

// Addressable is one named Layout slot with an exact value Type. The Layout
// remains the owner of source order, attributes, and authored location; this
// identity exists only so graph consumers can refer to that real slot.
class Addressable : public Ttx::Model::Addressable {
 public:
  TTX_CONTRACT(Addressable, Ttx::Model::Addressable);

  static auto create_authored(
      Perimortem::Memory::Allocator::Arena& arena,
      Perimortem::Core::View::Bytes name,
      const Ttx::Model::Type& type,
      Perimortem::Core::Option<Concept::Abstract::Handle> source = {})
      -> Perimortem::Core::Option<Addressable&>;

  static auto create_synthetic(
      Perimortem::Memory::Allocator::Arena& arena,
      Perimortem::Core::View::Bytes name,
      const Ttx::Model::Type& type) -> Addressable&;

  Addressable(const Addressable&) = delete;
  Addressable(Addressable&&) = delete;
  auto operator=(const Addressable&) -> Addressable& = delete;
  auto operator=(Addressable&&) -> Addressable& = delete;

  TTX_NAME(name);
  TTX_EMPTY_DOCUMENTATION();

  auto bind_interface(Perimortem::System::Uuid requested) const -> Perimortem::
      Utility::Result<Semantic::Binding, Semantic::Binding::Failure> override {
    using Contract = Ttx::Model::Addressable;
    if (requested != Contract::contract_id) {
      return Contract::bind_interface(requested);
    }
    static const Contract::Operations operations = {
      [](const void* provider) -> Concept::Abstract::Handle {
        return static_cast<const Addressable*>(provider)->source;
      },
    };
    return Semantic::Binding::provide<Contract>(this, operations);
  }

  constexpr auto get_type() const -> const Ttx::Model::Type& override {
    return type;
  }

 private:
  constexpr Addressable(
      Perimortem::Core::View::Bytes name,
      const Ttx::Model::Type& type,
      Concept::Abstract::Handle source)
      : name(name), type(type), source(source) {}

  Perimortem::Core::View::Bytes name;
  const Ttx::Model::Type& type;
  // Native fitting keeps the completed Type. Boundary queries retain the
  // source edge used to obtain it, which may carry an Import policy.
  Concept::Abstract::Handle source;
};

}  // namespace Ttx::Model::Layouts
