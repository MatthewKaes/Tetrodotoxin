// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/dialect.hpp"

#include "tetrodotoxin/library/interpreter/source/library.hpp"
#include "tetrodotoxin/library/language/monograph.hpp"
#include "tetrodotoxin/library/simulacra.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Ttx::Concept;
using Ttx::Semantic::Binding;
using namespace Ttx::Lexical;
using namespace Tetrodotoxin;

auto Library::Dialect::bind_interface(Perimortem::System::Uuid requested) const
    -> Perimortem::Utility::Result<Binding, Binding::Failure> {
  if (requested != Simulacra::contract_id) {
    return Tetrodotoxin::Language::Dialect::bind_interface(requested);
  }

  static const Simulacra::Operations operations = {
    [](const void*, Abstract::Handle source)
        -> Perimortem::Utility::Result<Simulacra, Simulacra::Failure> {
      return Simulacra::project(source);
    },
  };

  return Binding::provide<Simulacra>(this, operations);
}

auto Library::Dialect::interpret(
    Cursor& cursor,
    const Documentation& documentation,
    const Anchor& source_anchor,
    Abstract& context) -> Option<Tetrodotoxin::Language::Monograph&> {
  auto& monograph = Language::Monograph::create_authored(
      cursor.get_arena(), documentation, source_anchor, *this, context);
  Interpreter::Source::Library::parse(monograph.get_source(), cursor);
  return monograph;
}

auto Library::Dialect::encode(const Abstract& monograph) const
    -> Option<Dynamic::Bytes> {
  // Library projections have no byte container until Package supplies one.
  return {};
}

auto Library::Dialect::decode(
    Allocator::Arena& arena,
    View::Bytes payload,
    Abstract& context) -> Option<Ttx::Concept::Abstract&> {
  // The source reconstruction format is not the stored Library contract.
  return {};
}
