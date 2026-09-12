// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_PROTOCOL_FRAGMENT_H
#define TTX_DATA_PROTOCOL_FRAGMENT_H

#include "ttx/data/form/representation.h"

// A provider can describe a record without assembling it in memory. Fragment
// requests its individual values through typed thunks at agreed ABI byte
// coordinates. Each getter finishes before returning and supplies a complete
// value on success. It retains no output pointer and schedules no later write.
//
// The representation determines which getters the provider supplies. A typed
// output fixes the extent of each request, so reads cannot ambiguously span
// fields and padding. Pointer observations describe their storage only.
// Each result is a native ABI value, even if the described payload uses another
// byte order. The operation realizes that value in its destination format, so
// the provider never needs to expose a buffer just to return encoded bytes.
//
// Successive reads need not observe a snapshot. An overlapping output can
// affect later observations, leaving the combined reflow result unspecified.
// A stronger observation or execution policy belongs outside this contract.
typedef struct ttx_fragment_view_operations {
  const ttx_representation* (*representation)(const void* source);
} ttx_fragment_view_operations;

typedef struct ttx_fragment_access_operations {
  const ttx_representation* (*representation)(const void* source);
  ttx_data_status (*get_u8)(const void* source, Count position, U8* result);
  ttx_data_status (*get_u16)(const void* source, Count position, U16* result);
  ttx_data_status (*get_u32)(const void* source, Count position, U32* result);
  ttx_data_status (*get_u64)(const void* source, Count position, U64* result);
  ttx_data_status (*get_s8)(const void* source, Count position, S8* result);
  ttx_data_status (*get_s16)(const void* source, Count position, S16* result);
  ttx_data_status (*get_s32)(const void* source, Count position, S32* result);
  ttx_data_status (*get_s64)(const void* source, Count position, S64* result);
  ttx_data_status (*get_r32)(const void* source, Count position, R32* result);
  ttx_data_status (*get_r64)(const void* source, Count position, R64* result);
  ttx_data_status (
      *get_pointer)(const void* source, Count position, void** result);
} ttx_fragment_access_operations;

typedef struct ttx_fragment_view {
  const void* source;
  const ttx_fragment_view_operations* operations;
} ttx_fragment_view;

typedef struct ttx_fragment_access {
  const void* source;
  const ttx_fragment_access_operations* operations;
} ttx_fragment_access;

#endif
