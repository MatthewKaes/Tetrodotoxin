// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_DATA_PROTOCOL_SHARED_H
#define TTX_DATA_PROTOCOL_SHARED_H

#include "ttx/data/form/representation.h"

// A provider may be able to expose data in the agreed C format only while it
// holds a resource. Shared lets it lend that payload with one release
// obligation, so the reader can use the pointer without reacquiring the
// resource for each operation. Semantic Flow keeps this lifetime until the Flow
// closes.
//
// Data is the public payload pointer that may be cast under the agreed format.
// Source belongs to the release thunk and remains opaque to the reader. Keeping
// them separate lets an owner release a resource whose internal state has a
// different representation from the data it lent.
typedef struct ttx_shared_lifetime {
  const void* data;
  const void* source;
  void (*release)(const void* source);
} ttx_shared_lifetime;
typedef struct ttx_shared_view_operations {
  const ttx_representation* (*representation)(const void* source);
} ttx_shared_view_operations;
typedef struct ttx_shared_access_operations {
  const ttx_representation* (*representation)(const void* source);
  // Success supplies ready payload data and transfers its release
  // obligation. Failure supplies no lifetime. Acquisition itself is finished
  // before returning, even though the acquired lifetime continues afterward.
  ttx_data_status (*acquire)(const void* source, ttx_shared_lifetime* result);
} ttx_shared_access_operations;
typedef struct ttx_shared_view {
  const void* source;
  const ttx_shared_view_operations* operations;
} ttx_shared_view;
typedef struct ttx_shared_access {
  const void* source;
  const ttx_shared_access_operations* operations;
} ttx_shared_access;

// Clear the caller's carrier before invoking owner code. A release hook can
// reenter its caller without observing or releasing the old obligation again.
PERIMORTEM_C void ttx_shared_release(ttx_shared_lifetime* lifetime);

#endif
