// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/pending.hpp"

using namespace Validation::FlowTests;

// Extract starts a read and returns Pending without touching the destination.
// Publishing the read starts a write, but the source loan must remain held
// while that write is pending. Only committing the complete unit changes the
// output and allows Semantic to release the loan and finish the request.
PERIMORTEM_UNIT_TEST(TtxFlow, deferred_read_then_write) {
  Deferred source, destination;
  destination.values[0] = 99;
  Flow::Request request;
  request.extract(bind_view(source.query()), bind_access(destination.query()));
  EXPECT(request.is_pending());
  EXPECT(source.reading);
  EXPECT_EQ(destination.values[0], U32(99));
  source.publish();
  EXPECT(request.is_pending());
  EXPECT(destination.writing);
  EXPECT(source.leased);
  EXPECT_EQ(source.releases, Count(0));
  EXPECT_EQ(destination.values[0], U32(99));
  destination.commit();
  EXPECT_NOT(request.is_pending());
  EXPECT(request.get_status() == Status::Success);
  EXPECT_EQ(destination.values[0], U32(42));
  EXPECT_EQ(source.releases, Count(1));
}

// Failure to obtain the first unit publishes nothing. After one separate write
// has completed, a later I/O failure is a partial semantic operation. Data's
// failed unit leaves its destination unchanged, while the request reports both
// the earlier success count and the cause for a higher recovery policy.
PERIMORTEM_UNIT_TEST(TtxFlow, partial_operation_keeps_failure_boundary) {
  const auto pair = Schema::range(integer, 2, 4, 8, 2, 4);
  Deferred source, destination;
  source.schema = destination.schema = &pair;
  destination.values[0] = destination.values[1] = 99;
  Flow::Request request;
  request.extract(bind_view(source.query()), bind_access(destination.query()));
  source.publish(DataStatus::IoError);
  EXPECT(request.get_status() == Status::IoError);
  EXPECT_EQ(destination.writes, Count(0));
  request.extract(bind_view(source.query()), bind_access(destination.query()));
  source.publish();
  destination.commit();
  EXPECT_EQ(destination.values[0], U32(42));
  EXPECT(source.reading);
  source.publish();
  destination.commit(DataStatus::IoError);
  EXPECT_NOT(request.is_pending());
  EXPECT(request.get_status() == Status::Partial);
  EXPECT(request.get_cause() == Status::IoError);
  EXPECT_EQ(request.get_completed(), Count(1));
  EXPECT_EQ(destination.values[1], U32(99));
  EXPECT_EQ(source.releases, Count(2));
}

// Huge compatible ranges stop at the first pending access. Negotiation must
// not expand the advertised count into an assignment inventory. Failing that
// pending read then completes the request without allocating a huge buffer.
PERIMORTEM_UNIT_TEST(TtxFlow, compact_handshake_before_pending_access) {
  const auto a =
      Schema::range(integer, 1000000000, 4, 4000000000, 1000000000, 4);
  const auto other = Schema::primitive(Value::U32);
  const auto b = Schema::range(other, 1000000000, 4, 4000000000, 1000000000, 4);
  Deferred source, destination;
  source.schema = &a;
  destination.schema = &b;
  Flow::Request request;
  request.extract(bind_view(source.query()), bind_access(destination.query()));
  ASSERT(request.is_pending());
  EXPECT_EQ(source.reads, Count(1));
  EXPECT_EQ(destination.writes, Count(0));
  source.publish(DataStatus::IoError);
  EXPECT_NOT(request.is_pending());
}

// A successful read still has to publish the contract that was negotiated.
// Rejecting a malformed loan releases it exactly once and starts no write.
// Reporting Pending as a completion is also invalid: there must be one later
// terminal callback rather than a completion that leaves ownership ambiguous.
PERIMORTEM_UNIT_TEST(TtxFlow, malformed_completion_releases_loan) {
  Deferred source, destination;
  Flow::Request request;
  request.extract(bind_view(source.query()), bind_access(destination.query()));
  source.requested_schema = &real;
  source.publish();
  EXPECT(request.get_status() == Status::Incompatible);
  EXPECT_EQ(source.releases, Count(1));
  EXPECT_EQ(destination.writes, Count(0));
  request.extract(bind_view(source.query()), bind_access(destination.query()));
  source.publish(DataStatus::Pending);
  EXPECT_NOT(request.is_pending());
  EXPECT(request.get_status() == Status::Invalid);
}

// Completion runs after loans have been released and request state has become
// inactive. It may immediately start another operation on that same request.
// The driver must not touch the completed operation's state afterward.
struct Restart {
  Flow::Request& request;
  Transport::View source;
  Transport::Access destination;
  Count completions = 0;
  auto done(Status status, Status cause, Count completed) -> void {
    if (status != Status::Success || cause != Status::Success ||
        completed != 1) {
      Diagnostics::Log::fatal("Unexpected restart completion."_view);
    }
    if (++completions == 1) {
      request.extract(
          source, destination, Flow::Completion::bind<&Restart::done>(*this));
    }
  }
};
PERIMORTEM_UNIT_TEST(TtxFlow, completion_can_restart_request) {
  Deferred source, destination;
  Flow::Request request;
  Restart observer{
    request, bind_view(source.query()), bind_access(destination.query())};
  request.extract(
      observer.source, observer.destination,
      Flow::Completion::bind<&Restart::done>(observer));
  source.publish();
  destination.commit();
  EXPECT_EQ(observer.completions, Count(1));
  EXPECT(request.is_pending());
  EXPECT(source.reading);
  source.publish();
  destination.commit();
  EXPECT_EQ(observer.completions, Count(2));
  EXPECT_NOT(request.is_pending());
  EXPECT_EQ(source.releases, Count(2));
}

// The source's loan has room for two U32s, but the requested unit contains
// only one. Its spare capacity must not become an extra output write, nor an
// oversized view of the conversion scratch retained by the request.
PERIMORTEM_UNIT_TEST(TtxFlow, read_capacity_does_not_expand_the_write) {
  Deferred source, destination;
  source.capacity = sizeof(source.values);
  destination.values[0] = destination.values[1] = 99;
  Flow::Request request;
  request.extract(bind_view(source.query()), bind_access(destination.query()));
  source.publish();
  EXPECT_EQ(destination.input.get_size(), Count(sizeof(U32)));
  destination.commit();
  EXPECT(request.get_status() == Status::Success);
  EXPECT_EQ(destination.values[0], U32(42));
  EXPECT_EQ(destination.values[1], U32(99));
}
