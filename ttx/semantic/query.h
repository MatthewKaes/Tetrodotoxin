// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_QUERY_H
#define TTX_SEMANTIC_QUERY_H

#include "perimortem/system/uuid.h"

#include "ttx/semantic/binding.h"

// Two systems need an agreed entry point before either can ask the other for
// an interface. A module can return this Query from its entry function, or a
// native owner can lend it directly. That enclosing agreement supplies the
// bind thunk and its lifetime, so calling bind does not first require a Flow.
// Systems can therefore begin cooperating without constructing a TTX graph.
//
// Each call asks for one UUID contract and returns its bound operations. The
// consumer can use those operations immediately under the requested contract,
// including establishing a Flow to acquire other data. There is no additional
// permission negotiation inside Query. Binding's status and borrowing rules
// are described in ttx/semantic/binding.h.
//
// Actually accessing `source` inside of TTX is undefined behavior as far as
// Tetrodotoxin is concerned. It can _technically_ be safe but making any
// assumptions about the lifetime or wire format of the source is a quick way to
// get into trouble since sources allow for dynamic substitution under TTX's
// semantic simulacra principle.
//
// The enclosing owner keeps the supplying state and implementation code alive
// through negotiation and every use of the returned bindings. Copying this
// Query borrows that agreement without acquiring another lifetime.
typedef struct ttx_semantic_query {
  const void* source;
  ttx_binding_status (*bind)(
      const void* source,
      perimortem_uuid requested,
      ttx_binding* result);
} ttx_semantic_query;

#endif
