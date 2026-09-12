// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/builtin/object/clone.hpp"

using namespace Perimortem;
using namespace Ttx::Concept;
using namespace Tetrodotoxin::Library;

auto Builtin::Object::Clone::create(
    Memory::Allocator::Arena& domain,
    const Language::Model::Type& receiver) -> Clone& {
  Ttx::Model::Layouts::Addressable& self =
      Ttx::Model::Layouts::Addressable::create_synthetic(
          domain, "self"_view, receiver);
  return domain.construct_from<Clone>([&]() -> Clone { return Clone(self); });
}

auto Builtin::Object::Clone::accepts_receiver(
    const Abstract& receiver,
    const Abstract& host) const -> Bool {
  auto addressable = receiver.resolve().select<Language::Model::Memory>();
  auto access_scope = host.resolve().select<Language::Model::Type>();
  return addressable && access_scope &&
         addressable->permits_write_from(*access_scope);
}
