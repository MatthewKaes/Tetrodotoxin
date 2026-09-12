// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/data/form/visitation.h"

static ttx_data_status observe(
    void* source,
    ttx_representation_position position) {
  visitation_probe* probe = source;
  probe->offsets[probe->count++] = position.offset;
  return probe->count == probe->stop ? TTX_DATA_DENIED : TTX_DATA_SUCCESS;
}

ttx_data_status observe_representation(
    const ttx_representation* representation,
    const Count* coordinates,
    Count count,
    visitation_probe* probe) {
  const ttx_representation_visitor visitor = {probe, observe};
  if (coordinates) {
    return ttx_representation_visit_selected(
        representation, coordinates, count, visitor);
  }

  return ttx_representation_visit(representation, visitor);
}
