// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_FLOW_H
#define TTX_SEMANTIC_FLOW_H

#include "ttx/data/form/representation.h"
#include "ttx/semantic/query.h"

typedef U8 ttx_flow_status;
#define TTX_FLOW_REJECTED ((ttx_flow_status)32)
#define TTX_FLOW_BINDING_PENDING ((ttx_flow_status)33)

// Flow establishes one synchronous access agreement between already
// bootstrapped Queries. Direct, Shared, Block and Fragment are independent
// contracts tried in that preference order. The first compatible pair wins,
// and the host retains only that protocol's state for later operations.
//
// The reader needs no target Storage during establishment. Each operation can
// supply its own storage without negotiating again. The opaque handle keeps
// the host's local protocol union out of the foreign module's C
// representation.
typedef struct ttx_flow ttx_flow;

// Knowing the required representation is enough to establish access. This
// reader policy accepts all four Data protocols while operations supply their
// own storage later. The query and its Flows borrow the representation, so
// its owner
// keeps that description alive through every use of the agreement.
PERIMORTEM_C ttx_semantic_query ttx_flow_reader(const ttx_representation* representation);

// The host supplies a fresh or closed Flow. Success means its representation
// or operations are ready before this call returns. Shared additionally lends
// one lifetime that remains acquired until Flow closes.
//
// BindingPending reports an unresolved bind attempt, not work continuing
// after
// return. Pending or rejected binding stops negotiation, and acquisition
// failure
// does not backtrack after selecting a protocol. Endpoint state,
// representations and
// implementation code remain borrowed throughout every subsequent Flow use.
PERIMORTEM_C ttx_flow_status ttx_flow_connect(
    ttx_flow* flow,
    ttx_semantic_query reader,
    ttx_semantic_query writer);

#endif
