// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/semantic/flows/copy.hpp"

#include "perimortem/core/data.hpp"

#include "ttx/semantic/operations/fragment.hpp"

using namespace Ttx::Semantic;
using namespace Ttx::Semantic::Flows;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;
using Ttx::Data::Protocol::Block;
using Ttx::Data::Protocol::Fragment;

// Fragment lets the source produce each value when it is observed. Copy owns
// the other half of that operation: it writes the returned value into the
// caller's Storage before requesting another. The provider needs no backing
// record, while the caller obtains a concrete record satisfying the agreed
// form. Padding has a fixed location but no promised value, so these writes
// leave it alone.
static auto copy_fragments(Fragment::Access source, Storage target)
    -> Status {
  return target.get_representation().visit(
      [&](Representation::Position position) {
        return Operations::Fragment::read(
            source, position, position.offset, [&](auto value) {
              Operations::Fragment::put(target, position, value);
            });
      });
}

auto Copy::flow(const Flow& flow, Storage target) -> Status {
  // Each call can supply fresh Storage with an independent descriptor. Its
  // admission already proved capacity and alignment, but it still has to
  // describe the form this Flow agreed to observe.
  if (!flow.get_representation().compatible(target.get_representation())) {
    return Status::Incompatible;
  }

  // The agreed physical form lets Direct and Shared move the entire extent.
  // memmove also preserves source bytes when the two concrete regions overlap.
  auto memory = [&](const void* source) -> Status {
    const Count extent = target.get_representation().get_extent();
    if (extent) {
      memmove(target.get_bytes().get_data(), source, extent);
    }

    return Status::Success;
  };

  return flow.visit(
      memory, memory,
      // Block gives the provider this call's destination surface. The provider
      // finishes populating it before returning, while other calls can supply
      // independently owned surfaces through the same retained agreement.
      [&](Block::View reader, Block::Access writer) -> Status {
        return writer.commit(reader.surface(target));
      },
      // Fragment supplies typed observations. Copy cannot infer permission for
      // a whole byte transfer from those getters. Successive observations need
      // not form a snapshot, so overlap can affect the combined result.
      [&](Fragment::Access source) { return copy_fragments(source, target); });
}

auto ttx_copy(const ttx_flow* flow, ttx_storage target) -> ttx_data_status {
  return static_cast<ttx_data_status>(
      Copy::flow(*reinterpret_cast<const Flow*>(flow), Storage(target)));
}
