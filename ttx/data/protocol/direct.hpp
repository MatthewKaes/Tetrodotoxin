// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/protocol/direct.h"
#include "ttx/data/form/representation.hpp"

namespace Ttx::Data::Protocol {

// A publication may already expose the representation its reader needs.
// Direct lends that address without acquiring another lifetime, leaving the
// publisher responsible for keeping the representation valid for its readers.
class Direct {
 public:
  // View accepts a public representation with a stable publication lifetime.
  // It supplies the required representation without owning a destination
  // Storage.
  class View {
   public:
    using Operations = ttx_direct_view_operations;

    constexpr View(const void* source, const Operations& operations)
        : value{source, &operations} {}

    constexpr explicit View(ttx_direct_view value) : value(value) {}

    auto get_representation() const -> const Form::Representation& {
      return *value.operations->representation(value.source);
    }

    constexpr auto get_abi() const -> ttx_direct_view { return value; }

   private:
    ttx_direct_view value;
  };

  // Access lends the agreed representation itself. The publication keeps the
  // pointer valid, so consuming it requires no acquire or release operation.
  class Access {
   public:
    using Operations = ttx_direct_access_operations;

    constexpr Access(const void* source, const Operations& operations)
        : value{source, &operations} {}

    constexpr explicit Access(ttx_direct_access value) : value(value) {}

    auto get_representation() const -> const Form::Representation& {
      return *value.operations->representation(value.source);
    }

    constexpr auto get_abi() const -> ttx_direct_access { return value; }

    auto read_ptr() const -> const void* {
      return value.operations->read_ptr(value.source);
    }

   private:
    ttx_direct_access value;
  };
};
}  // namespace Ttx::Data::Protocol
