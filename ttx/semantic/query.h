// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_QUERY_H
#define TTX_SEMANTIC_QUERY_H

#include "perimortem/system/uuid.h"

#include "ttx/semantic/binding.h"

// Semantic negotiation needs the supplying owner's bind operation to extract
// the actual semantic operations. A Semantic query is the thinnest negotiation
// on top of the TTX flow stack and allows consumer negotiation outside of the
// TTX graph, allowing it to provide its own method of semantic negotation.
//
// This is a critical step to allowing interop between systems loosly embedded
// in the TTX graph as it allows progressive semantic cooporation.
//
// The query surface is extremely light. It only negotiates with the source that
// it can continue negotiations under a binding contract. That TTX layer is then
// used to negotiate a binding contract using a UUID using the `bind` contract.
//
// If the source agrees to the contract it will `ttx_binding` surface can then
// be used for further semantic negotiation.
//
// (read details in ttx/semantic/binding.h)
//
// Actually accessing `source` inside of TTX is undefined behavior as far as
// Tetrodotoxin is concerned. It can _technically_ be safe but making any
// assumptions about the lifetime or wire format of the source is a quick way to
// get into trouble since sources allow for dynamic substitution under TTX's
// semantic simulacra principle.
//
// The caller does need to promise it will keep its state and code alive through
// negotiation and use of the returned bindings. Probing selects operations
// without materializing their data.
typedef struct ttx_semantic_query {
  const void* source;
  ttx_binding_status (*bind)(
      const void* source,
      perimortem_uuid requested,
      ttx_binding* result);
} ttx_semantic_query;

#endif
