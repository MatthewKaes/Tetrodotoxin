// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#ifndef TTX_SEMANTIC_BINDING_H
#define TTX_SEMANTIC_BINDING_H

#include "perimortem/core/perimortem.h"

// Two providers can answer the same questions using entirely different
// storage. If a consumer inspected source, every substitute would have to
// reproduce that storage layout as well as the answers. Binding keeps the
// receiver opaque so the provider's thunks can interpret their own state and
// change its representation without changing the consumer.
//
// The requested contract defines the operation table and its calling
// agreement, but gives consumers no object layout or byte order for source.
// Consumer access to a presumed representation behind source is undefined
// behavior under this contract. A stateless handler may use null source when
// its contract permits it. The operation table is always nonnull.
//
// A consumer that needs a fixed byte representation can negotiate a separate
// operation whose thunk supplies a pointer to that format. The operation's
// contract specifies the layout, extent, alignment, byte order, access rights,
// and lifetime of those bytes. This grants access to the returned representation
// without granting permission to interpret source. Binding selects the thunk,
// while invoking it may force the provider to materialize the requested form.
//
// Exposing such a format is strongly discouraged for a general interface.
// Every provider offering it must supply that representation, even when its
// own storage is compressed, computed, or held on a device. The promise can
// therefore require allocations or transfers that ordinary question answering
// would avoid, and excludes substitutes unable to supply that form. It can be
// worth that constraint when an external API or a direct memory consumer needs
// the actual bytes, but the materialization cost belongs to that explicit
// operation rather than to ordinary binding.
//
// Once selected, this pair lets the consumer call those operations without
// negotiating again. It borrows the state, table, and implementation code so
// an existing owner can retain their lifetime for a whole group of views.
// Copying or returning a binding does not retain that owner. The owner must
// remain alive for every use, including any work that still needs its code.
typedef struct ttx_binding {
  const void* source;
  const void* operations;
} ttx_binding;

// A binding request may pass through several policy layers. A layer that does
// not supply an implementation can leave the question to the next policy, but
// an unfinished answer or a rejection must stop there. Treating either as
// unsupported could select an implementation that the preceding policy would
// not permit. The status keeps these decisions distinct in a single byte:
//
// 0: Binding satisfied and a valid binding was produced.
// 1: The request is unsupported here, so forwarding may continue.
// 2: The binding cannot yet be determined, so forwarding stops.
// 3: The binding was rejected, so forwarding stops.
//
// Keeping failure in the status lets a successful binding contain only the
// state and operations needed by its caller. Bind writes that output only on
// success and leaves it untouched on failure, so callers inspect the status
// before using the pair. Values outside these four states are invalid and the
// receiving facade rejects them.
typedef U8 ttx_binding_status;

#define TTX_BINDING_SATISFIED ((ttx_binding_status)0)
#define TTX_BINDING_UNSUPPORTED ((ttx_binding_status)1)
#define TTX_BINDING_PENDING ((ttx_binding_status)2)
#define TTX_BINDING_REJECTED ((ttx_binding_status)3)

#endif
