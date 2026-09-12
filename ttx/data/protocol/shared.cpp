// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors
#include "ttx/data/protocol/shared.h"

auto ttx_shared_release(ttx_shared_lifetime* lifetime) -> void {
  // Release enters provider code, which can reenter the owner of this carrier.
  // Remove the old obligation first so that reentry cannot release it twice.
  const auto previous = *lifetime;
  *lifetime = {};
  if (previous.release) {
    previous.release(previous.source);
  }
}
