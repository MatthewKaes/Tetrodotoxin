// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_FLOWS_COPY_H
#define TTX_SEMANTIC_FLOWS_COPY_H

#include "ttx/data/form/storage.h"
#include "ttx/semantic/flow.h"

// Copy reports whether the promised observation was delivered. Counting the
// steps taken would make the result depend on how the provider fulfilled that
// promise. Failure can leave writes behind, but supplies no usable prefix or
// rollback guarantee. A policy that needs either must arrange it separately.
typedef struct ttx_copy_result {
  ttx_data_status status;
} ttx_copy_result;

// Copy supplies a Storage view to an established Flow and invokes a full
// simulacra copy from the source into that storage. After the copy
// the storage is considered to be a 1:1 representation of the sources promised
// wire format.
//
// Copy supports all four of the main flow transports. Fragment flows however
// can result in two possible behaviors that are mostly defined but can cause
// confusing output for overlapping transfers. Overlapping Fragment transfer has
// a well defined invocation order but an unspecified combined result. A policy
// that needs a snapshot arranges that guarantee explicitly but the copy policy
// does NOT require it.
//
// This makes Fragmented views extremely useful for generators: A random number
// generator can promise any sized representation and just lazily populate it on demand.
PERIMORTEM_C ttx_copy_result ttx_copy(const ttx_flow* flow, ttx_storage target);

#endif
