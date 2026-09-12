// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/builtin/object/access.hpp"

#include "tetrodotoxin/library/language/types/object_storage.hpp"

using namespace Perimortem;
using namespace Ttx::Concept;
using namespace Tetrodotoxin::Library;

auto Builtin::Object::Access::create(
    Memory::Allocator::Arena& domain,
    const Language::Model::Type& receiver,
    const Language::Model::Type& result) -> Access& {
  auto& self = Ttx::Model::Layouts::Addressable::create_synthetic(
      domain, "self"_view, receiver);
  return domain.construct_from<Access>(
      [&]() -> Access { return Access(self, result); });
}

auto Builtin::Object::Access::accepts_receiver(
    const Abstract& receiver,
    const Abstract& host) const -> Bool {
  auto addressable = receiver.resolve().select<Language::Model::Memory>();
  auto access_scope = host.resolve().select<Language::Model::Type>();
  return addressable && access_scope &&
         addressable->permits_write_from(*access_scope);
}
