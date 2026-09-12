// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/scene/archive/reader.hpp"

#include "perimortem/core/reader/binary.hpp"

#include "tetrodotoxin/language/definition.hpp"
#include "tetrodotoxin/language/visibility.hpp"
#include "tetrodotoxin/library/language/function.hpp"
#include "tetrodotoxin/library/language/types/object.hpp"
#include "tetrodotoxin/scene/language/signal.hpp"
#include "ttx/lexical/anchor.hpp"
#include "ttx/model/documentations/block.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Ttx::Concept;
using namespace Tetrodotoxin;

auto Scene::Archive::Reader::open(View::Bytes payload) -> Option<Reader> {
  BAIL_IF(payload.get_size() < 8);
  Perimortem::Core::Reader::Binary<Data::ByteOrder::Little> reader(
      payload.slice(0, 8));
  auto magic = reader.read_bytes(4);
  auto version = reader.read_u16();
  auto flags = reader.read_u16();
  BAIL_IF(!magic || !version || !flags);
  BAIL_IF(*magic != "TTSC"_view || *version != 3 || *flags != 0);
  return Reader(payload.slice(8));
}

auto Scene::Archive::Reader::restore(
    Allocator::Arena& arena,
    View::Bytes payload,
    const Abstract& language,
    const Library::Dialect& library,
    Abstract& context) -> Option<Ttx::Concept::Abstract&> {
  // Scene restoration requires the deferred Package projection decoder.
  return {};
}

auto Scene::Archive::Reader::take(Count size) -> Option<View::Bytes> {
  BAIL_IF(
      location > payload.get_size() || size > payload.get_size() - location);
  View::Bytes selected = payload.slice(location, size);
  location += size;
  return selected;
}

auto Scene::Archive::Reader::read_u8() -> Option<U8> {
  auto selected = take(sizeof(U8));
  BAIL_IF(!selected);
  return Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>(*selected)
      .read_u8();
}

auto Scene::Archive::Reader::read_u32() -> Option<U32> {
  auto selected = take(sizeof(U32));
  BAIL_IF(!selected);
  return Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>(*selected)
      .read_u32();
}

auto Scene::Archive::Reader::read_bytes() -> Option<View::Bytes> {
  auto size = read_u32();
  BAIL_IF(!size);
  return take(*size);
}

auto Scene::Archive::Reader::read_documentation(Allocator::Arena& arena)
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
