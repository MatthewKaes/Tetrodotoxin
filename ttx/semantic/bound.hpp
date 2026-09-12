// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

namespace Ttx::Semantic {

// A bound view borrows an implementation and its operation table. The table
// belongs to the provider, so its thunks can reach an owned member or read a
// compact representation without making that representation inherit the
// requested contract. Calling an operation needs no further negotiation.
//
// Each contract gives this pair its named query methods. Both the state and
// the code behind the table must outlive every use of the view. The view does
// not acquire a module or extend an Arena's lifetime.
template <typename Operations>
class Bound {
 public:
  constexpr Bound(const void* source, const Operations& operations)
      : source(source), operations(operations) {}

 protected:
  const void* source;
  const Operations& operations;
};

}  // namespace Ttx::Semantic
