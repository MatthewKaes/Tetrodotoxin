// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/perimortem.h"

namespace Validation::FlowTests {

// Since flow in TTX is used for crossing binary boundaries it can make it hard
// to coordinate important states like allocations and copies without adding
// invasive test only API points. Instead The Flow tests provide a simple sample
// measurement system that captures allocation and copy stats.
class Measurement {
 public:
  Measurement();
  ~Measurement();
  Measurement(const Measurement&) = delete;
  auto operator=(const Measurement&) -> Measurement& = delete;

  auto stop() -> void;
  auto get_allocations() const -> Count { return allocations; }
  auto get_copies() const -> Count { return copies; }

  // Linker wrappers call these hooks. The examples observe counts rather than
  // replacing an allocator or a copy implementation with test behavior.
  static auto allocation() -> void;
  static auto copy() -> void;

 private:
  static Measurement* active;
  Measurement* previous;
  Count allocations = 0;
  Count copies = 0;
};

}  // namespace Validation::FlowTests
