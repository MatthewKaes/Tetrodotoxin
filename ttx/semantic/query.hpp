// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/system/uuid.hpp"

#include "perimortem/utility/result.hpp"

#include "ttx/semantic/binding.hpp"
#include "ttx/semantic/query.h"

namespace Ttx::Semantic {

// An enclosing protocol supplies the first Query. A loaded module can return
// it from an agreed entry point, while a native owner can lend its bind thunk
// directly. That initial agreement makes UUID negotiation possible without
// requiring either participant to construct an Abstract graph. This entry is
// a prearranged directly accessible thunk table with an enclosing lifetime.
// It therefore does not need to negotiate a transport to invoke bind itself.
// Later contracts can use Data access to acquire or populate their own tables.
//
// Once acquired, Query asks for one contract and returns its bound operations.
// There is no additional permission handshake here. The supplying state and
// code remain borrowed through negotiation and every use of the returned view.
class Query {
 public:
  constexpr Query() = default;

  constexpr explicit Query(ttx_semantic_query value) : value(value) {}

  constexpr operator ttx_semantic_query() const { return value; }

  constexpr auto is_set() const -> Bool { return value.bind != nullptr; }

  auto bind(Perimortem::System::Uuid contract) const
      -> Perimortem::Utility::Result<Binding, Binding::Failure>;

  template <typename Contract>
  auto bind() const -> Perimortem::Utility::
      Result<typename Contract::Handle, Binding::Failure> {
    using Result = Perimortem::Utility::Result<
        typename Contract::Handle, Binding::Failure>;
    return bind(Contract::contract_id)
        .visit(
            [](const Binding& result) -> Result {
              return result.get<Contract>();
            },
            [](Binding::Failure failure) -> Result { return failure; });
  }

 private:
  ttx_semantic_query value = {};
};

}  // namespace Ttx::Semantic
