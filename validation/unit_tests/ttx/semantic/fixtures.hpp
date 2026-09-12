// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors
#pragma once
#include "validation/unit_test.hpp"
#include "validation/unit_tests/ttx/semantic/module.hpp"
#include "validation/unit_tests/ttx/data/form/preparation.hpp"

#include "perimortem/core/access/vector.hpp"

#include "ttx/data/form/schema.hpp"
#include "ttx/semantic/block.hpp"
#include "ttx/semantic/direct.hpp"
#include "ttx/semantic/flows/copy.hpp"
#include "ttx/semantic/flows/swizzle.hpp"
#include "ttx/semantic/fragment.hpp"
#include "ttx/semantic/shared.hpp"
#include "validation/unit_tests/ttx/semantic/fixtures/heterogeneous_provider.h"
#include "validation/unit_tests/ttx/semantic/fixtures/provider.h"

namespace Validation::FlowTests {
using namespace Perimortem::Core;
using Ttx::Data::Form::Representation;
using Ttx::Data::Form::Schema;
using Ttx::Data::Status;
using Ttx::Data::Form::Storage;
using Ttx::Semantic::Binding;
using Ttx::Semantic::Flow;
using Ttx::Semantic::Query;
using Ttx::Semantic::Flows::Copy;
using Ttx::Semantic::Flows::Swizzle;
using Protocol = Flow::Protocol;
extern Harness TtxFlow;
inline constexpr auto integer = Schema::primitive(Schema::Value::U32);
inline constexpr auto real = Schema::primitive(Schema::Value::R32);
using Validation::DataTests::Preparation;
inline constexpr auto four_schema = Schema::range(integer, 4, 4, 16, 4);

// This reader is just a protocol policy and an ABI requirement. It owns no
// destination. A Storage may supply such a query too, without changing Flow.
struct Reader {
  const Representation& schema;
  U8 provides =
      PROVIDES_DIRECT | PROVIDES_SHARED | PROVIDES_BLOCK | PROVIDES_FRAGMENT;
  Count binds[4] = {};
  Count descriptions = 0;
  Binding::Status decline = Binding::Status::Unsupported;
  const Representation* direct_schema = nullptr;
  auto query() -> Query;
};

template <typename T>
auto storage(const Representation& schema, T& data) -> Storage {
  return Storage::create(schema, {reinterpret_cast<U8*>(&data), sizeof(data)})
      .visit(
          [](Storage value) { return value; },
          [](Status) -> Storage {
            Diagnostics::Log::fatal("Invalid fixture Storage."_view);
          });
}

}  // namespace Validation::FlowTests
