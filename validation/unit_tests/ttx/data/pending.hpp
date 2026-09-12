// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "validation/unit_tests/ttx/data/fixtures.hpp"

namespace Validation::FlowTests {

// A manually clocked provider makes publication and write completion separate
// observable steps. It needs no thread or scheduler to test the lifetime
// contract. The tests decide when each outstanding callback is delivered.
struct Deferred {
  const Schema* schema = &integer;
  U32 values[2] = {42, 84};
  Count reads = 0;
  Count writes = 0;
  Count releases = 0;
  Count acquisitions = 0;
  Bool blocks = false;
  Bool leased = false;
  Bool reading = false;
  Bool writing = false;
  DataPack::Read pending_read{{}};
  DataPack::Write pending_write{{}};
  const Schema* requested_schema = nullptr;
  Count offset = 0;
  Count capacity = 0;
  View::Bytes input;

  auto publish(DataStatus status = DataStatus::Success) -> void {
    auto callback = pending_read;
    reading = false;
    if (status != DataStatus::Success) {
      callback.complete(status);
      return;
    }
    leased = true;
    callback.complete(
        status, Block::View({
                  requested_schema,
                  reinterpret_cast<const U8*>(values) + offset,
                  capacity ? capacity : requested_schema->extent,
                  this,
                  [](const void* source) {
                    auto& owner = *const_cast<Deferred*>(
                        static_cast<const Deferred*>(source));
                    owner.leased = false;
                    ++owner.releases;
                  },
                }));
  }
  auto commit(DataStatus status = DataStatus::Success) -> void {
    const auto callback = pending_write;
    if (status == DataStatus::Success) {
      memcpy(
          reinterpret_cast<U8*>(values) + offset, input.get_data(),
          input.get_size());
    }
    input = {};
    writing = false;
    callback.complete(status);
  }
  auto query() -> Query {
    return Query(
        {this,
         [](const void* source, perimortem_uuid requested,
            ttx_binding* result) -> ttx_binding_status {
           auto& self =
               *const_cast<Deferred*>(static_cast<const Deferred*>(source));
           const auto describe = +[](const void* source,
                                     const Schema** result) -> ttx_data_status {
             *result = static_cast<const Deferred*>(source)->schema;
             return TTX_DATA_SUCCESS;
           };
           static const DataPack::View::Operations view = {
             describe,
             [](const void* source, ttx_data_index index,
                ttx_pack_read result) {
               auto& self =
                   *const_cast<Deferred*>(static_cast<const Deferred*>(source));
               DataPack::Read callback(result);
               if (self.reading || self.leased) {
                 callback.complete(DataStatus::Busy);
                 return;
               }
               Index(index)
                   .resolve(*self.schema)
                   .visit(
                       [&](const Schema::Position& position) {
                         self.requested_schema = position.schema;
                         self.offset = position.offset;
                         self.pending_read = callback;
                         self.reading = true;
                         ++self.reads;
                       },
                       [&](DataStatus status) { callback.complete(status); });
             },
           };
           static const DataPack::Access::Operations access = {
             describe,
             [](const void* source, ttx_data_index index, ttx_block_view values,
                ttx_pack_write result) {
               auto& self =
                   *const_cast<Deferred*>(static_cast<const Deferred*>(source));
               DataPack::Write callback(result);
               if (self.writing) {
                 callback.complete(DataStatus::Busy);
                 return;
               }
               Index(index)
                   .resolve(*self.schema)
                   .visit(
                       [&](const Schema::Position& position) {
                         self.offset = position.offset;
                         self.input = {values.data, values.size};
                         self.pending_write = callback;
                         self.writing = true;
                         ++self.writes;
                       },
                       [&](DataStatus status) { callback.complete(status); });
             },
           };
           static const DataPack::Block::View::Operations block = {
             describe,
             [](const void* source, ttx_pack_read result) {
               auto& self =
                   *const_cast<Deferred*>(static_cast<const Deferred*>(source));
               DataPack::Read callback(result);
               if (self.leased || self.reading) {
                 callback.complete(DataStatus::Busy);
                 return;
               }
               self.pending_read = callback;
               self.requested_schema = self.schema;
               self.offset = 0;
               self.reading = true;
               ++self.acquisitions;
             },
           };
           const Perimortem::System::Uuid id(requested);
           if (id == Pack::View::contract_id) {
             *result = Binding::provide<Pack::View>(&self, view).get_abi();
           } else if (id == Pack::Access::contract_id) {
             *result = Binding::provide<Pack::Access>(&self, access).get_abi();
           } else if (id == Pack::Block::View::contract_id && self.blocks) {
             *result =
                 Binding::provide<Pack::Block::View>(&self, block).get_abi();
           } else {
             return TTX_BINDING_UNSUPPORTED;
           }
           return TTX_BINDING_SATISFIED;
         }});
  }
};

}  // namespace Validation::FlowTests
