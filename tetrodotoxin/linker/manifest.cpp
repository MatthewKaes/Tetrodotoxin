// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/linker/manifest.hpp"

#include "perimortem/core/access/bytes.hpp"
#include "perimortem/core/data.hpp"
#include "perimortem/core/null_terminated.hpp"
#include "perimortem/core/reader/binary.hpp"
#include "perimortem/core/writer/binary.hpp"

#include "perimortem/memory/managed/vector.hpp"

using namespace Perimortem;
using namespace Tetrodotoxin;

using LittleReader = Core::Reader::Binary<Core::Data::ByteOrder::Little>;
using LittleWriter = Core::Writer::Binary<Core::Data::ByteOrder::Little>;

static constexpr Core::View::Bytes manifest_magic = "TTXABI01"_view;

static auto add_size(Count& total, Count value) -> Bool {
  if (value > Count(-1) - total) {
    return False;
  }
  total += value;
  return True;
}

static auto add_sized_bytes(Count& total, Core::View::Bytes value) -> Bool {
  return value.get_size() <= U32(-1) && add_size(total, sizeof(U32)) &&
         add_size(total, value.get_size());
}

static auto write_sized_bytes(LittleWriter& writer, Core::View::Bytes value)
    -> Bool {
  writer << U32(value.get_size()) << value;
  return writer.is_valid();
}

static auto read_sized_bytes(LittleReader& reader)
    -> Core::Option<Core::View::Bytes> {
  auto size = reader.read_u32();
  if (!size) {
    return {};
  }

  return reader.read_bytes(*size);
}

auto Linker::Manifest::write(const Manifest& manifest)
    -> Core::Option<Memory::Dynamic::Bytes> {
  if (manifest.get_identity().is_empty() || manifest.get_version().is_null() ||
      manifest.get_artifact().is_empty() || manifest.get_target().is_empty() ||
      manifest.get_imports().get_size() > U32(-1)) {
    return {};
  }

  Count size = manifest_magic.get_size();
  if (!add_sized_bytes(size, manifest.get_identity()) ||
      !add_size(size, sizeof(U16) * 2) ||
      !add_sized_bytes(size, manifest.get_artifact()) ||
      !add_sized_bytes(size, manifest.get_target()) ||
      !add_size(size, sizeof(U64) + sizeof(U32))) {
    return {};
  }

  for (const Linker::Import& import : manifest.get_imports()) {
    if (import.get_abi().is_empty() || import.get_symbol().is_empty() ||
        import.get_provider().is_empty() || !add_size(size, sizeof(U8)) ||
        !add_sized_bytes(size, import.get_abi()) ||
        !add_sized_bytes(size, import.get_symbol()) ||
        !add_sized_bytes(size, import.get_provider())) {
      return {};
    }
  }

  Memory::Dynamic::Bytes output;
  output.forgetful_resize(size);
  LittleWriter writer(output.get_access());
  writer << manifest_magic;
  if (!write_sized_bytes(writer, manifest.get_identity())) {
    return {};
  }
  writer << manifest.get_version().get_major()
         << manifest.get_version().get_minor();
  if (!write_sized_bytes(writer, manifest.get_artifact()) ||
      !write_sized_bytes(writer, manifest.get_target())) {
    return {};
  }
  writer << manifest.get_fingerprint().get_value()
         << U32(manifest.get_imports().get_size());
  for (const Linker::Import& import : manifest.get_imports()) {
    writer << U8(import.get_kind());
    if (!write_sized_bytes(writer, import.get_abi()) ||
        !write_sized_bytes(writer, import.get_symbol()) ||
        !write_sized_bytes(writer, import.get_provider())) {
      return {};
    }
  }
  return writer.is_valid() && writer.get_location() == size
             ? Core::Option<Memory::Dynamic::Bytes>(
                   static_cast<Memory::Dynamic::Bytes&&>(output))
             : Core::Option<Memory::Dynamic::Bytes>();
}

auto Linker::Manifest::read(
    Memory::Allocator::Arena& arena,
    Core::View::Bytes bytes) -> Utility::Result<Manifest, Error> {
  if (bytes.get_size() < manifest_magic.get_size()) {
    return Error::InvalidFormat;
  }
  Core::View::Bytes magic = bytes.slice(0, manifest_magic.get_size());
  if (magic != manifest_magic) {
    return magic.slice(0, 6) == manifest_magic.slice(0, 6)
               ? Error::UnsupportedFormat
               : Error::InvalidFormat;
  }

  LittleReader reader(bytes.slice(manifest_magic.get_size()));
  auto identity = read_sized_bytes(reader);
  auto major = reader.read_u16();
  auto minor = reader.read_u16();
  auto artifact = read_sized_bytes(reader);
  auto target = read_sized_bytes(reader);
  auto fingerprint = reader.read_u64();
  auto count = reader.read_u32();
  if (!major || !minor || !fingerprint || !count || !identity ||
      identity->is_empty() || (*major == 0 && *minor == 0) || !artifact ||
      artifact->is_empty() || !target || target->is_empty() ||
      Count(*count) > (reader.get_size() - reader.get_location()) /
                          (sizeof(U8) + sizeof(U32) * 3)) {
    return Error::InvalidFormat;
  }

  Memory::Managed::Vector<Linker::Import> imports(arena);
  for (U32 index = 0; index < *count; index++) {
    auto kind = reader.read_u8();
    auto abi = read_sized_bytes(reader);
    auto symbol = read_sized_bytes(reader);
    auto provider = read_sized_bytes(reader);
    if (!kind || *kind > U8(Linker::Import::Kind::WritableState) || !abi ||
        abi->is_empty() || !symbol || symbol->is_empty() || !provider ||
        provider->is_empty()) {
      return Error::InvalidFormat;
    }
    imports.insert(
        Linker::Import(Linker::Import::Kind(*kind), *abi, *symbol, *provider));
  }
  if (reader.get_location() != reader.get_size()) {
    return Error::InvalidFormat;
  }

  return Manifest(
      *identity, System::Version(*major, *minor), *artifact, *target,
      Linker::Fingerprint(*fingerprint), imports.get_view());
}
