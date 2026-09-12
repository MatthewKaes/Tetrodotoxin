// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef VALIDATION_DATA_PROVIDER_H
#define VALIDATION_DATA_PROVIDER_H

#include "ttx/semantic/block.h"
#include "ttx/semantic/direct.h"
#include "ttx/semantic/fragment.h"
#include "ttx/semantic/query.h"
#include "ttx/semantic/shared.h"
#include "ttx/semantic/flows/swizzle.h"

// Fixture flags describe explicit implementations, not inherited abilities.
#define PROVIDES_DIRECT 1
#define PROVIDES_SHARED 2
#define PROVIDES_BLOCK 4
#define PROVIDES_FRAGMENT 8

typedef struct provider_state {
  U8 provides;
  U8 failure;
  U8 held;
  Count binds[4];
  Count descriptions;
  Count reads;
  Count commits;
  Count acquires;
  Count releases;
  Count fail_at;
  U32 values[4];
  void* observer;
  void (*released)(void*);
} provider_state;

typedef struct provider_values {
  U8 u8;
  U16 u16;
  U32 u32;
  U64 u64;
  S8 s8;
  S16 s16;
  S32 s32;
  S64 s64;
  R32 r32;
  R64 r64;
  void* pointer;
  U32 (*function)(U32);
} provider_values;

typedef struct provider_operations {
  ttx_swizzle_result (*swizzle)(const ttx_flow*, ttx_swizzle_mapping, ttx_storage);
} provider_operations;

typedef struct provider_api {
  ttx_semantic_query (*writer)(provider_state*);
  ttx_semantic_query (*bootstrap_writer)(void);
  const ttx_swizzle_mapping* (*selection)(void);
  ttx_semantic_query (*primitives)(void);
  const ttx_representation* (*primitive_schema)(void);
  ttx_swizzle_result (*select)(const provider_operations*, const ttx_flow*, ttx_storage);
  ttx_semantic_query (*legacy_writer)(provider_state*);
} provider_api;

#endif
