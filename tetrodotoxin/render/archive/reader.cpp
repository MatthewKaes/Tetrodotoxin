// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/render/archive/reader.hpp"

#include "perimortem/core/reader/binary.hpp"

#include "perimortem/memory/managed/vector.hpp"

#include "tetrodotoxin/render/language/alias.hpp"
#include "tetrodotoxin/render/language/attributes.hpp"
#include "tetrodotoxin/render/language/binding.hpp"
#include "tetrodotoxin/render/language/stage.hpp"
#include "tetrodotoxin/render/language/structure.hpp"
#include "ttx/lexical/anchor.hpp"
#include "ttx/model/documentations/block.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Ttx::Concept;
using namespace Tetrodotoxin;

enum class RenderReaderAttributeValue : U8 {
  Empty,
  Bytes,
  Unsigned,
  Signed,
  Real,
  Flag,
};

auto Render::Archive::Reader::Definition::create(
    Allocator::Arena& arena,
    Abstract& host) const -> Tetrodotoxin::Language::Definition& {
  return Tetrodotoxin::Language::Definition::create_restored(
      arena, documentation, host, attributes, name, visibility);
}

auto Render::Archive::Reader::open(View::Bytes payload) -> Option<Reader> {
  BAIL_IF(payload.get_size() < 8);
  Perimortem::Core::Reader::Binary<Data::ByteOrder::Little> reader(
      payload.slice(0, 8));
  auto magic = reader.read_bytes(4);
  auto version = reader.read_u16();
  auto flags = reader.read_u16();
  BAIL_IF(!magic || !version || !flags);
  BAIL_IF(*magic != "TTXR"_view || *version != 2 || *flags != 0);
  return Reader(payload.slice(8));
}

auto Render::Archive::Reader::restore(
    Allocator::Arena& arena,
    View::Bytes payload,
    const Abstract& language,
    Abstract& context) -> Option<Ttx::Concept::Abstract&> {
  // The outer record bounds every declaration before any graph identity is
  // created. A malformed sibling is therefore confined to its own payload and
  // cannot consume the valid bytes that follow it.
  auto opened = open(payload);
  BAIL_IF(!opened);
  auto record = opened->read_record();
  BAIL_IF(
      !record || record->get_tag() != U16(Tag::Monograph) ||
      !opened->is_complete());

  Reader contents(record->get_payload());
  auto documentation = contents.read_documentation(arena);
  BAIL_IF(!documentation);
  auto& monograph = Render::Language::Monograph::create(
      arena, language, *documentation, context);
  BAIL_IF(!contents.read_entries(arena, monograph) || !contents.is_complete());
  return monograph;
}

auto Render::Archive::Reader::take(Count size) -> Option<View::Bytes> {
  BAIL_IF(
      location > payload.get_size() || size > payload.get_size() - location);
  View::Bytes selected = payload.slice(location, size);
  location += size;
  return selected;
}

auto Render::Archive::Reader::read_record() -> Option<Record> {
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

auto Render::Archive::Reader::read_u8() -> Option<U8> {
  auto selected = take(sizeof(U8));
  BAIL_IF(!selected);
  return Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>(*selected)
      .read_u8();
}

auto Render::Archive::Reader::read_u32() -> Option<U32> {
  auto selected = take(sizeof(U32));
  BAIL_IF(!selected);
  return Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>(*selected)
      .read_u32();
}

auto Render::Archive::Reader::read_u64() -> Option<U64> {
  auto selected = take(sizeof(U64));
  BAIL_IF(!selected);
  return Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>(*selected)
      .read_u64();
}

auto Render::Archive::Reader::read_s64() -> Option<S64> {
  auto selected = take(sizeof(S64));
  BAIL_IF(!selected);
  return Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>(*selected)
      .read_s64();
}

auto Render::Archive::Reader::read_r64() -> Option<R64> {
  auto selected = take(sizeof(R64));
  BAIL_IF(!selected);
  return Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>(*selected)
      .read_r64();
}

auto Render::Archive::Reader::read_bytes() -> Option<View::Bytes> {
  auto size = read_u32();
  BAIL_IF(!size);
  return take(*size);
}

auto Render::Archive::Reader::read_documentation(Allocator::Arena& arena)
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

auto Render::Archive::Reader::read_attributes(Allocator::Arena& arena)
    -> Option<View::Vector<Tetrodotoxin::Language::Attribute>> {
  auto count = read_u32();
  BAIL_IF(!count || Count(*count) > payload.get_size());
  Managed::Vector<Tetrodotoxin::Language::Attribute> attributes(arena);
  for (Count index = 0; index < *count; index++) {
    auto key = read_bytes();
    auto kind = read_u8();
    BAIL_IF(!key || key->is_empty() || !kind);
    Tetrodotoxin::Language::Attribute::Value value;
    switch (RenderReaderAttributeValue(*kind)) {
    case RenderReaderAttributeValue::Empty:
      break;
    case RenderReaderAttributeValue::Bytes: {
      auto selected = read_bytes();
      BAIL_IF(!selected);
      value = Tetrodotoxin::Language::Attribute::Value(arena.proxy(*selected));
      break;
    }
    case RenderReaderAttributeValue::Unsigned: {
      auto selected = read_u64();
      BAIL_IF(!selected);
      value = Tetrodotoxin::Language::Attribute::Value(*selected);
      break;
    }
    case RenderReaderAttributeValue::Signed: {
      auto selected = read_s64();
      BAIL_IF(!selected);
      value = Tetrodotoxin::Language::Attribute::Value(*selected);
      break;
    }
    case RenderReaderAttributeValue::Real: {
      auto selected = read_r64();
      BAIL_IF(!selected);
      value = Tetrodotoxin::Language::Attribute::Value(*selected);
      break;
    }
    case RenderReaderAttributeValue::Flag: {
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

auto Render::Archive::Reader::read_definition(Allocator::Arena& arena)
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

auto Render::Archive::Reader::read_type_reference(Allocator::Arena& arena)
    -> Option<Tetrodotoxin::Language::TypeReference> {
  auto route = read_bytes();
  BAIL_IF(!route || route->is_empty());
  return Tetrodotoxin::Language::TypeReference::create(
      arena.proxy(*route), Ttx::Lexical::Anchor::create(Ttx::Lexical::Span()));
}

auto Render::Archive::Reader::read_layout(Allocator::Arena& arena)
    -> Option<Render::Language::Layout&> {
  auto record = read_record();
  BAIL_IF(!record || record->get_tag() != U16(Tag::Layout));
  Reader contents(record->get_payload());
  auto parameters = contents.read_u8();
  auto count = contents.read_u32();
  BAIL_IF(!parameters || *parameters > 1 || !count);
  Managed::Vector<Render::Language::Layout::Slot> slots(arena);
  for (Count index = 0; index < *count; index++) {
    auto slot_record = contents.read_record();
    BAIL_IF(!slot_record || slot_record->get_tag() != U16(Tag::Slot));
    Reader slot(slot_record->get_payload());
    auto name = slot.read_bytes();
    auto attributes = slot.read_attributes(arena);
    auto reference = slot.read_type_reference(arena);
    BAIL_IF(
        !name || !attributes || !reference || !slot.is_complete() ||
        !Render::Language::Attributes::accepts(
            *attributes, Render::Language::Attributes::Placement::StageEntry));
    slots.insert(
        Render::Language::Layout::Slot(
            *reference, arena.proxy(*name), *attributes,
            Ttx::Lexical::Anchor::create(Ttx::Lexical::Span())));
  }
  BAIL_IF(!contents.is_complete());
  return Render::Language::Layout::create(
      arena, slots, Ttx::Lexical::Anchor::create(Ttx::Lexical::Span()),
      *parameters == 1);
}

auto Render::Archive::Reader::read_entry(
    Allocator::Arena& arena,
    Abstract& host) -> Option<Entry> {
  Reader probe = *this;
  auto record = probe.read_record();
  BAIL_IF(!record);
  Tag tag = Tag(record->get_tag());

  auto selected = read_record();
  BAIL_IF(!selected);
  Reader contents(selected->get_payload());
  auto definition = contents.read_definition(arena);
  BAIL_IF(!definition);
  auto& restored_definition = definition->create(arena, host);

  // Each record constructs its real Render owner immediately. The temporary
  // Definition value exists only long enough to select that semantic identity.
  if (tag == Tag::Alias) {
    auto reference = contents.read_type_reference(arena);
    BAIL_IF(!reference || !contents.is_complete());
    auto& alias =
        Render::Language::Alias::create(arena, restored_definition, *reference);
    return Entry(alias, Category::Type, definition->get_visibility(), False);
  }
  if (tag == Tag::Binding) {
    auto kind = contents.read_u8();
    auto access = contents.read_u8();
    auto reference = contents.read_type_reference(arena);
    BAIL_IF(
        !kind || *kind > U8(Render::Language::Binding::Kind::Resource) ||
        *kind == U8(Render::Language::Binding::Kind::Parameter) || !access ||
        *access > U8(Render::Language::Binding::Access::ReadWrite) ||
        (*kind == U8(Render::Language::Binding::Kind::Resource)) !=
            (*access != U8(Render::Language::Binding::Access::None)) ||
        !reference || !contents.is_complete());
    auto selected_kind = Render::Language::Binding::Kind(*kind);
    auto placement = Render::Language::Attributes::Placement::Value;
    if (selected_kind == Render::Language::Binding::Kind::Push) {
      placement = Render::Language::Attributes::Placement::Push;
    } else if (selected_kind == Render::Language::Binding::Kind::Resource) {
      placement = Render::Language::Attributes::Placement::Resource;
    }
    BAIL_IF(!Render::Language::Attributes::accepts(
        definition->get_attributes(), placement));
    auto& binding = Render::Language::Binding::create_authored(
        arena, restored_definition, selected_kind, *reference,
        Render::Language::Binding::Access(*access));
    return Entry(
        binding, Category::Addressable, definition->get_visibility(),
        selected_kind == Render::Language::Binding::Kind::Value);
  }
  if (tag == Tag::Stage) {
    auto parameters = contents.read_layout(arena);
    auto results = contents.read_layout(arena);
    BAIL_IF(
        !parameters || !results || !parameters->contains_parameters() ||
        results->contains_parameters() || !contents.is_complete() ||
        !Render::Language::Attributes::accepts(
            definition->get_attributes(),
            Render::Language::Attributes::Placement::Stage));
    auto& stage = Render::Language::Stage::create(
        arena, restored_definition, *parameters, *results);
    return Entry(
        stage, Category::Callable, definition->get_visibility(), False);
  }
  if (tag == Tag::Structure) {
    BAIL_IF(!Render::Language::Attributes::accepts(
        definition->get_attributes(),
        Render::Language::Attributes::Placement::Structure));
    auto& structure =
        Render::Language::Structure::create(arena, restored_definition);
    BAIL_IF(
        !contents.read_entries(arena, structure) || !contents.is_complete());
    return Entry(
        structure, Category::Type, definition->get_visibility(), False);
  }
  return {};
}

auto Render::Archive::Reader::read_entries(
    Allocator::Arena& arena,
    Render::Language::Monograph& monograph) -> Bool {
  while (!is_complete()) {
    auto entry = read_entry(arena, monograph);
    BAIL_IF(!entry || entry->instance);
    Bool retained = False;
    switch (entry->category) {
    case Category::Addressable:
      retained =
          monograph.retain_addressable(entry->semantic, entry->visibility);
      break;
    case Category::Callable:
      retained = monograph.retain_callable(entry->semantic, entry->visibility);
      break;
    case Category::Type:
      retained = monograph.retain_type(entry->semantic, entry->visibility);
      break;
    }
    BAIL_IF(!retained);
  }
  return True;
}

auto Render::Archive::Reader::read_entries(
    Allocator::Arena& arena,
    Render::Language::Structure& structure) -> Bool {
  while (!is_complete()) {
    auto entry = read_entry(arena, structure);
    BAIL_IF(!entry);
    Bool retained = False;
    switch (entry->category) {
    case Category::Addressable:
      retained =
          structure.retain_addressable(entry->semantic, entry->visibility);
      break;
    case Category::Callable:
      retained = structure.retain_callable(entry->semantic, entry->visibility);
      break;
    case Category::Type:
      retained = structure.retain_type(entry->semantic, entry->visibility);
      break;
    }
    BAIL_IF(!retained);
    if (entry->instance) {
      auto addressable = entry->semantic.select<Ttx::Model::Addressable>();
      BAIL_IF(!addressable);
      structure.retain_instance(*addressable);
    }
  }
  return True;
}
