// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/system/uuid.hpp"

#include "ttx/data/protocol/block.hpp"
#include "ttx/semantic/block.h"

namespace Ttx::Semantic {

// Block negotiation asks whether a reader can supply a complete surface and a
// provider can populate it. These identities name those two Data roles so the
// agreement requires neither a provider pointer nor support for individual
// field reads. A caller needing either must negotiate that separate contract.
class Block {
 public:
  // Binding View supplies the reader's way to expose a target surface. The
  // surface is requested per call so independently owned results stay separate.
  struct View {
    static constexpr Perimortem::System::Uuid contract_id{
      TTX_BLOCK_VIEW_ID_HIGH,
      TTX_BLOCK_VIEW_ID_LOW,
    };

    using Operations = ttx_block_view_operations;
    using Handle = Data::Protocol::Block::View;
  };

  // Binding Access allows the provider to fill a complete surface on request.
  // Its returned status reports the result without lending its own data
  // pointer.
  struct Access {
    static constexpr Perimortem::System::Uuid contract_id{
      TTX_BLOCK_ACCESS_ID_HIGH,
      TTX_BLOCK_ACCESS_ID_LOW,
    };

    using Operations = ttx_block_access_operations;
    using Handle = Data::Protocol::Block::Access;
  };
};
}  // namespace Ttx::Semantic
