// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_CONCEPT_ABSTRACT_H
#define TTX_CONCEPT_ABSTRACT_H

#include "perimortem/core/perimortem.h"
#include "perimortem/system/uuid.h"
#include "ttx/semantic/binding.h"
#include "ttx/concept/documentation.h"

#define TTX_ABSTRACT_ID_HIGH 0x01a08522d86b7decULL
#define TTX_ABSTRACT_ID_LOW 0x9fa65e9c70720964ULL

#ifdef __cplusplus
extern "C" {
#endif

// A graph can contain providers that have no native C++ Abstract base. Keeping
// the receiver separate from its operations lets those providers answer from
// their own storage, without allocating an adapter object for every graph
// edge. Consumers traverse the supplied operations instead of depending on
// the language or class that implements the subject.
//
// The supplying boundary establishes the exact Abstract contract before the
// table is used, since an unknown table cannot safely describe how to call
// itself. That agreement includes all six operations, the documentation view,
// and the synchronous visitor. This representation uses the platform C ABI and
// has been exercised on Linux with 64 bit pointers.
//
// The pair borrows its provider so graph owners can manage many views under
// one lifetime. Source is nonnull and identifies the subject for that promised
// lifetime, but retaining the token alone keeps neither state nor code alive.
// The caller retains their lifetime owner, including the providers needed by
// returned handles.
typedef struct ttx_abstract {
  const void* source;
  const struct ttx_abstract_ops* operations;
} ttx_abstract;

// A provider can enumerate its own storage directly when the receiver is
// borrowed for just this call. Names need only survive receive, while values
// keep their ordinary provider lifetime. A consumer that sorts or retains
// names copies them during the callback. The provider finishes all callbacks
// before visitation returns and cannot retain this receiver for later work.
typedef struct ttx_concept_visitor {
  void* source;
  void (*receive)(void* source, perimortem_view_bytes name, ttx_abstract value);
} ttx_concept_visitor;

typedef struct ttx_abstract_ops {
  // Separating selection from value production lets a consumer obtain an
  // interface without triggering its computation or data transfers. Bind
  // therefore selects an implementation without invoking its value operations.
  // Result points to writable caller storage and is written only for Satisfied.
  // An Abstract request returns this same pair because the supplying boundary
  // has already established that contract.
  ttx_binding_status (*bind)(const void* source, perimortem_uuid requested,
                             ttx_binding* result);

  // Borrowed names and documentation let tools inspect the subject without
  // building a separate metadata object. The name remains readable for this
  // Abstract's promised lifetime, and documentation carries its own line view.
  perimortem_view_bytes (*get_name)(const void* source);
  ttx_documentation (*get_documentation)(const void* source);

  // The selected subject brings its own operation table, allowing resolution
  // to cross between different provider representations. It satisfies this
  // same Abstract contract, so the consumer can continue without recovering
  // a native class or reinterpreting the original receiver's storage.
  ttx_abstract (*resolve)(const void* source);

  // Lookup and discovery belong to the same subject, so a consumer does not
  // negotiate a different authority merely to follow a named edge. Visitation
  // advertises the owner's visible answers and gives their order no meaning.
  // The receiver must not invalidate the traversed state during a callback.
  ttx_abstract (*resolve_concept)(const void* source, perimortem_view_bytes name);
  void (*visit_concepts)(const void* source, ttx_concept_visitor visitor);
} ttx_abstract_ops;

#ifdef __cplusplus
}
#endif

#endif
