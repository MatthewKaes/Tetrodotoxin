// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/fixtures.hpp"

using namespace Validation::FlowTests;
static_assert(__is_standard_layout(Schema));
static_assert(__is_trivially_copyable(Schema));
static_assert(__is_trivially_copyable(Schema::Position));
static_assert(__is_trivially_copyable(Flow::Assignment));

// Use both typed value bindings directly. The reader transfers a complete
// loan into ReadResult, which keeps it alive until the writer completes.
// This exercises the public C++ types rather than casting operation tables.
PERIMORTEM_UNIT_TEST(TtxFlow, typed_value_contracts) {
  U32 source = 42, output = 0;
  auto from = view(integer, source);
  auto to = access(integer, output);
  const Query source_query = from, destination_query = to;
  ReadResult read;
  WriteResult written;
  source_query.bind<Pack::View>().visit(
      [&](DataPack::View reader) {
        reader.read_values(
            Index(), DataPack::Read::bind<&ReadResult::ready>(read));
      },
      [&](Binding::Failure) { EXPECT(false); });
  ASSERT(read.status == DataStatus::Success);
  destination_query.bind<Pack::Access>().visit(
      [&](DataPack::Access writer) {
        writer.write_values(
            Index(), read.block,
            DataPack::Write::bind<&WriteResult::ready>(written));
      },
      [&](Binding::Failure) { EXPECT(false); });
  EXPECT(written.status == DataStatus::Success);
  EXPECT_EQ(output, U32(42));
}

// Both block contracts must construct the appropriate C++ handle. These
// instantiations caught a previous wrapper error hidden by raw table casts.
PERIMORTEM_UNIT_TEST(TtxFlow, typed_block_contracts) {
  U32 source = 42, output = 0;
  auto from = view(integer, source);
  auto to = access(integer, output);
  const Query source_query = from, destination_query = to;
  ReadResult read;
  AccessResult write;
  source_query.bind<Pack::Block::View>().visit(
      [&](DataPack::Block::View reader) {
        reader.acquire(DataPack::Read::bind<&ReadResult::ready>(read));
      },
      [&](Binding::Failure) { EXPECT(false); });
  destination_query.bind<Pack::Block::Access>().visit(
      [&](DataPack::Block::Access writer) {
        writer.acquire(DataPack::Acquire::bind<&AccessResult::ready>(write));
      },
      [&](Binding::Failure) { EXPECT(false); });
  ASSERT(read.status == DataStatus::Success);
  ASSERT(write.status == DataStatus::Success);
  EXPECT(DataFlow::block(read.block, write.block) == DataStatus::Success);
  EXPECT_EQ(output, U32(42));
}
