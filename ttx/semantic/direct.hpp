// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/system/uuid.hpp"

#include "ttx/data/protocol/direct.hpp"
#include "ttx/semantic/direct.h"

namespace Ttx::Semantic {

// These identities let independently loaded readers and providers agree on
// Direct's C operation tables without sharing C++ type identities. The payload
// schemas still establish which representation the returned pointer exposes.
// A successful bind grants that role alone, not another transport inferred from
// the fact that native code could read the published bytes.
class Direct {
 public:
  // Binding View declares that the reader accepts a directly published ABI.
  // Its schema remains the requirement even when no target Storage exists yet.
  struct View {
    static constexpr Perimortem::System::Uuid contract_id{
      TTX_DIRECT_VIEW_ID_HIGH,
      TTX_DIRECT_VIEW_ID_LOW,
    };

    using Operations = ttx_direct_view_operations;
    using Handle = Data::Protocol::Direct::View;
  };

  // Binding Access grants the public pointer under its publication lifetime.
  // Agreement on this identity does not grant any other transport capability.
  struct Access {
    static constexpr Perimortem::System::Uuid contract_id{
      TTX_DIRECT_ACCESS_ID_HIGH,
      TTX_DIRECT_ACCESS_ID_LOW,
    };

    using Operations = ttx_direct_access_operations;
    using Handle = Data::Protocol::Direct::Access;
  };
};
}  // namespace Ttx::Semantic
