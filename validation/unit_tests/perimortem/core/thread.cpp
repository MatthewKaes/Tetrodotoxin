// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_test.hpp"

#include <sched.h>

#include "perimortem/core/access/bytes.hpp"
#include "perimortem/core/static/bytes.hpp"
#include "perimortem/core/reader/binary.hpp"
#include "perimortem/core/thread/worker.hpp"
#include "perimortem/core/writer/binary.hpp"

using namespace Perimortem::Core;
using namespace Validation;

static Harness CoreThreadWorker = {
  .name = "Core::Thread::Worker"_view,
};

static constexpr Count large_worker_payload_size = 4096;
static constexpr View::Bytes long_worker_name =
    "worker.thread.name.with.more.than.twenty.four.bytes"_view;
static constexpr Count long_worker_name_size = long_worker_name.get_size();

struct WorkerPayloadResult {
  Count byte_count = 0;
  U64 checksum = 0;
  Bool read_successfully = False;
};

struct WorkerNameResult {
  Count name_size = 0;
  U64 checksum = 0;
  Bool matches_expected = False;
  Bool read_successfully = False;
};

struct WorkerCountResult {
  Count active_worker_count = 0;
  Count thread_id = 0;
  Bool started = False;
  Bool release = False;
  Bool read_successfully = False;
};

static auto fill_worker_payload(
    Static::Bytes<large_worker_payload_size>& payload_bytes) -> void {
  for (Count i = 0; i < payload_bytes.get_size(); i++) {
    payload_bytes[i] = U8((i * 37 + 11) & 0xFF);
  }
}

static auto checksum_worker_payload(View::Bytes payload_bytes) -> U64 {
  U64 checksum = 1469598103934665603ull;
  for (Count i = 0; i < payload_bytes.get_size(); i++) {
    checksum ^= payload_bytes[i];
    checksum *= 1099511628211ull;
  }

  return checksum;
}

static auto read_large_payload_job(View::Bytes job_data) -> void {
  Reader::Binary<Data::ByteOrder::Native> reader(job_data);
  const auto result_address = reader.read_u64();
  const auto payload_bytes = reader.read_bytes(large_worker_payload_size);
  if (!result_address || !payload_bytes || *result_address == 0) {
    return;
  }

  auto* worker_result = reinterpret_cast<WorkerPayloadResult*>(*result_address);
  worker_result->byte_count = payload_bytes->get_size();
  worker_result->checksum = checksum_worker_payload(*payload_bytes);
  worker_result->read_successfully = reader.get_location() == reader.get_size();
}

static auto read_thread_name_job(View::Bytes job_data) -> void {
  Reader::Binary<Data::ByteOrder::Native> reader(job_data);
  const auto result_address = reader.read_u64();
  const auto expected_name_size = reader.read_u64();
  if (!result_address || !expected_name_size || *result_address == 0) {
    return;
  }

  const auto expected_name = reader.read_bytes(*expected_name_size);
  if (!expected_name) {
    return;
  }

  auto* worker_result = reinterpret_cast<WorkerNameResult*>(*result_address);
  const View::Bytes actual_name = Thread::Worker::get_thread_name();
  worker_result->name_size = actual_name.get_size();
  worker_result->checksum = checksum_worker_payload(actual_name);
  worker_result->matches_expected = actual_name == *expected_name;
  worker_result->read_successfully = reader.get_location() == reader.get_size();
}

static auto hold_worker_job(View::Bytes job_data) -> void {
  Reader::Binary<Data::ByteOrder::Native> reader(job_data);
  const auto result_address = reader.read_u64();
  if (!result_address || *result_address == 0) {
    return;
  }

  auto* worker_result = reinterpret_cast<WorkerCountResult*>(*result_address);
  worker_result->active_worker_count = Thread::Worker::get_worker_count();
  worker_result->thread_id = Thread::Worker::get_thread_id();
  worker_result->read_successfully = reader.get_location() == reader.get_size();
  __atomic_store_n(&worker_result->started.value, True.value, __ATOMIC_RELEASE);
  while (Bool(__atomic_load_n(
             &worker_result->release.value, __ATOMIC_ACQUIRE)) != True) {
    sched_yield();
  }
}

PERIMORTEM_UNIT_TEST(CoreThreadWorker, copies_large_job) {
  WorkerPayloadResult worker_result;
  Static::Bytes<large_worker_payload_size> expected_payload;
  fill_worker_payload(expected_payload);

  const U64 expected_checksum = checksum_worker_payload(expected_payload);

  Static::Bytes<sizeof(U64) + large_worker_payload_size + sizeof(U64) * 2>
      job_storage;
  Count source_offset = 1;
  if ((Count(job_storage.get_data() + source_offset) & (sizeof(U64) - 1)) ==
      0) {
    source_offset++;
  }

  Access::Bytes job_data(
      job_storage.get_data() + source_offset,
      job_storage.get_size() - source_offset);

  Writer::Binary<Data::ByteOrder::Native> writer(job_data);
  writer << reinterpret_cast<U64>(&worker_result)
         << expected_payload.get_view();
  ASSERT(writer.is_valid());

  Thread::Worker worker = Thread::Worker::start(
      "worker.payload"_view, read_large_payload_job, writer);
  for (Count i = 0; i < job_storage.get_size(); i++) {
    job_storage[i] = 0xA5;
  }

  worker.join();
  EXPECT(worker_result.read_successfully);
  EXPECT_EQ(worker_result.byte_count, large_worker_payload_size);
  EXPECT_EQ(worker_result.checksum, expected_checksum);
}

PERIMORTEM_UNIT_TEST(CoreThreadWorker, copies_thread_name) {
  WorkerNameResult worker_result;
  Static::Bytes<long_worker_name_size> requested_name(long_worker_name);
  const U64 expected_checksum = checksum_worker_payload(long_worker_name);

  Static::Bytes<sizeof(U64) * 2 + long_worker_name_size + sizeof(U64) * 2>
      job_storage;

  Writer::Binary<Data::ByteOrder::Native> writer(job_storage.get_access());
  writer << reinterpret_cast<U64>(&worker_result) << U64(long_worker_name_size)
         << requested_name.get_view();
  ASSERT(writer.is_valid());

  Thread::Worker worker = Thread::Worker::start(
      requested_name.get_view(), read_thread_name_job, writer);
  for (Count i = 0; i < requested_name.get_size(); i++) {
    requested_name[i] = 0;
  }

  worker.join();
  EXPECT(worker_result.read_successfully);
  EXPECT(worker_result.matches_expected);
  EXPECT_EQ(worker_result.name_size, long_worker_name_size);
  EXPECT_EQ(worker_result.checksum, expected_checksum);
}

PERIMORTEM_UNIT_TEST(CoreThreadWorker, worker_count) {
  WorkerCountResult worker_result;
  const Count baseline_worker_count = Thread::Worker::get_worker_count();

  Static::Bytes<sizeof(U64)> job_storage;
  Writer::Binary<Data::ByteOrder::Native> writer(job_storage.get_access());
  writer << reinterpret_cast<U64>(&worker_result);
  ASSERT(writer.is_valid());

  Thread::Worker worker =
      Thread::Worker::start("worker.count"_view, hold_worker_job, writer);
  while (Bool(__atomic_load_n(
             &worker_result.started.value, __ATOMIC_ACQUIRE)) != True) {
    sched_yield();
  }

  EXPECT(worker_result.read_successfully);
  EXPECT(worker_result.thread_id < Thread::Worker::max_workers());
  EXPECT_EQ(worker_result.active_worker_count, baseline_worker_count + 1);
  EXPECT_EQ(Thread::Worker::get_worker_count(), baseline_worker_count + 1);

  __atomic_store_n(&worker_result.release.value, True.value, __ATOMIC_RELEASE);
  worker.join();

  EXPECT_EQ(Thread::Worker::get_worker_count(), baseline_worker_count);
}
