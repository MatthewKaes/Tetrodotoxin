// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/protocol/shared.h"
#include "ttx/data/form/representation.hpp"

namespace Ttx::Data::Protocol {

// A provider may need to hold a resource before it can lend a representation.
// Shared carries that lifetime with the pointer, so the reader can keep using
// the same representation until it releases the agreement.
class Shared {
 public:
  // A successful acquisition transfers one release obligation with its data.
  // Moving the lifetime transfers that obligation rather than acquiring again.
  class Lifetime {
   public:
    constexpr Lifetime() : value{} {}

    constexpr explicit Lifetime(ttx_shared_lifetime value) : value(value) {}

    Lifetime(const Lifetime&) = delete;

    auto operator=(const Lifetime&) -> Lifetime& = delete;

    constexpr Lifetime(Lifetime&& other) : value(other.take_abi()) {}

    auto operator=(Lifetime&& other) -> Lifetime& {
      if (this != &other) {
        ttx_shared_release(&value);
        value = other.take_abi();
      }

      return *this;
    }

    ~Lifetime() { ttx_shared_release(&value); }

    auto get_pointer() const -> const void* { return value.data; }

    auto take_abi() -> ttx_shared_lifetime {
      auto result = value;
      value = {};
      return result;
    }

   private:
    ttx_shared_lifetime value = {};
  };

  // View accepts a representation held through an acquired lifetime. It names
  // the required representation without deciding where later operations store
  // results.
  class View {
   public:
    using Operations = ttx_shared_view_operations;

    constexpr View(const void* source, const Operations& operations)
        : value{source, &operations} {}

    constexpr explicit View(ttx_shared_view value) : value(value) {}

    auto get_representation() const -> const Form::Representation& {
      return *value.operations->representation(value.source);
    }

    constexpr auto get_abi() const -> ttx_shared_view { return value; }

   private:
    ttx_shared_view value;
  };

  // Access supplies the representation together with its lifetime agreement.
  // Successful acquisition returns a ready view with its release obligation.
  class Access {
   public:
    using Operations = ttx_shared_access_operations;

    constexpr Access(const void* source, const Operations& operations)
        : value{source, &operations} {}

    constexpr explicit Access(ttx_shared_access value) : value(value) {}

    auto get_representation() const -> const Form::Representation& {
      return *value.operations->representation(value.source);
    }

    constexpr auto get_abi() const -> ttx_shared_access { return value; }

    auto acquire() const -> Perimortem::Utility::Result<Lifetime, Status> {
      ttx_shared_lifetime result;
      const auto status = value.operations->acquire(value.source, &result);
      if (status != TTX_DATA_SUCCESS) {
        return static_cast<Status>(status);
      }

      return Lifetime(result);
    }

   private:
    ttx_shared_access value;
  };
};
}  // namespace Ttx::Data::Protocol
