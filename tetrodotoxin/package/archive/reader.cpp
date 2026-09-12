// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/package/archive/reader.hpp"

#include "perimortem/core/static/vector.hpp"
#include "perimortem/core/diagnostics/log.hpp"
#include "perimortem/core/reader/binary.hpp"

#include "perimortem/memory/dynamic/vector.hpp"
#include "perimortem/memory/managed/vector.hpp"

#include "perimortem/system/path.hpp"

#include "ttx/lexical/lexicon.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Perimortem::System;
using namespace Perimortem::Utility;
using namespace Ttx::Lexical;
using namespace Tetrodotoxin;

static constexpr Static::Vector<Code::Type, 1> semantic_separators = {{
  Code::Type::TypeAccessOp,
}};
static constexpr Static::Vector<Code::Type, 1> package_separators = {{
  Code::Type::AddressOp,
}};

static auto read_bytes(
    Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>& reader,
    View::Bytes& value) -> Bool {
  auto size = reader.read_u32();
  BAIL_IF(!size);
  auto bytes = reader.read_bytes(*size);
  BAIL_IF(!bytes);
  value = *bytes;
  return True;
}

template <typename Value>
static auto count_fits(
    U32 count,
    const Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>& reader,
    Count minimum_size) -> Bool {
  BAIL_IF(reader.get_location() > reader.get_size() || minimum_size == 0);
  Count remaining = reader.get_size() - reader.get_location();
  return Count(count) <= remaining / minimum_size &&
         Count(count) <= Count(-1) / sizeof(Value);
}

static auto read_members(
    Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>& reader,
    Dynamic::Vector<Package::Archive::Member>& members) -> Bool {
  auto count = reader.read_u32();
  BAIL_IF(!count || !count_fits<Package::Archive::Member>(*count, reader, 12));
  members = Dynamic::Vector<Package::Archive::Member>(*count);
  for (U32 index = 0; index < *count; index++) {
    View::Bytes name;
    View::Bytes dialect;
    View::Bytes payload;
    BAIL_IF(
        !read_bytes(reader, name) || !read_bytes(reader, dialect) ||
        !read_bytes(reader, payload));
    members.emplace(Package::Archive::Member(name, dialect, payload));
  }
  return True;
}

static auto read_resources(
    Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>& reader,
    Dynamic::Vector<Package::Archive::Resource>& resources) -> Bool {
  auto count = reader.read_u32();
  BAIL_IF(!count || !count_fits<Package::Archive::Resource>(*count, reader, 8));
  resources = Dynamic::Vector<Package::Archive::Resource>(*count);
  for (U32 index = 0; index < *count; index++) {
    View::Bytes route;
    View::Bytes value;
    BAIL_IF(!read_bytes(reader, route) || !read_bytes(reader, value));
    resources.emplace(Package::Archive::Resource(route, value));
  }
  return True;
}

static auto read_imports(
    Perimortem::Core::Reader::Binary<Data::ByteOrder::Little>& reader,
    Dynamic::Vector<Package::Archive::GraphImport>& imports) -> Bool {
  auto count = reader.read_u32();
  BAIL_IF(
      !count || !count_fits<Package::Archive::GraphImport>(*count, reader, 22));
  imports = Dynamic::Vector<Package::Archive::GraphImport>(*count);
  for (U32 index = 0; index < *count; index++) {
    auto kind = reader.read_u8();
    auto visibility = reader.read_u8();
    View::Bytes importer;
    View::Bytes local_name;
    View::Bytes target;
    BAIL_IF(
        !kind || !visibility ||
        *kind > U8(Tetrodotoxin::Language::Import::Kind::Package) ||
        *visibility > U8(Tetrodotoxin::Language::Visibility::Public) ||
        !read_bytes(reader, importer) || !read_bytes(reader, local_name) ||
        !read_bytes(reader, target));
    auto major = reader.read_u16();
    auto minor = reader.read_u16();
    View::Bytes route;
    BAIL_IF(!major || !minor || !read_bytes(reader, route));
    imports.emplace(
        Package::Archive::GraphImport(
            importer, local_name,
            Tetrodotoxin::Language::Visibility(*visibility),
            Tetrodotoxin::Language::Import::Kind(*kind), target,
            Version(*major, *minor), route));
  }
  return True;
}

static auto duplicate_member(
    View::Vector<Package::Archive::Member> members,
    Count index) -> Bool {
  for (Count prior = 0; prior < index; prior++) {
    if (members.get_data()[prior].get_semantic_name() ==
        members.get_data()[index].get_semantic_name()) {
      return True;
    }
  }
  return False;
}

static auto has_member(
    View::Vector<Package::Archive::Member> members,
    View::Bytes name) -> Bool {
  return members.contains([&](const Package::Archive::Member& member) {
    return member.get_semantic_name() == name;
  });
}

static auto validate_members(View::Vector<Package::Archive::Member> members)
    -> Bool {
  BAIL_IF(members.is_empty());
  for (Count index = 0; index < members.get_size(); index++) {
    const Package::Archive::Member& member = members.get_data()[index];
    BAIL_IF(
        !Lexicon::validate(
            Code::Type::Type, member.get_semantic_name(),
            semantic_separators) ||
        !Lexicon::validate(Code::Type::Type, member.get_dialect_name()) ||
        duplicate_member(members, index));
  }
  return has_member(members, "PackageSurface"_view);
}

static auto validate_resources(
    View::Vector<Package::Archive::Resource> resources) -> Bool {
  for (Count index = 0; index < resources.get_size(); index++) {
    View::Bytes route = resources.get_data()[index].get_route();
    Path normalized(route);
    BAIL_IF(
        route.is_empty() || normalized.is_rooted() ||
        normalized.get_view() != route);
    for (Count prior = 0; prior < index; prior++) {
      BAIL_IF(resources.get_data()[prior].get_route() == route);
    }
  }
  return True;
}

static auto validate_imports(
    View::Vector<Package::Archive::Member> members,
    View::Vector<Package::Archive::GraphImport> imports) -> Bool {
  for (Count index = 0; index < imports.get_size(); index++) {
    const Package::Archive::GraphImport& import = imports.get_data()[index];
    BAIL_IF(
        !Lexicon::validate(
            Code::Type::Type, import.get_importer(), semantic_separators) ||
        !Lexicon::validate(Code::Type::Type, import.get_local_name()) ||
        import.get_target().is_empty() ||
        (!import.get_route().is_empty() &&
         !Lexicon::validate(
             Code::Type::Type, import.get_route(), semantic_separators)) ||
        !has_member(members, import.get_importer()));

    Bool package =
        import.get_kind() == Tetrodotoxin::Language::Import::Kind::Package;
    BAIL_IF(package == import.get_version().is_null());
    if (package) {
      BAIL_IF(!Lexicon::validate(
          Code::Type::Type, import.get_target(), package_separators));
    } else {
      BAIL_IF(
          import.get_target() == "PackageSurface"_view ||
          !has_member(members, import.get_target()));
    }

    for (Count prior = 0; prior < index; prior++) {
      BAIL_IF(
          imports.get_data()[prior].get_importer() == import.get_importer() &&
          imports.get_data()[prior].get_local_name() ==
              import.get_local_name());
    }
  }
  return True;
}

template <typename Value>
static auto retain(Allocator::Arena& arena, View::Vector<Value> values)
    -> View::Vector<Value> {
  Managed::Vector<Value> retained(arena);
  if (values.get_size() > retained.get_capacity()) {
    retained.reset(values.get_size());
  }
  for (const Value& value : values) {
    retained.insert(value);
  }
  return retained.get_view();
}

static auto reject(View::Bytes reason)
    -> Result<Package::Archive::Archive, Package::Archive::Reader::Error> {
  Diagnostics::Log::Message<256> message(Diagnostics::Log::Level::Debug);
  message << "Package Archive rejected: "_view << reason;
  return Package::Archive::Reader::Error::InvalidFormat;
}

auto Package::Archive::Reader::read(Allocator::Arena& arena, View::Bytes input)
    -> Result<Archive, Error> {
  Perimortem::Core::Reader::Binary<Data::ByteOrder::Little> header(input);
  auto magic = header.read_bytes(4);
  auto selected_format = header.read_u16();
  auto flags = header.read_u16();
  auto body_size = header.read_u32();
  if (!magic || !selected_format || !flags || !body_size ||
      *magic != "TTXA"_view) {
    return reject("invalid header"_view);
  }
  if (*selected_format != Archive::format) {
    return Error::UnsupportedFormat;
  }
  if (*flags != 0 || header.get_location() != Archive::header_size ||
      Count(*body_size) != input.get_size() - Archive::header_size) {
    return reject("invalid body boundary"_view);
  }

  Perimortem::Core::Reader::Binary<Data::ByteOrder::Little> body(
      input.slice(Archive::header_size));
  View::Bytes identity;
  if (!read_bytes(body, identity)) {
    return reject("invalid identity"_view);
  }
  auto major = body.read_u16();
  auto minor = body.read_u16();
  if (!major || !minor) {
    return reject("invalid graph records"_view);
  }

  Version version(*major, *minor);
  Dynamic::Vector<Package::Archive::Member> members;
  Dynamic::Vector<Package::Archive::Resource> resources;
  Dynamic::Vector<Package::Archive::GraphImport> imports;
  if (!read_members(body, members) || !read_resources(body, resources) ||
      !read_imports(body, imports) || body.get_location() != body.get_size()) {
    return reject("invalid graph records"_view);
  }
  if (!Lexicon::validate(Code::Type::Type, identity, package_separators) ||
      version.is_null() || !validate_members(members.get_view()) ||
      !validate_resources(resources.get_view()) ||
      !validate_imports(members.get_view(), imports.get_view())) {
    return reject("invalid graph facts"_view);
  }

  return Archive(
      identity, version, retain(arena, members.get_view()),
      retain(arena, resources.get_view()), retain(arena, imports.get_view()));
}
