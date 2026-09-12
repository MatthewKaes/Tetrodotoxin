// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors
#ifndef VALIDATION_HETEROGENEOUS_PROVIDER_H
#define VALIDATION_HETEROGENEOUS_PROVIDER_H
#include "ttx/semantic/query.h"

// The writer computes observations from a seed. Its state has no record whose
// layout matches the advertised tag, energy and frame schema.
typedef struct heterogeneous_state {
  U32 seed;
  Count reads;
  Count descriptions;
} heterogeneous_state;
typedef struct heterogeneous_provider {
  ttx_semantic_query (*writer)(heterogeneous_state*);
  ttx_semantic_query (*reader)(heterogeneous_state*);
} heterogeneous_provider;
#endif
