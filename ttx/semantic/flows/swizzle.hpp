// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/semantic/flow.hpp"
#include "ttx/semantic/flows/swizzle.h"

namespace Ttx::Semantic::Flows {

// Swizzle applies a separate correspondence policy to an established Flow.
// The policy can repeat a color channel or rearrange a record while access
// remains unchanged. The call returns its final result and retains no request,
// target or callback. Deferred execution belongs to an outer operation policy.
class Swizzle {
 public:
  // Mapping admission asks whether every output can be supplied by its chosen
  // input. Keeping that proof separate lets an owner use the policy repeatedly
  // without paying for the same walk each time. Its representations and
  // coordinate function remain borrowed, immutable and deterministic through
  // those uses.
  class Mapping {
   public:
    static auto create(ttx_swizzle_mapping value)
        -> Perimortem::Utility::Result<Mapping, Data::Status> {
      const auto status = ttx_swizzle_mapping_check(value);
      if (status) {
        return static_cast<Data::Status>(status);
      }

      return Mapping(value);
    }

    template <typename Resolver>
    static auto create(
        const Data::Form::Representation& input,
        const Data::Form::Representation& output,
        Resolver& resolver)
        -> Perimortem::Utility::Result<Mapping, Data::Status> {
      return create(
          {&input, &output, &resolver,
           [](const void* source, Count position) -> Count {
             return (*static_cast<const Resolver*>(source))(position);
           }});
    }

    // A foreign provider or compiler may already establish these facts.
    // Construction from its carrier borrows that admitted immutable policy.
    constexpr explicit Mapping(ttx_swizzle_mapping value) : value(value) {}

    auto get_input() const -> const Data::Form::Representation& {
      return *value.input;
    }

    auto get_output() const -> const Data::Form::Representation& {
      return *value.output;
    }

    auto position(Count output) const -> Count {
      return value.position(value.source, output);
    }

    constexpr auto get_abi() const -> ttx_swizzle_mapping { return value; }

   private:
    ttx_swizzle_mapping value;
  };

  // Success supplies the requested observation. Failure reports its cause
  // without certifying progress, so a consumer cannot rely on the provider's
  // individual steps as another form of successful projection.
  using Result = Data::Status;

  // The target follows Mapping's output representation. Overlapping Fragment
  // reflow has an unspecified combined result unless an outer policy supplies a
  // stronger observation guarantee. Block requires an explicit input buffer
  // for projection and is therefore Unsupported by this entry.
  static auto flow(
      const Flow& flow,
      Mapping mapping,
      Data::Form::Storage target) -> Result;
};

}  // namespace Ttx::Semantic::Flows
