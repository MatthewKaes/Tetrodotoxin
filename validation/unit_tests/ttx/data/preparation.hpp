// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "validation/unit_test.hpp"

#include "ttx/data/form/representation.hpp"
#include "ttx/data/form/schema.hpp"

namespace Validation::DataTests {

// A test owns its prepared metadata for exactly the duration of that case.
// Keeping preparation outside operation measurements lets those measurements
// detect accidental compilation or allocation during established Flow use.
class Preparation {
 public:
  auto operator()(const Ttx::Data::Form::Schema& source)
      -> const Ttx::Data::Form::Representation& {
    return Ttx::Data::Form::Representation::compile(source, arena)
        .visit(
            [](const Ttx::Data::Form::Representation& result)
                -> const Ttx::Data::Form::Representation& { return result; },
            [](Ttx::Data::Status) -> const Ttx::Data::Form::Representation& {
              Perimortem::Core::Diagnostics::Log::fatal(
                  "Invalid fixture Representation."_view);
            });
  }

  auto validate(const Ttx::Data::Form::Schema& source) -> Ttx::Data::Status {
    return Ttx::Data::Form::Representation::compile(source, arena)
        .visit(
            [](const Ttx::Data::Form::Representation&) {
              return Ttx::Data::Status::Success;
            },
            [](Ttx::Data::Status status) { return status; });
  }

 private:
  Perimortem::Memory::Allocator::Arena arena;
};

}  // namespace Validation::DataTests
