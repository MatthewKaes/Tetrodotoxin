// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors
#include <stddef.h>
#include <string.h>

#include "validation/unit_tests/ttx/semantic/fixtures/provider_representation.h"

#include "validation/unit_tests/ttx/semantic/fixtures/provider.h"

static const ttx_schema u32 = {
  4,
  4,
  TTX_SCHEMA_VALUE,
  {.value = {TTX_SCHEMA_U32, TTX_SCHEMA_LITTLE_ENDIAN}}};
static const ttx_schema four =
    {16, 4, TTX_SCHEMA_RANGE, {.range = {&u32, 4, 4}}};
static const ttx_representation* four_representation;
static const ttx_representation* query_representation;
static const ttx_representation* primitive_representation;

static int matches(perimortem_uuid id, U64 high, U64 low) {
  return id.high == high && id.low == low;
}

static const ttx_representation* schema(const void* source) {
  ++((provider_state*)source)->descriptions;
  return four_representation;
}

static const void* direct(const void* source) {
  return ((const provider_state*)source)->values;
}

static void release(const void* source) {
  provider_state* state = (provider_state*)source;
  state->held = 0;
  ++state->releases;
  if (state->released) {
    state->released(state->observer);
  }

}

// The synchronous accessors borrow their outputs only for the call. No
// provider state stores an output surface or a completion for later use.
static ttx_data_status shared(const void* source, ttx_shared_lifetime* result) {
  provider_state* state = (provider_state*)source;
  ++state->acquires;
  if (state->held) {
    return TTX_DATA_BUSY;
  }

  if (state->failure) {
    return TTX_DATA_IO_ERROR;
  }

  state->held = 1;
  *result = (ttx_shared_lifetime){state->values, state, release};
  return TTX_DATA_SUCCESS;
}

static ttx_data_status block(const void* source, ttx_block_surface surface) {
  provider_state* state = (provider_state*)source;
  ++state->commits;
  if (state->failure) {
    return TTX_DATA_IO_ERROR;
  }

  memcpy(surface.data, state->values, four.extent);
  return TTX_DATA_SUCCESS;
}

static ttx_data_status fragment(const void* source, Count position, U32* result) {
  provider_state* state = (provider_state*)source;
  ++state->reads;
  if (state->failure && state->reads == state->fail_at) {
    return TTX_DATA_IO_ERROR;
  }

  if (position >= four.extent || position % sizeof(U32)) {
    return TTX_DATA_BOUNDS;
  }

  *result = state->values[position / sizeof(U32)];
  if (state->generating) {
    ++state->values[position / sizeof(U32)];
  }

  return TTX_DATA_SUCCESS;
}

static const ttx_direct_access_operations direct_ops = {schema, direct};
static const ttx_shared_access_operations shared_ops = {schema, shared};
static const ttx_block_access_operations block_ops = {schema, block};
static const ttx_fragment_access_operations fragment_ops = {
  .representation = schema,
  .get_u32 = fragment};

static ttx_binding_status
    bind(const void* source, perimortem_uuid id, ttx_binding* result) {
  provider_state* state = (provider_state*)source;
  if (matches(id, TTX_DIRECT_ACCESS_ID_HIGH, TTX_DIRECT_ACCESS_ID_LOW)) {
    ++state->binds[0];
    if (!(state->provides & PROVIDES_DIRECT)) {
      return TTX_BINDING_UNSUPPORTED;
    }

    *result = (ttx_binding){source, &direct_ops};
  } else if (matches(id, TTX_SHARED_ACCESS_ID_HIGH, TTX_SHARED_ACCESS_ID_LOW)) {
    ++state->binds[1];
    if (!(state->provides & PROVIDES_SHARED)) {
      return TTX_BINDING_UNSUPPORTED;
    }

    *result = (ttx_binding){source, &shared_ops};
  } else if (matches(id, TTX_BLOCK_ACCESS_ID_HIGH, TTX_BLOCK_ACCESS_ID_LOW)) {
    ++state->binds[2];
    if (!(state->provides & PROVIDES_BLOCK)) {
      return TTX_BINDING_UNSUPPORTED;
    }

    *result = (ttx_binding){source, &block_ops};
  } else if (
      matches(id, TTX_FRAGMENT_ACCESS_ID_HIGH, TTX_FRAGMENT_ACCESS_ID_LOW)) {
    ++state->binds[3];
    if (!(state->provides & PROVIDES_FRAGMENT)) {
      return TTX_BINDING_UNSUPPORTED;
    }

    *result = (ttx_binding){source, &fragment_ops};
  } else {
    return TTX_BINDING_UNSUPPORTED;
  }

  return TTX_BINDING_SATISFIED;
}

static ttx_semantic_query writer(provider_state* state) {
  return (ttx_semantic_query){state, bind};
}

// The loader already knows this C entry contract. It can obtain the data
// pointer for a stable Query publication, check the agreed Query ABI and use
// that Query's bind thunk. No Fragment or Block implementation is implied.
static const ttx_schema pointer = {
  8,
  8,
  TTX_SCHEMA_VALUE,
  {.value = {TTX_SCHEMA_POINTER, TTX_SCHEMA_LITTLE_ENDIAN}}};
// The entry contract already fixes the Query ABI and its bind signature.
// Data describes only the two pointer slots that Direct will make readable.
static const ttx_schema_position query_fields[] = {
  {&pointer, offsetof(ttx_semantic_query, source)},
  {&pointer, offsetof(ttx_semantic_query, bind)},
};

static const ttx_schema query_schema = {
  sizeof(ttx_semantic_query),
  _Alignof(ttx_semantic_query),
  TTX_SCHEMA_COMPOSITE,
  {.composite = {query_fields, 2}}};
static provider_state static_state = {
  .provides = PROVIDES_DIRECT,
  .values = {1, 2, 3, 4}};
static const ttx_semantic_query published = {&static_state, bind};
static const ttx_representation* bootstrap_schema(const void* source) {
  (void)source;
  return query_representation;
}

static const void* bootstrap_pointer(const void* source) {
  (void)source;
  return &published;
}

static const ttx_direct_access_operations bootstrap_ops = {
  bootstrap_schema, bootstrap_pointer};
static ttx_binding_status bootstrap_bind(
    const void* source,
    perimortem_uuid id,
    ttx_binding* result) {
  if (!matches(id, TTX_DIRECT_ACCESS_ID_HIGH, TTX_DIRECT_ACCESS_ID_LOW)) {
    return TTX_BINDING_UNSUPPORTED;
  }

  *result = (ttx_binding){source, &bootstrap_ops};
  return TTX_BINDING_SATISFIED;
}

static ttx_semantic_query bootstrap_writer(void) {
  return (ttx_semantic_query){NULL, bootstrap_bind};
}

// This policy selects position three, then position zero. The provider
// supplies no selected values here. Its caller can run this same mapping
// against a Direct, Shared or Fragment Flow established independently.
static const ttx_schema two =
    {8, 4, TTX_SCHEMA_RANGE, {.range = {&u32, 2, 4}}};
static Count select_position(const void* source, Count output) {
  (void)source;
  return output ? 0 : 12;
}

static ttx_swizzle_selection selection_policy;
static ttx_swizzle_mapping selection_mapping;
static const ttx_representation_position selection_outputs[] = {
  {0, TTX_SCHEMA_U32, TTX_SCHEMA_LITTLE_ENDIAN},
  {4, TTX_SCHEMA_U32, TTX_SCHEMA_LITTLE_ENDIAN},
};
static const ttx_swizzle_group selection_groups[] = {
  {{12, TTX_SCHEMA_U32, TTX_SCHEMA_LITTLE_ENDIAN}, &selection_outputs[0], 1},
  {{0, TTX_SCHEMA_U32, TTX_SCHEMA_LITTLE_ENDIAN}, &selection_outputs[1], 1},
};
static const ttx_swizzle_selection* selection(void) {
  return &selection_policy;
}

static ttx_data_status select_values(
    const provider_operations* operations,
    const ttx_flow* flow,
    ttx_storage target) {
  return operations->swizzle(flow, selection_mapping, target);
}

// This fixture crosses every primitive result ABI, including signed and
// floating register classes. No whole record exists behind these getters.
static const ttx_schema primitive_u8 = {
  sizeof(U8),
  _Alignof(U8),
  TTX_SCHEMA_VALUE,
  {.value = {TTX_SCHEMA_U8, TTX_SCHEMA_LITTLE_ENDIAN}}};
static ttx_data_status primitive_get_u8(
    const void* source, Count position, U8* result) {
  (void)source;
  if (position != offsetof(provider_values, u8)) {
    return TTX_DATA_BOUNDS;
  }

  *result = (U8)(251);
  return TTX_DATA_SUCCESS;
}

static const ttx_schema primitive_u16 = {
  sizeof(U16),
  _Alignof(U16),
  TTX_SCHEMA_VALUE,
  {.value = {TTX_SCHEMA_U16, TTX_SCHEMA_LITTLE_ENDIAN}}};
static ttx_data_status primitive_get_u16(
    const void* source, Count position, U16* result) {
  (void)source;
  if (position != offsetof(provider_values, u16)) {
    return TTX_DATA_BOUNDS;
  }

  *result = (U16)(513);
  return TTX_DATA_SUCCESS;
}

static const ttx_schema primitive_u32 = {
  sizeof(U32),
  _Alignof(U32),
  TTX_SCHEMA_VALUE,
  {.value = {TTX_SCHEMA_U32, TTX_SCHEMA_LITTLE_ENDIAN}}};
static ttx_data_status primitive_get_u32(
    const void* source, Count position, U32* result) {
  (void)source;
  if (position != offsetof(provider_values, u32)) {
    return TTX_DATA_BOUNDS;
  }

  *result = (U32)(1234567);
  return TTX_DATA_SUCCESS;
}

static const ttx_schema primitive_u64 = {
  sizeof(U64),
  _Alignof(U64),
  TTX_SCHEMA_VALUE,
  {.value = {TTX_SCHEMA_U64, TTX_SCHEMA_LITTLE_ENDIAN}}};
static ttx_data_status primitive_get_u64(
    const void* source, Count position, U64* result) {
  (void)source;
  if (position != offsetof(provider_values, u64)) {
    return TTX_DATA_BOUNDS;
  }

  *result = (U64)(0xfedcba9876543210ULL);
  return TTX_DATA_SUCCESS;
}

static const ttx_schema primitive_s8 = {
  sizeof(S8),
  _Alignof(S8),
  TTX_SCHEMA_VALUE,
  {.value = {TTX_SCHEMA_S8, TTX_SCHEMA_LITTLE_ENDIAN}}};
static ttx_data_status primitive_get_s8(
    const void* source, Count position, S8* result) {
  (void)source;
  if (position != offsetof(provider_values, s8)) {
    return TTX_DATA_BOUNDS;
  }

  *result = (S8)(-12);
  return TTX_DATA_SUCCESS;
}

static const ttx_schema primitive_s16 = {
  sizeof(S16),
  _Alignof(S16),
  TTX_SCHEMA_VALUE,
  {.value = {TTX_SCHEMA_S16, TTX_SCHEMA_LITTLE_ENDIAN}}};
static ttx_data_status primitive_get_s16(
    const void* source, Count position, S16* result) {
  (void)source;
  if (position != offsetof(provider_values, s16)) {
    return TTX_DATA_BOUNDS;
  }

  *result = (S16)(-1234);
  return TTX_DATA_SUCCESS;
}

static const ttx_schema primitive_s32 = {
  sizeof(S32),
  _Alignof(S32),
  TTX_SCHEMA_VALUE,
  {.value = {TTX_SCHEMA_S32, TTX_SCHEMA_LITTLE_ENDIAN}}};
static ttx_data_status primitive_get_s32(
    const void* source, Count position, S32* result) {
  (void)source;
  if (position != offsetof(provider_values, s32)) {
    return TTX_DATA_BOUNDS;
  }

  *result = (S32)(-123456);
  return TTX_DATA_SUCCESS;
}

static const ttx_schema primitive_s64 = {
  sizeof(S64),
  _Alignof(S64),
  TTX_SCHEMA_VALUE,
  {.value = {TTX_SCHEMA_S64, TTX_SCHEMA_LITTLE_ENDIAN}}};
static ttx_data_status primitive_get_s64(
    const void* source, Count position, S64* result) {
  (void)source;
  if (position != offsetof(provider_values, s64)) {
    return TTX_DATA_BOUNDS;
  }

  *result = (S64)(-123456789012LL);
  return TTX_DATA_SUCCESS;
}

static const ttx_schema primitive_r32 = {
  sizeof(R32),
  _Alignof(R32),
  TTX_SCHEMA_VALUE,
  {.value = {TTX_SCHEMA_R32, TTX_SCHEMA_LITTLE_ENDIAN}}};
static ttx_data_status primitive_get_r32(
    const void* source, Count position, R32* result) {
  (void)source;
  if (position != offsetof(provider_values, r32)) {
    return TTX_DATA_BOUNDS;
  }

  *result = (R32)(1.25f);
  return TTX_DATA_SUCCESS;
}

static const ttx_schema primitive_r64 = {
  sizeof(R64),
  _Alignof(R64),
  TTX_SCHEMA_VALUE,
  {.value = {TTX_SCHEMA_R64, TTX_SCHEMA_LITTLE_ENDIAN}}};
static ttx_data_status primitive_get_r64(
    const void* source, Count position, R64* result) {
  (void)source;
  if (position != offsetof(provider_values, r64)) {
    return TTX_DATA_BOUNDS;
  }

  *result = (R64)(-2.5);
  return TTX_DATA_SUCCESS;
}

static const ttx_schema primitive_pointer = {
  sizeof(U64),
  _Alignof(U64),
  TTX_SCHEMA_VALUE,
  {.value = {TTX_SCHEMA_POINTER, TTX_SCHEMA_LITTLE_ENDIAN}}};
static ttx_data_status primitive_get_pointer(
    const void* source, Count position, void** result) {
  (void)source;
  if (position != offsetof(provider_values, pointer)) {
    return TTX_DATA_BOUNDS;
  }

  static U32 marker = 42;
  *result = &marker;
  return TTX_DATA_SUCCESS;
}

static const ttx_schema_position primitive_fields[] = {
  {&primitive_u8, offsetof(provider_values, u8)},
  {&primitive_u16, offsetof(provider_values, u16)},
  {&primitive_u32, offsetof(provider_values, u32)},
  {&primitive_u64, offsetof(provider_values, u64)},
  {&primitive_s8, offsetof(provider_values, s8)},
  {&primitive_s16, offsetof(provider_values, s16)},
  {&primitive_s32, offsetof(provider_values, s32)},
  {&primitive_s64, offsetof(provider_values, s64)},
  {&primitive_r32, offsetof(provider_values, r32)},
  {&primitive_r64, offsetof(provider_values, r64)},
  {&primitive_pointer, offsetof(provider_values, pointer)},
};

static const ttx_schema primitive_record = {
  sizeof(provider_values),
  _Alignof(provider_values),
  TTX_SCHEMA_COMPOSITE,
  {.composite = {primitive_fields, 11}}};
static const ttx_representation* primitive_schema(void) {
  return primitive_representation;
}

static const ttx_representation* primitive_describe(const void* source) {
  (void)source;
  return primitive_representation;
}

static const ttx_fragment_access_operations primitive_access = {
  .representation = primitive_describe,
  .get_u8 = primitive_get_u8,
  .get_u16 = primitive_get_u16,
  .get_u32 = primitive_get_u32,
  .get_u64 = primitive_get_u64,
  .get_s8 = primitive_get_s8,
  .get_s16 = primitive_get_s16,
  .get_s32 = primitive_get_s32,
  .get_s64 = primitive_get_s64,
  .get_r32 = primitive_get_r32,
  .get_r64 = primitive_get_r64,
  .get_pointer = primitive_get_pointer,
};

static ttx_binding_status primitive_bind(
    const void* source,
    perimortem_uuid id,
    ttx_binding* result) {
  if (!matches(id, TTX_FRAGMENT_ACCESS_ID_HIGH, TTX_FRAGMENT_ACCESS_ID_LOW)) {
    return TTX_BINDING_UNSUPPORTED;
  }

  *result = (ttx_binding){source, &primitive_access};
  return TTX_BINDING_SATISFIED;
}

static ttx_semantic_query primitives(void) {
  return (ttx_semantic_query){NULL, primitive_bind};
}

// This retained table uses the old callback ABI and its old identity. The
// synchronous host must decline it rather than reinterpret its commit slot.
typedef struct legacy_completion {
  void* receiver;
  void (*complete)(void*, ttx_data_status);
} legacy_completion;

typedef struct legacy_block_operations {
  const ttx_schema* (*schema)(const void*);
  void (*commit)(const void*, ttx_block_surface, legacy_completion);
} legacy_block_operations;

static void legacy_commit(
    const void* source,
    ttx_block_surface surface,
    legacy_completion reply) {
  reply.complete(reply.receiver, block(source, surface));
}

static ttx_binding_status legacy_bind(
    const void* source,
    perimortem_uuid id,
    ttx_binding* result) {
  static const legacy_block_operations operations = {NULL, legacy_commit};
  if (!matches(id, 0x51214d6ff9654e48ULL, 0x886b7c136306884aULL)) {
    return TTX_BINDING_UNSUPPORTED;
  }

  *result = (ttx_binding){source, &operations};
  return TTX_BINDING_SATISFIED;
}

static ttx_semantic_query legacy_writer(provider_state* state) {
  return (ttx_semantic_query){state, legacy_bind};
}

const provider_api* flow_provider_open(provider_compile compiler) {
  compile_representation = compiler;
  if (!four_representation) {
    four_representation = prepare_representation(&four);
    query_representation = prepare_representation(&query_schema);
    primitive_representation = prepare_representation(&primitive_record);
    const ttx_representation* pair = prepare_representation(&two);
    selection_policy = (ttx_swizzle_selection){
      four_representation, pair, NULL, select_position};
    selection_mapping = (ttx_swizzle_mapping){
      four_representation, pair, selection_groups, 2};
  }

  static const provider_api api = {
    writer, bootstrap_writer, selection, primitives, primitive_schema,
    select_values, legacy_writer};
  return &api;
}
