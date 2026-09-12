// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/measurement.hpp"
#include "validation/unit_tests/ttx/data/module.hpp"
#include "validation/unit_tests/ttx/data/pending.hpp"

using namespace Validation::FlowTests;

// Sharing negotiates a ready block with no destination and no mapping. Before
// publication there is no readable address. After publication the receiver
// holds the provider's actual address until release. This is the DMA lifetime
// contract, with simulated device readiness supplied by publish().
PERIMORTEM_UNIT_TEST(TtxFlow, shared_publication_and_release) {
  Deferred source;
  source.blocks = true;
  Flow::Request request;
  ReadResult reader;
  request.share(
      bind_view(source.query()), integer,
      DataPack::Read::bind<&ReadResult::ready>(reader));
  EXPECT(request.is_pending());
  EXPECT(reader.status == DataStatus::Pending);
  EXPECT(reader.block.get_abi().data == nullptr);
  source.publish();
  ASSERT(reader.status == DataStatus::Success);
  EXPECT_NOT(request.is_pending());
  EXPECT(source.leased);
  EXPECT(
      reader.block.get_bytes().get_data() ==
      reinterpret_cast<const U8*>(source.values));
  reader.block.release();
  EXPECT_NOT(source.leased);
  EXPECT_EQ(source.releases, Count(1));
}

// Independently compiled C and C++ schemas agree structurally. Acquisition
// transfers the C owner's loan to the native wrapper without copying bytes.
// A second acquisition while that storage is leased is rejected. The loaded
// module remains alive until every release thunk has been called.
PERIMORTEM_UNIT_TEST(TtxFlow, c_shared_block_has_exact_lifetime) {
  Module provider;
  ASSERT(provider.symbol);
  Module::State source = {.stable = 1};
  const auto schema = Schema::range(integer, 100, 4, 400, 100, 4);
  ReadResult reader, competing;
  Flow::Request request, other;
  Measurement measurement;
  request.share(
      bind_view(provider.source(source)), schema,
      DataPack::Read::bind<&ReadResult::ready>(reader));
  measurement.stop();
  ASSERT(reader.status == DataStatus::Success);
  EXPECT_EQ(source.releases, Count(0));
  EXPECT_EQ(measurement.get_copies(), Count(0));
  EXPECT_EQ(measurement.get_allocations(), Count(0));
  other.share(
      bind_view(provider.source(source)), schema,
      DataPack::Read::bind<&ReadResult::ready>(competing));
  EXPECT(competing.status == DataStatus::Busy);
  reader.block.release();
  EXPECT_EQ(source.releases, Count(1));
  request.share(
      bind_view(provider.source(source)), schema,
      DataPack::Read::bind<&ReadResult::ready>(reader));
  ASSERT(reader.status == DataStatus::Success);
  reader.block.release();
  EXPECT_EQ(source.releases, Count(2));
}

// Sharing cannot silently swizzle, reinterpret an equally wide type or
// materialize a missing block. Mismatch is rejected before acquisition, while
// a value-only provider simply declines the stronger sharing agreement.
PERIMORTEM_UNIT_TEST(TtxFlow, shared_contract_rejects_mismatch) {
  Deferred source;
  source.blocks = true;
  Flow::Request request;
  ReadResult reader;
  request.share(
      bind_view(source.query()), real,
      DataPack::Read::bind<&ReadResult::ready>(reader));
  EXPECT(request.get_status() == Status::Incompatible);
  EXPECT_EQ(source.acquisitions, Count(0));
  source.blocks = false;
  request.share(
      bind_view(source.query()), integer,
      DataPack::Read::bind<&ReadResult::ready>(reader));
  EXPECT(request.get_status() == Status::Unsupported);
}
