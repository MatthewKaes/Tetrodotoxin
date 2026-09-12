// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef VALIDATION_DATA_NAMED_PROVIDER_H
#define VALIDATION_DATA_NAMED_PROVIDER_H

#include "ttx/semantic/flow.h"

// The fixture exposes its observations so C++ can check the provider without
// inspecting the state carried by a Pack. Generated values are derived from
// the seed rather than stored in a buffer matching the advertised Schema.
typedef struct named_provider_state {
  U32 seed;
  Count reads;
  Count queries;
  Count writes;
  U16 tag;
  R64 energy;
  U32 frame;
} named_provider_state;

typedef struct named_provider {
  ttx_semantic_query (*source)(named_provider_state*);
  ttx_semantic_query (*destination)(named_provider_state*);
} named_provider;

#endif
