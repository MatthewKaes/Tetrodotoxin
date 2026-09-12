// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_FLOWS_SWIZZLE_H
#define TTX_SEMANTIC_FLOWS_SWIZZLE_H

#include "ttx/data/form/storage.h"
#include "ttx/semantic/flow.h"

// A naming or selection policy chooses a source coordinate for each output.
// Preparing this selection resolves primitive facts once and groups all uses
// of a source together. The resolver is borrowed only during preparation, so
// later observations do not repeat name lookup or retain that policy's state.
typedef struct ttx_swizzle_selection {
  const ttx_representation* input;
  const ttx_representation* output;
  const void* source;
  Count (*position)(const void* source, Count output_coordinate);
} ttx_swizzle_selection;

// Repeating red in five output slots still makes one observation of red. Its
// group retains that source's primitive facts and the flat list of destinations
// receiving the observation. Each destination can have its own byte order.
typedef struct ttx_swizzle_group {
  ttx_representation_position input;
  const ttx_representation_position* outputs;
  Count count;
#ifdef __cplusplus
  constexpr ttx_swizzle_group(
      ttx_representation_position input = ttx_representation_position(),
      const ttx_representation_position* outputs = nullptr,
      Count count = 0)
      : input(input), outputs(outputs), count(count) {}
#endif
} ttx_swizzle_group;

// A prepared mapping borrows immutable groups in first selection order. Every
// output primitive appears exactly once, with the same primitive type as its
// group's input. The owner establishes those facts before publishing this
// carrier and retains the groups, positions and representations through use.
// Native Mapping owns its arrays, while a C producer may publish fixed arrays.
typedef struct ttx_swizzle_mapping {
  const ttx_representation* input;
  const ttx_representation* output;
  const ttx_swizzle_group* groups;
  Count count;
} ttx_swizzle_mapping;

// The Flow and mapping are already established. This call observes each source
// group once and writes all its destinations before continuing. Failure reports
// its cause without certifying a partial result. Overlapping Fragment reflow
// has no snapshot guarantee. Block requires separate input materialization and
// is Unsupported here because this operation owns no intermediate buffer.
PERIMORTEM_C ttx_data_status ttx_swizzle(
    const ttx_flow* flow,
    ttx_swizzle_mapping mapping,
    ttx_storage target);

#endif
