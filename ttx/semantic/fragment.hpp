// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/system/uuid.hpp"

#include "ttx/data/protocol/fragment.hpp"
#include "ttx/semantic/fragment.h"

namespace Ttx::Semantic {

// Fragment negotiation permits a representation to be assembled from separate
// observations. These identities connect a reader accepting that access model
// with the provider's typed getters. They establish neither a whole snapshot
// nor a block that could be obtained by casting the bound source state.
class Fragment {
 public:
  // Binding View accepts separate typed observations in the required schema.
  // Any stronger consistency between observations belongs to the reader policy.
  struct View {
    static constexpr Perimortem::System::Uuid contract_id{
      TTX_FRAGMENT_VIEW_ID_HIGH,
      TTX_FRAGMENT_VIEW_ID_LOW,
    };

    using Operations = ttx_fragment_view_operations;
    using Handle = Data::Protocol::Fragment::View;
  };

  // Binding Access supplies only the typed getters that its schema describes.
  // A representation assembled by observing them need not exist in the
  // provider.
  struct Access {
    static constexpr Perimortem::System::Uuid contract_id{
      TTX_FRAGMENT_ACCESS_ID_HIGH,
      TTX_FRAGMENT_ACCESS_ID_LOW,
    };

    using Operations = ttx_fragment_access_operations;
    using Handle = Data::Protocol::Fragment::Access;
  };
};
}  // namespace Ttx::Semantic
