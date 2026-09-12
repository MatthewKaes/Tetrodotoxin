// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/shader/archive/reader.hpp"

#include "perimortem/core/reader/binary.hpp"

#include "perimortem/memory/managed/vector.hpp"

#include "tetrodotoxin/library/language/field.hpp"
#include "tetrodotoxin/render/language/attributes.hpp"
#include "ttx/concept/unknown.hpp"
#include "ttx/lexical/anchor.hpp"
#include "ttx/model/documentations/block.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Ttx::Concept;
using namespace Tetrodotoxin;

enum class ShaderReaderAttributeValue : U8 {
  Empty,
  Bytes,
  Unsigned,
  Signed,
  Real,
  Flag,
};

auto Shader::Archive::Reader::Definition::create(
    Allocator::Arena& arena,
    Abstract& host) const -> Tetrodotoxin::Language::Definition& {
  return Tetrodotoxin::Language::Definition::create_restored(
      arena, documentation, host, attributes, name, visibility);
}

auto Shader::Archive::Reader::open(View::Bytes payload) -> Option<Reader> {
  BAIL_IF(payload.get_size() < 8);
  Perimortem::Core::Reader::Binary<Data::ByteOrder::Little> reader(
      payload.slice(0, 8));
  auto magic = reader.read_bytes(4);
  auto version = reader.read_u16();
  auto flags = reader.read_u16();
  BAIL_IF(!magic || !version || !flags);
  BAIL_IF(*magic != "TTXS"_view || *version != 2 || *flags != 0);
  return Reader(payload.slice(8));
}

auto Shader::Archive::Reader::restore(
    Allocator::Arena& arena,
    View::Bytes payload,
    const Abstract& language,
    const Library::Dialect& library,
    Abstract& context) -> Option<Ttx::Concept::Abstract&> {
  // Shader and its Library child share one reconstruction Arena just as they
  // share one authored source transaction. Program records can then restore
  // Library declarations into the exact Shader subtype they describe.
  auto opened = open(payload);
  BAIL_IF(!opened);
  auto record = opened->read_record();
  BAIL_IF(
      !record || record->get_tag() != U16(Tag::Monograph) ||
      !opened->is_complete());

  Reader contents(record->get_payload());
  auto documentation = contents.read_documentation(arena);
  BAIL_IF(!documentation);
  auto& child = Library::Language::Monograph::create(
      arena, *documentation, Ttx::Lexical::Anchor::create(Ttx::Lexical::Span()),
      library, context);
  auto& monograph = Shader::Language::Monograph::create(
      arena, language, *documentation, context, child);
  while (!contents.is_complete()) {
    Reader probe = contents;
    auto next = probe.read_record();
    BAIL_IF(!next);
    if (next->get_tag() == U16(Tag::Program)) {
      BAIL_IF(!contents.read_program(arena, monograph));
    } else if (next->get_tag() == U16(Tag::Bridge)) {
      BAIL_IF(!contents.read_bridge(arena, monograph));
    } else {
      return {};
    }
  }
  return monograph;
}

auto Shader::Archive::Reader::take(Count size) -> Option<View::Bytes> {
  BAIL_IF(
      location > payload.get_size() || size > payload.get_size() - location);
  View::Bytes selected = payload.slice(location, size);
  location += size;
  return selected;
}

auto Shader::Archive::Reader::read_record() -> Option<Record> {
  auto header = take(8);
  BAIL_IF(!header);
  Perimortem::Core::Reader::Binary<Data::ByteOrder::Little> reader(*header);
  auto tag = reader.read_u16();
  auto flags = reader.read_u16();
  auto size = reader.read_u32();
  BAIL_IF(!tag || !flags || !size || *flags != 0);
  auto selected = take(*size);
  BAIL_IF(!selected);
  return Record(*tag, *selected);
}

auto Shader::Archive::Reader::read_u8() -> Option<U8> {
  auto selected = take(sizeof(U8));
  BAIL_IF(!selected);
  return Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>(*selected)
      .read_u8();
}

auto Shader::Archive::Reader::read_u32() -> Option<U32> {
  auto selected = take(sizeof(U32));
  BAIL_IF(!selected);
  return Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>(*selected)
      .read_u32();
}

auto Shader::Archive::Reader::read_u64() -> Option<U64> {
  auto selected = take(sizeof(U64));
  BAIL_IF(!selected);
  return Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>(*selected)
      .read_u64();
}

auto Shader::Archive::Reader::read_s64() -> Option<S64> {
  auto selected = take(sizeof(S64));
  BAIL_IF(!selected);
  return Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>(*selected)
      .read_s64();
}

auto Shader::Archive::Reader::read_r64() -> Option<R64> {
  auto selected = take(sizeof(R64));
  BAIL_IF(!selected);
  return Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>(*selected)
      .read_r64();
}

auto Shader::Archive::Reader::read_bytes() -> Option<View::Bytes> {
  auto size = read_u32();
  BAIL_IF(!size);
  return take(*size);
}

auto Shader::Archive::Reader::read_documentation(Allocator::Arena& arena)
    -> Option<const Documentation&> {
  auto count = read_u32();
  BAIL_IF(!count || Count(*count) > payload.get_size());
  auto lines = arena.reserve<View::Bytes>(*count);
  for (Count index = 0; index < *count; index++) {
    auto line = read_bytes();
    BAIL_IF(!line);
    lines.get_data()[index] = arena.proxy(*line);
  }
  return arena.construct<Ttx::Model::Documentations::Block>(
      View::Vector<View::Bytes>(lines.get_data(), lines.get_size()));
}

auto Shader::Archive::Reader::read_attributes(Allocator::Arena& arena)
    -> Option<View::Vector<Tetrodotoxin::Language::Attribute>> {
  auto count = read_u32();
  BAIL_IF(!count || Count(*count) > payload.get_size());
  Managed::Vector<Tetrodotoxin::Language::Attribute> attributes(arena);
  for (Count index = 0; index < *count; index++) {
    auto key = read_bytes();
    auto kind = read_u8();
    BAIL_IF(!key || key->is_empty() || !kind);
    Tetrodotoxin::Language::Attribute::Value value;
    switch (ShaderReaderAttributeValue(*kind)) {
    case ShaderReaderAttributeValue::Empty:
      break;
    case ShaderReaderAttributeValue::Bytes: {
      auto selected = read_bytes();
      BAIL_IF(!selected);
      value = Tetrodotoxin::Language::Attribute::Value(arena.proxy(*selected));
      break;
    }
    case ShaderReaderAttributeValue::Unsigned: {
      auto selected = read_u64();
      BAIL_IF(!selected);
      value = Tetrodotoxin::Language::Attribute::Value(*selected);
      break;
    }
    case ShaderReaderAttributeValue::Signed: {
      auto selected = read_s64();
      BAIL_IF(!selected);
      value = Tetrodotoxin::Language::Attribute::Value(*selected);
      break;
    }
    case ShaderReaderAttributeValue::Real: {
      auto selected = read_r64();
      BAIL_IF(!selected);
      value = Tetrodotoxin::Language::Attribute::Value(*selected);
      break;
    }
    case ShaderReaderAttributeValue::Flag: {
      auto selected = read_u8();
      BAIL_IF(!selected || *selected > 1);
      value = Tetrodotoxin::Language::Attribute::Value(
          *selected == 1 ? True : False);
      break;
    }
    default:
      return {};
    }
    attributes.insert(
        Tetrodotoxin::Language::Attribute::create_synthetic(
            arena.proxy(*key), value));
  }
  return attributes.get_view();
}

auto Shader::Archive::Reader::read_definition(Allocator::Arena& arena)
    -> Option<Definition> {
  auto documentation = read_documentation(arena);
  auto attributes = read_attributes(arena);
  auto name = read_bytes();
  auto visibility = read_u8();
  BAIL_IF(
      !documentation || !attributes || !name || name->is_empty() ||
      !visibility ||
      *visibility > U8(Tetrodotoxin::Language::Visibility::Exposed));
  auto selected_visibility = Tetrodotoxin::Language::Visibility(*visibility);
  return Definition(
      *documentation, *attributes, arena.proxy(*name), selected_visibility);
}

auto Shader::Archive::Reader::read_program(
    Allocator::Arena& arena,
    Shader::Language::Monograph& monograph)
    -> Option<Shader::Language::Program&> {
  // Program declarations require the deferred Library projection decoder.
  return {};
}

auto Shader::Archive::Reader::read_bridge(
    Allocator::Arena& arena,
    Shader::Language::Monograph& monograph)
    -> Option<Shader::Language::Bridge&> {
  // Bridge references require the deferred Package dependency decoder.
  return {};
}
