// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/fixtures.hpp"

using namespace Validation::FlowTests;

// The naming policy first resolves red. The resulting numeric assignment
// repeats its observation into two ordered outputs. Changing red between
// requests changes both outputs without changing either schema or mapping.
PERIMORTEM_UNIT_TEST(TtxFlow, extraction) {
  R32 source[] = {1, 2, 3, 4}, output[] = {0, 0};
  auto from = view(color, source, {color_names, 4});
  const auto pair = Schema::range(real, 2, 4, 8, 2, 4);
  const Flow::Assignment repeated = {named(from, "r"_view), Index(0), 2, 0, 1};
  EXPECT(
      extract(from, access(pair, output), {&repeated, 1}) == Status::Success);
  EXPECT_EQ(output[0], R32(1));
  EXPECT_EQ(output[1], R32(1));
  source[0] = 9;
  EXPECT(
      extract(from, access(pair, output), {&repeated, 1}) == Status::Success);
  EXPECT_EQ(output[1], R32(9));
}

// A slice is a semantic correspondence between successive positions. Block
// transfer does not accept this map. A policy may instead expose this slice as
// its own completed schema and lend a block matching that smaller contract.
PERIMORTEM_UNIT_TEST(TtxFlow, contiguous_slice) {
  R32 source[] = {1, 2, 3, 4}, output[] = {0, 0};
  const auto pair = Schema::range(real, 2, 4, 8, 2, 4);
  const Flow::Assignment slice = {Index(1), Index(0), 2, 1, 1};
  EXPECT(
      extract(view(color, source), access(pair, output), {&slice, 1}) ==
      Status::Success);
  EXPECT_EQ(output[0], R32(2));
  EXPECT_EQ(output[1], R32(3));
}
PERIMORTEM_UNIT_TEST(TtxFlow, named_member) {
  R32 source[] = {1, 2, 3, 4}, output = 0;
  auto from = view(color, source, {color_names, 4});
  const Flow::Assignment member = {named(from, "b"_view), Index(0)};
  EXPECT(extract(from, access(real, output), {&member, 1}) == Status::Success);
  EXPECT_EQ(output, R32(3));
}

// Named arguments resolve to destination positions before execution. The map
// lists those outputs in order, while sources can arrive in another order.
// Missing or duplicate destinations are rejected before the first write.
PERIMORTEM_UNIT_TEST(TtxFlow, named_arguments_and_coverage) {
  U32 source[] = {1, 20, 30, 40, 50}, output[] = {99, 99, 99, 99, 99};
  const auto five = Schema::range(integer, 5, 4, 20, 5, 4);
  const View::Bytes names[] = {
    "count"_view, "mode"_view, "name"_view, "value"_view, "flags"_view};
  auto to = access(five, output, {names, 5});
  const Flow::Assignment mapping[] = {
    {Index(0), Index(0)},
    {Index(3), named(to, "mode"_view)},
    {Index(1), named(to, "name"_view)},
    {Index(2), named(to, "value"_view)},
    {Index(4), named(to, "flags"_view)},
  };
  EXPECT(extract(view(five, source), to, {mapping, 3}) == Status::Incomplete);
  EXPECT_EQ(output[0], U32(99));
  EXPECT(extract(view(five, source), to, {mapping, 5}) == Status::Success);
  EXPECT_EQ(output[1], U32(40));
  EXPECT_EQ(output[2], U32(20));
  const Flow::Assignment conflict[] = {
    {Index(0), Index(0)}, {Index(1), Index(0)}};
  EXPECT(extract(view(five, source), to, {conflict, 2}) == Status::Conflict);
  const Flow::Assignment reversed[] = {
    {Index(0), Index(1)}, {Index(1), Index(0)}};
  EXPECT(extract(view(five, source), to, {reversed, 2}) == Status::Incomplete);
}

// A numeric path can open a Group and select its second member. Full fallback
// traversal also opens nested groups physically without removing their
// semantic boundary from the compatibility check.
PERIMORTEM_UNIT_TEST(TtxFlow, composite_range_and_groups) {
  U32 source[] = {10, 20, 30}, output = 0;
  const Schema::Position pair[] = {{integer, 0, 0}, {integer, 4, 1}};
  const auto group = Schema::group({pair, 2}, 8, 2, 4);
  const Schema::Position outer[] = {{group, 0, 0}, {integer, 8, 1}};
  const auto shape = Schema::composite({outer, 2}, 12, 2, 4);
  const Count scope[] = {0};
  const Flow::Assignment member = {Index({scope, 1}, 1), Index(0)};
  EXPECT(
      extract(view(shape, source), access(integer, output), {&member, 1}) ==
      Status::Success);
  EXPECT_EQ(output, U32(20));
  U32 copied[3] = {};
  const Flow::Assignment explicit_whole = {};
  EXPECT(
      extract(
          view(shape, source), access(shape, copied), {&explicit_whole, 1}) ==
      Status::Success);
  EXPECT_EQ(memcmp(source, copied, sizeof(source)), 0);
}
PERIMORTEM_UNIT_TEST(TtxFlow, empty_flow_and_overflow) {
  const auto empty = Schema::composite({}, 0, 0);
  Block::View source(empty, {});
  Block::Access destination(empty, {});
  EXPECT(
      extract(Pack::view(source), Pack::access(destination), {}) ==
      Status::Success);
  const auto huge =
      Schema::range(integer, Count(-1), 4, Count(-1), Count(-1), 4);
  EXPECT(huge.validate() == DataStatus::Overflow);
}

// Name failure belongs to its semantic owner. A missing or ambiguous name
// never becomes a fabricated numeric index accepted by the Data engine.
PERIMORTEM_UNIT_TEST(TtxFlow, ambiguous_names_and_access_failures) {
  U32 values[] = {1, 2};
  const auto pair = Schema::range(integer, 2, 4, 8, 2, 4);
  const View::Bytes duplicate[] = {"x"_view, "x"_view};
  auto source = view(pair, values, {duplicate, 2});
  const Query query = source;
  query.bind<Names>().visit(
      [&](Names names) {
        names.find("x"_view).visit(
            [&](Count) { EXPECT(false); },
            [&](Names::Failure failure) {
              EXPECT(failure == Names::Failure::Ambiguous);
            });
        names.find("missing"_view)
            .visit(
                [&](Count) { EXPECT(false); },
                [&](Names::Failure failure) {
                  EXPECT(failure == Names::Failure::Missing);
                });
      },
      [&](Binding::Failure) { EXPECT(false); });
  Transport::view({}).visit(
      [&](Transport::View) { EXPECT(false); },
      [&](Binding::Failure status) {
        EXPECT(status == Binding::Failure::Rejected);
      });
}

// Two policy projections borrow disjoint values in the same source storage.
// Neither has to duplicate that storage merely to expose another contract.
PERIMORTEM_UNIT_TEST(TtxFlow, overlapping_policies_share_backing) {
  R32 source[] = {1, 2, 3, 4}, red = 0, blue = 0;
  Block::View r(real, {reinterpret_cast<const U8*>(&source[0]), 4});
  Block::View b(real, {reinterpret_cast<const U8*>(&source[2]), 4});
  EXPECT(extract(Pack::view(r), access(real, red)) == Status::Success);
  EXPECT(extract(Pack::view(b), access(real, blue)) == Status::Success);
  source[0] = 5;
  EXPECT(extract(Pack::view(r), access(real, red)) == Status::Success);
  EXPECT_EQ(red, R32(5));
  EXPECT_EQ(blue, R32(3));
}

// A functional map can also reuse its negotiated indexed endpoints. This
// example makes the transport choice explicit before supplying the swizzle,
// rather than letting extract repeat negotiation for every operation.
PERIMORTEM_UNIT_TEST(TtxFlow, swizzle_consumes_retained_indexed_access) {
  R32 values[] = {1, 2, 3, 4}, output[] = {0, 0};
  const auto pair = Schema::range(real, 2, 4, 8, 2, 4);
  auto source_owner = view(color, values);
  auto destination_owner = access(pair, output);
  const auto source = bind_view(source_owner, Transport::Protocol::Indexed);
  const auto destination =
      bind_access(destination_owner, Transport::Protocol::Indexed);
  const Flow::Assignment repeated = {Index(0), Index(0), 2, 0, 1};
  Flow::Request request;
  request.swizzle(source, destination, {&repeated, 1});
  EXPECT(request.get_status() == Status::Success);
  EXPECT_EQ(output[1], R32(1));
  values[0] = 7;
  request.swizzle(source, destination, {&repeated, 1});
  EXPECT(request.get_status() == Status::Success);
  EXPECT_EQ(output[1], R32(7));
}
