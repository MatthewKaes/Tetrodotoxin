// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/memory/dynamic/vector.hpp"

#include "ttx/semantic/flow.hpp"
#include "ttx/semantic/flows/swizzle.h"

namespace Ttx::Semantic::Flows {

// A swizzle selects observations from an established Flow. Repeated selections
// share one observed value, even if a Fragment provider would generate a new
// answer for another read. Mapping owns that correspondence so execution can
// use it repeatedly without names, schema searches or temporary allocations.
class Swizzle {
 public:
  // Preparation groups destinations by their selected source coordinate and
  // resolves the source facts in one ordered walk. The temporary name resolver
  // and lookup map disappear afterward. Only the selected primitive facts and
  // their output positions remain, in the order each source was first selected.
  // Representations stay borrowed, while these position arrays belong to
  // Mapping.
  class Mapping {
   public:
    Mapping(const Mapping&) = delete;
    Mapping(Mapping&&) = default;

    static auto create(ttx_swizzle_selection selection)
        -> Perimortem::Utility::Result<Mapping, Data::Status>;

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

    auto get_input() const -> const Data::Form::Representation& {
      return input;
    }
    auto get_output() const -> const Data::Form::Representation& {
      return output;
    }

    auto get_abi() const -> ttx_swizzle_mapping {
      return {&input, &output, groups.get_data(), groups.get_size()};
    }

   private:
    Mapping(
        const Data::Form::Representation& input,
        const Data::Form::Representation& output,
        Count group_count,
        Count output_count)
        : input(input),
          output(output),
          groups(group_count),
          outputs(output_count) {}

    const Data::Form::Representation& input;
    const Data::Form::Representation& output;
    Perimortem::Memory::Dynamic::Vector<ttx_swizzle_group> groups;
    Perimortem::Memory::Dynamic::Vector<Data::Form::Representation::Position>
        outputs;
  };

  // The result concerns the whole observation. On failure some destinations
  // may have changed, but exposing progress would promise a partial projection.
  //
  // Mapping's output specifies the target form, including padding locations
  // whose byte values remain unspecified. Block needs explicit materialization
  // first. Other protocols reuse their existing agreement without fallback.
  static auto flow(
      const Flow& flow,
      const Mapping& mapping,
      Data::Form::Storage target) -> Data::Status;
};

}  // namespace Ttx::Semantic::Flows
