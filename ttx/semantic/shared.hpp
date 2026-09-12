// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/system/uuid.hpp"

#include "ttx/data/protocol/shared.hpp"
#include "ttx/semantic/shared.h"

namespace Ttx::Semantic {

// Shared's identities let a reader explicitly accept an acquired lifetime and
// a provider synchronously supply that agreement. Keeping these roles distinct
// from Direct means Flow can retain the release obligation instead of treating
// a temporary publication as an address that needs no lifetime negotiation.
class Shared {
 public:
  // Binding View permits Flow to hold an acquired representation for the
  // reader. The descriptor describes what that held representation must satisfy.
  struct View {
    static constexpr Perimortem::System::Uuid contract_id{
      TTX_SHARED_VIEW_ID_HIGH,
      TTX_SHARED_VIEW_ID_LOW,
    };

    using Operations = ttx_shared_view_operations;
    using Handle = Data::Protocol::Shared::View;
  };

  // Binding Access supplies acquisition and its release obligation. Flow can
  // retain the resulting lifetime across operations instead of acquiring again.
  struct Access {
    static constexpr Perimortem::System::Uuid contract_id{
      TTX_SHARED_ACCESS_ID_HIGH,
      TTX_SHARED_ACCESS_ID_LOW,
    };

    using Operations = ttx_shared_access_operations;
    using Handle = Data::Protocol::Shared::Access;
  };
};
}  // namespace Ttx::Semantic
