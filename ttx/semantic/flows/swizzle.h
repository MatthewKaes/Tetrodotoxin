// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_FLOWS_SWIZZLE_H
#define TTX_SEMANTIC_FLOWS_SWIZZLE_H

#include "ttx/data/form/storage.h"
#include "ttx/semantic/flow.h"

// Flow establishes access, while the mapping chooses one source coordinate
// for each output primitive. The completed output representation supplies
// order and
// coverage, allowing repetition without writing a destination twice.
// Admission
// checks those choices once, and the borrowed policy remains deterministic
// through
// every call that uses it.
typedef struct ttx_swizzle_mapping {
  const ttx_representation* input;
  const ttx_representation* output;
  const void* source;
  Count (*position)(const void* source, Count output_coordinate);
} ttx_swizzle_mapping;

// The result describes the whole requested observation. A failed projection
// can leave writes behind, but exposing a progress count would add a partial
// result contract that the chosen transport need not provide. Recovery
// belongs
// to an outer policy with its own observation and storage guarantees.
typedef struct ttx_swizzle_result {
  ttx_data_status status;
} ttx_swizzle_result;

// The representations are already admitted. This checks that each chosen
// input supplies
// the required output type before any provider observation or destination
// write.
PERIMORTEM_C ttx_data_status ttx_swizzle_mapping_check(
    ttx_swizzle_mapping mapping);

// The Flow is ready and the mapping is admitted. Block only supplies its
// whole
// input representation and this entry has no input buffer, so Block
// projection
// remains Unsupported. Overlapping Fragment reflow supplies no snapshot
// promise.
PERIMORTEM_C ttx_swizzle_result ttx_swizzle(
    const ttx_flow* flow,
    ttx_swizzle_mapping mapping,
    ttx_storage target);

#endif
