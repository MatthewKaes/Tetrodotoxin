// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/protocol/block.h"
#include "ttx/data/form/representation.hpp"
#include "ttx/data/form/storage.hpp"

namespace Ttx::Data::Protocol {

// A provider can produce a complete representation without lending its own
// storage. Block lets the reader supply that storage for a synchronous call.
// The returned status determines whether the whole result is available, and
// the provider has finished using the surface before control returns.
class Block {
 public:
  using Surface = ttx_block_surface;
  // View turns an operation's destination Storage into the surface it will
  // read. Supplying that surface per call permits independently owned results.
  class View {
   public:
    using Operations = ttx_block_view_operations;

    constexpr View(const void* source, const Operations& operations)
        : value{source, &operations} {}

    constexpr explicit View(ttx_block_view value) : value(value) {}

    auto get_representation() const -> const Form::Representation& {
      return *value.operations->representation(value.source);
    }

    constexpr auto get_abi() const -> ttx_block_view { return value; }

    auto surface(Form::Storage target) const -> Surface {
      return value.operations->surface(value.source, target.get_abi());
    }

   private:
    ttx_block_view value;
  };

  // Access commits a complete representation into the supplied surface. It
  // exposes no provider storage and retains no operation borrow after
  // returning.
  class Access {
   public:
    using Operations = ttx_block_access_operations;

    constexpr Access(const void* source, const Operations& operations)
        : value{source, &operations} {}

    constexpr explicit Access(ttx_block_access value) : value(value) {}

    auto get_representation() const -> const Form::Representation& {
      return *value.operations->representation(value.source);
    }

    constexpr auto get_abi() const -> ttx_block_access { return value; }

    auto commit(Surface surface) const -> Status {
      return static_cast<Status>(
          value.operations->commit(value.source, surface));
    }

   private:
    ttx_block_access value;
  };
};
}  // namespace Ttx::Data::Protocol
