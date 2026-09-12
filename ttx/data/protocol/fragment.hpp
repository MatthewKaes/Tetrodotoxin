// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/protocol/fragment.h"
#include "ttx/data/form/representation.hpp"

namespace Ttx::Data::Protocol {

// Fragment supplies individual observations without lending a backing record.
// The native facade returns a value or failure from each synchronous call,
// keeping the C output parameter inside the implementation boundary.
class Fragment {
 public:
  // View accepts the primitive observations described by its representation. It
  // needs no destination storage to establish that requirement with a provider.
  class View {
   public:
    using Operations = ttx_fragment_view_operations;

    constexpr View(const void* source, const Operations& operations)
        : value{source, &operations} {}

    constexpr explicit View(ttx_fragment_view value) : value(value) {}

    auto get_representation() const -> const Form::Representation& {
      return *value.operations->representation(value.source);
    }

    constexpr auto get_abi() const -> ttx_fragment_view { return value; }

   private:
    ttx_fragment_view value;
  };

  // Access finishes each observation before returning. A provider may compute
  // the value during that call, but it cannot retain an output for later work.
  class Access {
   public:
    using Operations = ttx_fragment_access_operations;

    constexpr Access(const void* source, const Operations& operations)
        : value{source, &operations} {}

    constexpr explicit Access(ttx_fragment_access value) : value(value) {}

    auto get_representation() const -> const Form::Representation& {
      return *value.operations->representation(value.source);
    }

    constexpr auto get_abi() const -> ttx_fragment_access { return value; }

    auto get_u8(Count position) const
        -> Perimortem::Utility::Result<U8, Status>;

    auto get_u16(Count position) const
        -> Perimortem::Utility::Result<U16, Status>;

    auto get_u32(Count position) const
        -> Perimortem::Utility::Result<U32, Status>;

    auto get_u64(Count position) const
        -> Perimortem::Utility::Result<U64, Status>;

    auto get_s8(Count position) const
        -> Perimortem::Utility::Result<S8, Status>;

    auto get_s16(Count position) const
        -> Perimortem::Utility::Result<S16, Status>;

    auto get_s32(Count position) const
        -> Perimortem::Utility::Result<S32, Status>;

    auto get_s64(Count position) const
        -> Perimortem::Utility::Result<S64, Status>;

    auto get_r32(Count position) const
        -> Perimortem::Utility::Result<R32, Status>;

    auto get_r64(Count position) const
        -> Perimortem::Utility::Result<R64, Status>;

    auto get_pointer(Count position) const
        -> Perimortem::Utility::Result<void*, Status>;

   private:
    ttx_fragment_access value;
  };
};

}  // namespace Ttx::Data::Protocol
