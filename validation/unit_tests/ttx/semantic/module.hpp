// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/semantic/flows/swizzle.hpp"
#include "validation/unit_tests/ttx/semantic/fixtures/heterogeneous_provider.h"
#include "validation/unit_tests/ttx/semantic/fixtures/provider.h"

namespace Validation::FlowTests {

// The shared library owns the code behind every returned thunk. Its scope
// encloses the provider states and retained Flows in each example.
class Module {
 public:
  using State = provider_state;
  using Heterogeneous = heterogeneous_state;
  using Primitives = provider_values;
  explicit Module(
      const char* library = "libflow_provider.so",
      const char* entry = "flow_provider_open");
  ~Module();
  Module(const Module&) = delete;
  auto operator=(const Module&) -> Module& = delete;
  auto writer(State& state) const -> Ttx::Semantic::Query;
  auto legacy_writer(State& state) const -> Ttx::Semantic::Query;
  auto writer(Heterogeneous& state) const -> Ttx::Semantic::Query;
  auto bootstrap_writer() const -> Ttx::Semantic::Query;
  auto import_query(const Ttx::Semantic::Flow& bootstrap) const
      -> Ttx::Semantic::Query;
  auto selection() const -> Ttx::Semantic::Flows::Swizzle::Mapping;
  auto primitives() const -> Ttx::Semantic::Query;
  auto primitive_schema() const -> const Ttx::Data::Form::Representation&;
  auto select(const Ttx::Semantic::Flow& flow, Ttx::Data::Form::Storage target)
      const -> Ttx::Data::Status;
  auto is_set() const -> Bool {
    return api != nullptr || heterogeneous != nullptr;
  }

 private:
  void* module = nullptr;
  const provider_api* api = nullptr;
  const heterogeneous_provider* heterogeneous = nullptr;
};
}  // namespace Validation::FlowTests
