// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_PROTOCOL_BLOCK_H
#define TTX_DATA_PROTOCOL_BLOCK_H

#include "ttx/data/form/storage.h"

// A provider may be able to produce an entire record without keeping
// a pointer that it can lend. Block lets the reader supply the storage instead.
// View obtains a surface from the operation's target Storage, then Access
// commits data matching the agreed descriptor into that surface. Each operation
// supplies its own surface so separate calls do not replace a destination
// stored on the provider.
//
// Commit finishes before returning. Success publishes the complete result.
// Failure supplies no readable result, even if the provider touched bytes while
// producing it. The provider retains no surface borrow after either outcome.
// A policy that needs deferred execution owns that work outside this contract.
typedef struct ttx_block_surface {
  U8* data;
  Count size;
} ttx_block_surface;
typedef struct ttx_block_view_operations {
  const ttx_representation* (*representation)(const void* source);
  ttx_block_surface (*surface)(const void* source, ttx_storage target);
} ttx_block_view_operations;
typedef struct ttx_block_access_operations {
  const ttx_representation* (*representation)(const void* source);
  ttx_data_status (*commit)(const void* source, ttx_block_surface surface);
} ttx_block_access_operations;
typedef struct ttx_block_view {
  const void* source;
  const ttx_block_view_operations* operations;
} ttx_block_view;
typedef struct ttx_block_access {
  const void* source;
  const ttx_block_access_operations* operations;
} ttx_block_access;

#endif
