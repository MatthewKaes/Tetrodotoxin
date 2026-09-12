// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef VALIDATION_TTX_DATA_FORM_VISITATION_H
#define VALIDATION_TTX_DATA_FORM_VISITATION_H

#include "ttx/data/form/representation.h"

// C owns the callback and records only a bounded prefix of observations. The
// fixture lets native tests check the C ABI without casting an opaque provider.
typedef struct visitation_probe {
  Count offsets[8];
  Count count;
  Count stop;
} visitation_probe;

PERIMORTEM_C ttx_data_status observe_representation(
    const ttx_representation* representation,
    const Count* coordinates,
    Count count,
    visitation_probe* probe);

#endif
