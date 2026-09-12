// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors
#include <stddef.h>

#include "validation/unit_tests/ttx/semantic/fixtures/provider_representation.h"

#include "ttx/semantic/fragment.h"
#include "validation/unit_tests/ttx/semantic/fixtures/heterogeneous_provider.h"

typedef struct record {
  U16 tag;
  R64 energy;
  U32 frame;
} record;
static const ttx_schema tag = {
  2,
  2,
  TTX_SCHEMA_VALUE,
  {.value = {TTX_SCHEMA_U16, TTX_SCHEMA_LITTLE_ENDIAN}}};
static const ttx_schema energy = {
  8,
  8,
  TTX_SCHEMA_VALUE,
  {.value = {TTX_SCHEMA_R64, TTX_SCHEMA_LITTLE_ENDIAN}}};
static const ttx_schema frame = {
  4,
  4,
  TTX_SCHEMA_VALUE,
  {.value = {TTX_SCHEMA_U32, TTX_SCHEMA_LITTLE_ENDIAN}}};
static const ttx_schema_position fields[] = {
  {&tag, offsetof(record, tag)},
  {&energy, offsetof(record, energy)},
  {&frame, offsetof(record, frame)},
};

static const ttx_schema schema = {
  sizeof(record),
  _Alignof(record),
  TTX_SCHEMA_COMPOSITE,
  {.composite = {fields, 3}}};
static const ttx_representation* prepared;

static const ttx_representation* describe(const void* source) {
  ++((heterogeneous_state*)source)->descriptions;
  return prepared;
}

static ttx_data_status get_tag(const void* source, Count position, U16* result) {
  heterogeneous_state* state = (heterogeneous_state*)source;
  ++state->reads;
  if (position != offsetof(record, tag)) {
    return TTX_DATA_BOUNDS;
  }

  *result = (U16)(state->seed + 1);
  return TTX_DATA_SUCCESS;
}

static ttx_data_status get_energy(const void* source, Count position, R64* result) {
  heterogeneous_state* state = (heterogeneous_state*)source;
  ++state->reads;
  if (position != offsetof(record, energy)) {
    return TTX_DATA_BOUNDS;
  }

  *result = state->seed * 0.25;
  return TTX_DATA_SUCCESS;
}

static ttx_data_status get_frame(const void* source, Count position, U32* result) {
  heterogeneous_state* state = (heterogeneous_state*)source;
  ++state->reads;
  if (position != offsetof(record, frame)) {
    return TTX_DATA_BOUNDS;
  }

  *result = state->seed * 100;
  return TTX_DATA_SUCCESS;
}

static const ttx_fragment_access_operations access = {
  .representation = describe,
  .get_u16 = get_tag,
  .get_u32 = get_frame,
  .get_r64 = get_energy};
static const ttx_fragment_view_operations view = {describe};
static ttx_binding_status
    writer_bind(const void* source, perimortem_uuid id, ttx_binding* result) {
  if (id.high != TTX_FRAGMENT_ACCESS_ID_HIGH ||
      id.low != TTX_FRAGMENT_ACCESS_ID_LOW) {
    return TTX_BINDING_UNSUPPORTED;
  }

  *result = (ttx_binding){source, &access};
  return TTX_BINDING_SATISFIED;
}

static ttx_binding_status
    reader_bind(const void* source, perimortem_uuid id, ttx_binding* result) {
  if (id.high != TTX_FRAGMENT_VIEW_ID_HIGH ||
      id.low != TTX_FRAGMENT_VIEW_ID_LOW) {
    return TTX_BINDING_UNSUPPORTED;
  }

  *result = (ttx_binding){source, &view};
  return TTX_BINDING_SATISFIED;
}

static ttx_semantic_query source(heterogeneous_state* state) {
  return (ttx_semantic_query){state, writer_bind};
}

static ttx_semantic_query destination(heterogeneous_state* state) {
  return (ttx_semantic_query){state, reader_bind};
}

const heterogeneous_provider* heterogeneous_provider_open(provider_compile compiler) {
  compile_representation = compiler;
  if (!prepared) {
    prepared = prepare_representation(&schema);
  }

  static const heterogeneous_provider provider = {source, destination};
  return &provider;
}
