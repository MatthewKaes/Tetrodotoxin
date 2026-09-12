// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/fixtures.hpp"

using namespace Validation::FlowTests;

// Direct support on both sides terminates the greedy search. No data is copied
// and neither endpoint is asked about another protocol after that success.
PERIMORTEM_UNIT_TEST(TtxFlow, direct_preference) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {.provides = 15, .values = {10, 20, 30, 40}};
  Reader reader{four};

  Flow flow;
  ASSERT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Success);

  EXPECT(flow.get_protocol() == Protocol::Direct);
  EXPECT_EQ(writer.binds[0], Count(1));
  for (Count i = 1; i < 4; ++i) {
    EXPECT_EQ(writer.binds[i], Count(0));
    EXPECT_EQ(reader.binds[i], Count(0));
  }

  EXPECT_EQ(writer.commits, Count(0));
  EXPECT_EQ(writer.acquires, Count(0));
}

// Preference is not inheritance. A writer supplying only Direct cannot serve
// a reader accepting only Fragment, even though native code could read and
// manufacture a Fragment adapter. Flow must not invent that implementation.
PERIMORTEM_UNIT_TEST(TtxFlow, disjoint_protocols) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {.provides = PROVIDES_DIRECT};
  Reader reader{four, PROVIDES_FRAGMENT};

  Flow flow;
  EXPECT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Unsupported);

  for (Count i = 0; i < 4; ++i) {
    EXPECT_EQ(reader.binds[i], Count(1));
  }

  EXPECT_EQ(writer.binds[3], Count(1));
  EXPECT_EQ(writer.reads, Count(0));
}

// Identity acceptance is followed by one ABI agreement. A U32 reader cannot
// cast or copy an R32 representation merely because their byte widths match.
// A policy rejection or pending bind also stops the search without a fallback.
PERIMORTEM_UNIT_TEST(TtxFlow, negotiation_failures) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {.provides = 15};
  const auto wrong = prepare(Schema::range(real, 4, 4, 16, 4));
  Reader reader{wrong};

  Flow flow;
  EXPECT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Incompatible);
  EXPECT_EQ(writer.binds[3], Count(1));
  EXPECT_EQ(writer.acquires, Count(0));

  Reader pending{four, 0};
  pending.decline = Binding::Status::Pending;
  flow.close();
  EXPECT(
      flow.connect(pending.query(), module.writer(writer)) ==
      Flow::Status::BindingPending);
  EXPECT_EQ(pending.binds[1], Count(0));

  pending.decline = Binding::Status::Rejected;
  flow.close();
  EXPECT(
      flow.connect(pending.query(), module.writer(writer)) ==
      Flow::Status::Rejected);
}

// Exhaust the four protocol cooperation matrix using independent C and C++
// implementations. The expected answer is the first common advertised bit,
// not any capability that could be synthesized from a stronger protocol.
PERIMORTEM_UNIT_TEST(TtxFlow, protocol_matrix) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  const Protocol protocols[] = {
    Protocol::Direct, Protocol::Shared, Protocol::Block, Protocol::Fragment};
  for (U8 source = 0; source < 16; ++source) {
    for (U8 target = 0; target < 16; ++target) {
      Module::State writer = {.provides = source};
      Reader reader{four, target};

      Flow flow;
      const auto status = flow.connect(reader.query(), module.writer(writer));

      const U8 common = source & target;
      if (!common) {
        EXPECT(status == Flow::Status::Unsupported);
        continue;
      }

      ASSERT(status == Flow::Status::Success);

      Count choice = 0;
      while (!(common & (1 << choice))) {
        ++choice;
      }

      EXPECT(flow.get_protocol() == protocols[choice]);
      for (Count i = choice + 1; i < 4; ++i) {
        EXPECT_EQ(reader.binds[i], Count(0));
        EXPECT_EQ(writer.binds[i], Count(0));
      }

      flow.close();
      EXPECT_EQ(writer.releases, choice == 1 ? Count(1) : Count(0));
    }
  }
}

// Binding a protocol is not yet agreement on its payload ABI. A reader may
// accept different representations under different protocols. An incompatible
// candidate performs no data access, allowing a later common ABI to win.
PERIMORTEM_UNIT_TEST(TtxFlow, protocol_abi_match) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {.provides = PROVIDES_DIRECT | PROVIDES_BLOCK};
  const auto wrong = prepare(Schema::range(real, 4, 4, 16, 4));
  Reader reader{four};
  reader.direct_schema = &wrong;

  Flow flow;
  EXPECT(
      flow.connect(reader.query(), module.writer(writer)) ==
      Flow::Status::Success);
  EXPECT(flow.get_protocol() == Protocol::Block);
  EXPECT_EQ(writer.commits, Count(0));
}

// Each bind supplies its own answer. A previous successful publication must
// not make a later empty answer usable. This is the Query admission boundary,
// so Flow and operations can consume admitted handles without repeating it.
PERIMORTEM_UNIT_TEST(TtxFlow, fresh_bind_results) {
  Module module;
  ASSERT(module.is_set());

  Module::State writer = {.provides = PROVIDES_DIRECT};
  auto published = module.writer(writer);

  struct Owner {
    Query query;
    Count calls = 0;
  } owner{published};

  const Query query({
    &owner,
    [](const void* source, perimortem_uuid id,
       ttx_binding* answer) -> ttx_binding_status {
      auto& owner = *const_cast<Owner*>(static_cast<const Owner*>(source));
      if (owner.calls++) {
        return TTX_BINDING_SATISFIED;
      }

      const auto native = static_cast<ttx_semantic_query>(owner.query);
      return native.bind(native.source, id, answer);
    },
  });

  query.bind<Ttx::Semantic::Direct::Access>().visit(
      [&](auto) {}, [&](Binding::Failure) { EXPECT(false); });

  query.bind<Ttx::Semantic::Direct::Access>().visit(
      [&](auto) { EXPECT(false); },
      [&](Binding::Failure status) {
        EXPECT(status == Binding::Failure::Rejected);
      });
}

// An independently compiled provider can still publish the retired callback
// table under its old UUID. The synchronous role has a new identity, so Flow
// cannot admit that table merely because both operations are called commit.
PERIMORTEM_UNIT_TEST(TtxFlow, retired_contract) {
  Preparation prepare;
  const auto& four = prepare(four_schema);

  Module module;
  ASSERT(module.is_set());

  Module::State writer = {.provides = PROVIDES_BLOCK};
  Reader reader{four, PROVIDES_BLOCK};

  Flow flow;
  EXPECT(
      flow.connect(reader.query(), module.legacy_writer(writer)) ==
      Flow::Status::Unsupported);
  EXPECT_EQ(writer.commits, Count(0));
}

// The preceding prepared record contract included the indexed field. Its UUID
// must retire with that promise even when native padding leaves sizeof intact.
// This fixture deliberately advertises the old identity around a live provider.
PERIMORTEM_UNIT_TEST(TtxFlow, retired_form) {
  Preparation prepare;
  const auto& four = prepare(four_schema);
  Module module;
  ASSERT(module.is_set());

  Module::State writer = {.provides = PROVIDES_DIRECT};
  const auto published = module.writer(writer);
  const Query legacy(
      {&published,
       [](const void* source, perimortem_uuid requested,
          ttx_binding* result) -> ttx_binding_status {
         if (requested.high != 0x4a902fc004e74ccfULL ||
             requested.low != 0x94bfa0d6cd66d43fULL) {
           return TTX_BINDING_UNSUPPORTED;
         }

         const auto query = static_cast<ttx_semantic_query>(
             *static_cast<const Query*>(source));
         const perimortem_uuid current = {
           TTX_DIRECT_ACCESS_ID_HIGH, TTX_DIRECT_ACCESS_ID_LOW};
         return query.bind(query.source, current, result);
       }});

  Reader reader{four, PROVIDES_DIRECT};
  Flow flow;
  EXPECT(flow.connect(reader.query(), legacy) == Flow::Status::Unsupported);
}
