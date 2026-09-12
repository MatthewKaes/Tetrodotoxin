// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "validation/unit_tests/ttx/semantic/measurement.hpp"

#include <stddef.h>

using Validation::FlowTests::Measurement;

Measurement* Measurement::active = nullptr;
Measurement::Measurement() : previous(active) {
  active = this;
}
Measurement::~Measurement() {
  stop();
}
auto Measurement::stop() -> void {
  if (active == this) {
    active = previous;
  }
}
auto Measurement::allocation() -> void {
  for (auto* interval = active; interval; interval = interval->previous) {
    ++interval->allocations;
  }
}
auto Measurement::copy() -> void {
  for (auto* interval = active; interval; interval = interval->previous) {
    ++interval->copies;
  }
}

//
// WARNING: The below is compiler jank and only sanctioned for test use!
//
// This measurement system is one of the reasons why the TTX Data tests are
// linked separately so they don't contaiminate the rest of the tests.
//

extern "C" {
void* __real_memmove(void*, const void*, size_t);
void* __wrap_memmove(void* destination, const void* source, size_t size) {
  Measurement::copy();
  return __real_memmove(destination, source, size);
}
#if __has_feature(address_sanitizer)
// ASan rewrites memory intrinsics before ordinary wrapping. Observe that entry
// too so a sanitizer run checks the same copy count instead of skipping it.
void* __real___asan_memmove(void*, const void*, size_t);
void* __wrap___asan_memmove(
    void* destination,
    const void* source,
    size_t size) {
  Measurement::copy();
  return __real___asan_memmove(destination, source, size);
}
#endif
void* __real_malloc(size_t);
void* __real_calloc(size_t, size_t);
void* __real_realloc(void*, size_t);
void* __real_aligned_alloc(size_t, size_t);
int __real_posix_memalign(void**, size_t, size_t);
void* __wrap_malloc(size_t size) {
  Measurement::allocation();
  return __real_malloc(size);
}
void* __wrap_calloc(size_t count, size_t size) {
  Measurement::allocation();
  return __real_calloc(count, size);
}
void* __wrap_realloc(void* pointer, size_t size) {
  Measurement::allocation();
  return __real_realloc(pointer, size);
}
void* __wrap_aligned_alloc(size_t alignment, size_t size) {
  Measurement::allocation();
  return __real_aligned_alloc(alignment, size);
}
int __wrap_posix_memalign(void** pointer, size_t alignment, size_t size) {
  Measurement::allocation();
  return __real_posix_memalign(pointer, alignment, size);
}
}
