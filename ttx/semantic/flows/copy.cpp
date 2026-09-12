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

// Fragmented memory access requires us to navigate transfer element by element
// which is very slow but gives the maximum power to the provider of the data to
// decide how that data is materialized.
//
// All the copy operator does is propagate each element of the representation in
// order to each side of the API. The operator doesn't even observe the
// transaction so it can't guarantee a success means any real data was even
// "stored".
static auto copy_fragments(Fragment::Access source, Storage target)
    -> Copy::Result {
  Count offset = 0;
  Representation::Position position;
  while (ttx_representation_next(
             &target.get_representation(), offset, &position) !=
         TTX_DATA_BOUNDS) {
    const auto status = Operations::Fragment::read(
        source, *position.representation, position.offset, [&](auto value) {
          Operations::Fragment::put(target, position, value);
        });
    if (status != Status::Success) {
      return status;
    }

    offset = position.offset + position.representation->extent;
  }

  return Status::Success;
}

auto Copy::flow(const Flow& flow, Storage target) -> Result {
  // A new target may have an independent descriptor so while Storage admission
  // already established capacity we still need a single compatability check
  // to make sure a bad target slipped.
  if (!flow.get_representation().compatible(target.get_representation())) {
    return Status::Incompatible;
  }

  // When possible we can just use a memmove when we have direct access to the
  // representation that matches our wire format.
  auto memory = [&](const void* source) -> Result {
    const Count extent = target.get_representation().extent;
    if (extent) {
      memmove(target.get_bytes().get_data(), source, extent);
    }

    return Status::Success;
  };

  return flow.visit(
      memory, memory,
      // Block transfers leave it up to the writer to decide how to handle the
      // memory transfer. It can still choose to use a memmove, but even if it
      // does we still have the overhead of the commit handshake.
      [&](Block::View reader, Block::Access writer) -> Result {
        return writer.commit(reader.surface(target));
      },
      // Fragmented access is such a weak promise that the writer can't even
      // upgrade the interaction to memmove if it wanted. For copies this should
      // be the method of last resort as the interaction itself isn't even
      // required to be "atomic" which can cause artifacts that look like bugs
      // when the source and destination overlap.
      [&](Fragment::Access source) { return copy_fragments(source, target); });
}

auto ttx_copy(const ttx_flow* flow, ttx_storage target) -> ttx_copy_result {
  return {static_cast<ttx_data_status>(
      Copy::flow(*reinterpret_cast<const Flow*>(flow), Storage(target)))};
}
