// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/semantic/flow.hpp"
#include "ttx/semantic/flows/copy.h"

namespace Ttx::Semantic::Flows {

// The semantic system for representing a "copy" when a source and destination
// both agree on an exact wire format that represents the `source`.
//
// Flowing a copy allows making an observation of that representation and
// storing it in a format provided by the destination receiver. This is often
// modeled as a `memmov` but the semantics are defined by the TTX transport used
// to perform the flow.
class Copy {
 public:
  // The result answers whether this observation succeeded. A progress count
  // would expose the transport's work as part of the representation's promise.
  // On failure the target may have changed, but no partial result is certified.
  using Result = Data::Status;

  // Performs the actual semantic representation of the flow's bounded source to
  // the target storage. If the operation was Successful then the supplied
  // storage contains the "canonical" wire form promised by that representation
  // at the requested observation point.
  //
  // Future observations can result in different results and can't be assumed
  // equivalent:
  //
  // Copy::flow(flow, a);
  // Copy::flow(flow, b);
  // a == b; // Can be false.
  //
  // The target storage's fit is always validated before performing the flow
  // and must fit the flow's required destination representation. Overlapping
  // Fragment reflow has an unspecified combined result and supplies no snapshot
  // promise.
  static auto flow(const Flow& flow, Data::Form::Storage target) -> Result;
};

}  // namespace Ttx::Semantic::Flows
