// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef VALIDATION_PROVIDER_REPRESENTATION_H
#define VALIDATION_PROVIDER_REPRESENTATION_H

#include <stdlib.h>

#include "ttx/data/form/representation.h"

// Each C fixture compiles its descriptions once when the module opens. This
// bounded output region lives with the loaded module, so every returned thunk
// and Representation shares that lifetime. Access callbacks perform no further
// preparation, and unloading the module retires the whole publication.
// The host supplies the compiler through the entry contract, letting this C
// module use its own storage without linking a second supporting runtime.
static _Alignas(8) U8 representation_storage[32768];
static Count representation_usage;

typedef ttx_data_status (*provider_compile)(
    const ttx_schema*, ttx_representation_allocator,
    const ttx_representation**);
static provider_compile compile_representation;

static void* representation_allocate(void* owner, Count size, Count alignment) {
  (void)owner;
  const Count start = (representation_usage + alignment - 1) & ~(alignment - 1);
  if (size > sizeof(representation_storage) - start) {
    abort();
  }

  representation_usage = start + size;
  return representation_storage + start;
}

static const ttx_representation* prepare_representation(const ttx_schema* source) {
  const ttx_representation* result;
  const ttx_representation_allocator output = {NULL, representation_allocate};
  if (compile_representation(source, output, &result) != TTX_DATA_SUCCESS) {
    abort();
  }

  return result;
}

#endif
