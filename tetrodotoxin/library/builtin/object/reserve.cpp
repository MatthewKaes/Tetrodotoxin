// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/builtin/object/reserve.hpp"

#include "tetrodotoxin/library/language/types/object_storage.hpp"

using namespace Perimortem;
using namespace Ttx::Concept;
using namespace Tetrodotoxin::Library;

static auto create_entries(
    Ttx::Model::Layouts::Addressable& self,
    Ttx::Model::Layouts::Addressable& count)
    -> Core::Static::Vector<Reference<const Abstract>, 2> {
  const Core::Static::Vector<Reference<const Abstract>, 2> entries = {{
    Reference<const Abstract>(self),
    Reference<const Abstract>(count),
  }};
  return entries;
}

Builtin::Object::Reserve::Reserve(
    Ttx::Model::Layouts::Addressable& self,
    Ttx::Model::Layouts::Addressable& count,
    const Language::Model::Type& result)
    : parameter_entries(create_entries(self, count)),
      parameters(parameter_entries.get_view()),
      results(result, 1) {}

auto Builtin::Object::Reserve::create(
    Memory::Allocator::Arena& domain,
    const Language::Model::Type& receiver,
    const Language::Model::Type& count,
    const Language::Model::Type& result) -> Reserve& {
  auto& self = Ttx::Model::Layouts::Addressable::create_synthetic(
      domain, "self"_view, receiver);
  auto& count_parameter = Ttx::Model::Layouts::Addressable::create_synthetic(
      domain, "count"_view, count);
  return domain.construct_from<Reserve>(
      [&]() -> Reserve { return Reserve(self, count_parameter, result); });
}

auto Builtin::Object::Reserve::accepts_receiver(
    const Abstract& receiver,
    const Abstract& host) const -> Bool {
  auto addressable = receiver.resolve().select<Language::Model::Memory>();
  auto access_scope = host.resolve().select<Language::Model::Type>();
  return addressable && access_scope &&
         addressable->permits_write_from(*access_scope);
}
