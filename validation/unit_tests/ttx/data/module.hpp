// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/semantic/flow.hpp"
#include "validation/unit_tests/ttx/data/named_provider.h"
#include "validation/unit_tests/ttx/data/provider.h"

namespace Validation::FlowTests {

// The module owns the code behind every returned C thunk. Examples construct
// it before their provider state so it is destroyed after all borrowed queries
// and calls are finished. Loading lives here to keep it out of the data flow
// being demonstrated and out of any allocation measurement.
class Module {
 public:
  using State = flow_provider_state;
  using NamedState = named_provider_state;
  using Operations = flow_operations;
  using Sink = flow_sink_state;
  Module(
      const char* library = "libflow_provider.so",
      const char* entry = "flow_provider_open");
  ~Module();
  Module(const Module&) = delete;
  auto operator=(const Module&) -> Module& = delete;

  auto source(State& state) const -> Ttx::Semantic::Query;
  auto destination(State& state) const -> Ttx::Semantic::Query;
  auto operations(State& state) const -> Ttx::Semantic::Query;
  auto counter(U32& next) const -> Ttx::Semantic::Query;
  auto sink(Sink& state) const -> Ttx::Semantic::Query;
  static auto set_block_status(
      State& state,
      Ttx::Semantic::Binding::Status status) -> void;
  auto named_source(NamedState& state) const -> Ttx::Semantic::Query;
  auto named_destination(NamedState& state) const -> Ttx::Semantic::Query;
  auto select_edges(
      Ttx::Semantic::Query source,
      Ttx::Semantic::Query destination) const -> Ttx::Semantic::Flow::Status;
  void* symbol = nullptr;

 private:
  auto get() const -> const flow_provider*;
  auto named() const -> const named_provider*;
  void* handle = nullptr;
};

}  // namespace Validation::FlowTests
