// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/access/bytes.hpp"

#include "ttx/data/form/representation.hpp"
#include "ttx/data/form/storage.h"

namespace Ttx::Data::Form {

// Storage gives an operation a concrete destination without deciding who
// allocates it. Construction checks the region against an admitted
// representation, then the descriptor can be borrowed without repeating those
// storage checks. Copies still refer to the same region, so its owner keeps
// both the bytes and representation alive until every operation using them has
// completed.
class Storage {
 public:
  static auto create(
      const Representation& representation,
      Perimortem::Core::Access::Bytes bytes)
      -> Perimortem::Utility::Result<Storage, Status> {
    const ttx_storage value{
      &representation, bytes.get_data(), bytes.get_size()};

    const auto status = ttx_storage_check(value);
    if (status) {
      return static_cast<Status>(status);
    }

    return Storage(value);
  }

  // A foreign owner may already have admitted its region. This constructor
  // consumes that established precondition instead of checking it again.
  constexpr explicit Storage(ttx_storage value) : value(value) {}

  constexpr auto get_representation() const -> const Representation& {
    return *value.representation;
  }

  constexpr auto get_bytes() const -> Perimortem::Core::Access::Bytes {
    return {value.data, value.size};
  }

  constexpr auto get_abi() const -> ttx_storage { return value; }

 private:
  ttx_storage value;
};
}  // namespace Ttx::Data::Form
