// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/semantic/query.hpp"

using namespace Perimortem;
using namespace Ttx::Semantic;

auto Query::bind(System::Uuid contract) const
    -> Utility::Result<Binding, Binding::Failure> {
  if (!value.bind) {
    return Binding::Failure::Rejected;
  }

  // Every request starts with an empty answer. In particular, a successful
  // block negotiation cannot supply the table for a later value negotiation
  // when a malformed provider forgets to write its new result.
  ttx_binding result = {};
  const auto status = value.bind(value.source, contract, &result);
  switch (static_cast<Binding::Status>(status)) {
  case Binding::Status::Satisfied:
    if (result.operations) {
      return Binding(result);
    }

    return Binding::Failure::Rejected;
  case Binding::Status::Unsupported:
    return Binding::Failure::Unsupported;
  case Binding::Status::Pending:
    return Binding::Failure::Pending;
  default:
    return Binding::Failure::Rejected;
  }
}
