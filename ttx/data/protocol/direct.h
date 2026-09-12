// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_PROTOCOL_DIRECT_H
#define TTX_DATA_PROTOCOL_DIRECT_H

#include "ttx/data/form/representation.h"

// A provider may already keep the required C representation in stable storage,
// as with a published operation table. Direct lets its reader use that address
// without adding an acquisition or release to every use. The publication must
// therefore keep the representation valid for all of those readers.
//
// View describes the ABI the reader accepts. Access supplies its representation
// and the public pointer that satisfies it. Agreement grants a cast of that
// pointer to the described C representation, not a cast of either binding's
// source. This lets the provider keep unrelated state behind its thunks while
// exposing just the representation it chose to publish. No destination Storage
// is needed to establish that agreement.
typedef struct ttx_direct_view_operations {
  const ttx_representation* (*representation)(const void* source);
} ttx_direct_view_operations;
typedef struct ttx_direct_access_operations {
  const ttx_representation* (*representation)(const void* source);
  const void* (*read_ptr)(const void* source);
} ttx_direct_access_operations;
typedef struct ttx_direct_view {
  const void* source;
  const ttx_direct_view_operations* operations;
} ttx_direct_view;
typedef struct ttx_direct_access {
  const void* source;
  const ttx_direct_access_operations* operations;
} ttx_direct_access;

#endif
