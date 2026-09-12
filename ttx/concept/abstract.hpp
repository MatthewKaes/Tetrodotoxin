// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/bytes.hpp"
#include "perimortem/core/option.hpp"

#include "perimortem/system/uuid.hpp"

#include "perimortem/utility/result.hpp"

#include "ttx/concept/abstract.h"
#include "ttx/concept/documentation.hpp"
#include "ttx/concept/type_identity.hpp"
#include "ttx/concept/visitor.hpp"
#include "ttx/semantic/binding.hpp"

namespace Ttx::Concept {

// Abstract is the native authoring surface for a shared semantic identity.
// Native selection proves a C++ base subobject, while binding supplies the
// operations an encountered policy offers. Keeping those questions separate
// lets existing C++ owners retain their direct representation and lets another
// provider answer the same queries without inheriting their classes.
//
// Lookup follows the language route one name at a time. The selected object
// answers from the context it owns, so Packages and Dialects can compose
// without a global member registry.
//
// Construction enriches these same objects as more context becomes available.
// An identity that has already answered successfully stays stable, giving
// editors, compilers, and runtimes one graph to share throughout completion.
class Abstract {
 public:
  static constexpr Perimortem::System::Uuid contract_id{
    TTX_ABSTRACT_ID_HIGH,
    TTX_ABSTRACT_ID_LOW,
  };

  // Graph edges can cross language and ABI lines meaning they may not leverage
  // the C++ Abstract base directly. This view lets an owner expose such an edge
  // directly from its own storage using thunks.
  //
  // Life time is borrowed and the identity belongs to the stable state promised
  // for the life time of the transaction. Resolve and operations are open to
  // select another view without destroying the policy carried by this one as
  // long as the parent keeps it alive.
  //
  // The main reason we use the thunk pattern is it gives us two distinct
  // advantages:
  //
  // * Use the host's language system to perform static optimizations, as long
  //   as they preserve policy.
  // * Binding only selects the implementation and nothing is materialized until
  //   any of its operations are invoked.
  //
  // This allows the Abstracts like a CUDA image to offer pixel level access
  // operations without requiring transfering the pixels to the CPU simply to
  // establish binding.
  class Handle {
   public:
    using Operations = ttx_abstract_ops;

    constexpr Handle(const void* source, const Operations& operations)
        : source(source), operations(&operations) {}

    explicit constexpr Handle(ttx_abstract value)
        : source(value.source), operations(value.operations) {}

    Handle(const Abstract& source) : Handle(source.get_interface()) {}

    constexpr auto get_abi() const -> ttx_abstract {
      return {source, operations};
    }

    template <typename Contract>
    auto bind() const -> Perimortem::Utility::
        Result<typename Contract::Handle, Semantic::Binding::Failure> {
      // First see if we can resolve the question staticly with C++'s native
      // type system.
      if constexpr (__is_same(Contract, Abstract)) {
        return *this;
      }

      // If not then we use a dynamic dispatch from the bind to try and extract
      // a workable contract.
      ttx_binding result = {};
      const auto status =
          operations->bind(source, Contract::contract_id.get_value(), &result);
      switch (status) {
      case TTX_BINDING_SATISFIED:
        if (result.operations) {
          return Semantic::Binding(result).template get<Contract>();
        }
        return Semantic::Binding::Failure::Rejected;
      case TTX_BINDING_UNSUPPORTED:
        return Semantic::Binding::Failure::Unsupported;
      case TTX_BINDING_PENDING:
        return Semantic::Binding::Failure::Pending;
      default:
        return Semantic::Binding::Failure::Rejected;
      }
    }

    auto get_name() const -> Perimortem::Core::View::Bytes {
      const auto name = operations->get_name(source);
      return {name.data, name.size};
    }

    auto get_documentation() const -> Documentation::Handle {
      return Documentation::Handle(operations->get_documentation(source));
    }

    auto resolve() const -> Handle {
      return Handle(operations->resolve(source));
    }

    using Visitor = Concept::Visitor<Handle>;

    auto resolve_concept(Perimortem::Core::View::Bytes name) const -> Handle {
      return Handle(operations->resolve_concept(
          source, {name.get_data(), name.get_size()}));
    }

    auto visit_concepts(Visitor visitor) const -> void {
      const ttx_concept_visitor receiver = {
        &visitor,
        [](void* source, perimortem_view_bytes name, ttx_abstract value) {
          (*static_cast<Visitor*>(source))(
              {name.data, name.size}, Handle(value));
        },
      };
      operations->visit_concepts(source, receiver);
    }

    // This token identifies an encountered policy during one observation.
    // Package assigns its own durable identities after gathering the edges.
    auto get_identity() const -> const void* { return source; }

   private:
    const void* source;
    const Operations* operations;
  };

  using Operations = Handle::Operations;

  // Provides a Handle to the Abstract allowing it be provided for binding
  // across compilation units.
  auto get_interface() const -> Handle;

  using ClassCatagory = Abstract;

  constexpr virtual ~Abstract() = default;

  // Binding attempts to resolve the Abstract's interface representation through
  // a contract.
  //
  // Native selection remains a separate question about a C++ base subobject and
  // cannot stand in for this operation.
  template <typename Contract>
  auto bind() const -> Perimortem::Utility::
      Result<typename Contract::Handle, Semantic::Binding::Failure> {
    return get_interface().template bind<Contract>();
  }

  virtual auto bind_interface(Perimortem::System::Uuid requested) const
      -> Perimortem::Utility::
          Result<Semantic::Binding, Semantic::Binding::Failure>;

  // Proves a native C++ base relationship without RTTI. These local tokens
  // remain separate from the UUIDs used to negotiate operation tables across
  // providers. Matching a public contract does not establish this native proof.
  //
  // A native implementation may return true only for public C++ base contracts,
  // each represented by one unique accessible base subobject. This invariant
  // makes visitor dispatch well defined.
  virtual constexpr auto implements(::U64 requested) const -> Bool {
    return requested == get_type_identity<Abstract>();
  }

  template <typename Requested>
  constexpr auto is() const -> Bool {
    static_assert(
        __is_base_of(Abstract, Requested),
        "A requested TTX contract must derive from Abstract.");
    static_assert(
        __is_same(Requested, typename Requested::ClassCatagory),
        "Only declared TTX contracts can be queried.");
    return implements(get_type_identity<Requested>());
  }

  // Returns the proven contract as one borrowed reference. Absence preserves
  // the same mismatch result as is() without making every caller rebuild the
  // identical visit pair merely to retain the selected object.
  template <typename Requested>
  constexpr auto select() -> Perimortem::Core::Option<Requested&> {
    if (!is<Requested>()) {
      return {};
    }

    return static_cast<Requested&>(*this);
  }

  template <typename Requested>
  constexpr auto select() const -> Perimortem::Core::Option<const Requested&> {
    if (!is<Requested>()) {
      return {};
    }

    return static_cast<const Requested&>(*this);
  }

  // Dispatches one proven public contract without exposing an unchecked
  // narrowed reference. A successful match receives the real Requested
  // object. A mismatch receives this exact Abstract so the caller can preserve
  // identity, report context, or continue through another query.
  //
  // The callbacks own the result of the operation. visit() only selects which
  // callback runs and forwards that callback's result.
  template <typename Requested, typename MatchVisitor, typename MismatchVisitor>
  constexpr auto visit(
      MatchVisitor match_visitor,
      MismatchVisitor mismatch_visitor) -> decltype(auto) {
    if (is<Requested>()) {
      return match_visitor(static_cast<Requested&>(*this));
    }

    return mismatch_visitor(*this);
  }

  template <typename Requested, typename MatchVisitor, typename MismatchVisitor>
  constexpr auto visit(
      MatchVisitor match_visitor,
      MismatchVisitor mismatch_visitor) const -> decltype(auto) {
    if (is<Requested>()) {
      return match_visitor(static_cast<const Requested&>(*this));
    }

    return mismatch_visitor(*this);
  }

  // Gets the name of this Abstract.
  // If a canonical name is required then first call `resolve()`:
  // `canonical_name = abstract.resolve().get_name()`
  virtual constexpr auto get_name() const -> Perimortem::Core::View::Bytes = 0;

  // Returns the Abstract represented by this name. Alias uses this query to
  // redirect identity while ordinary Abstracts return themselves or an Abstract
  // that represents the intended canonical identity.
  //
  // For an unchanged valid DAG, resolving is idempotent:
  // `&abstract.resolve() == &abstract.resolve().resolve()`.
  virtual constexpr auto resolve() const -> const Abstract& { return *this; }

  // Returns the exact Type fact established for this identity. None proves
  // that the identity has no Type. Unknown preserves an answer that may still
  // materialize while the graph is completing.
  virtual auto get_type() const -> const Abstract&;

  // Resolves one binary concept owned by this Abstract. Concrete languages
  // compose their own concepts one question at a time; TTX does not flatten
  // member access, receiver policy, or invocation into routing modes.
  // Named concepts on an ordinary Abstract lead to other identities. Unknown
  // and None are the exceptions: every concept question repeats that sentinel.
  // This differs from resolve(), which can establish the receiver itself as
  // the canonical identity without adding a named edge.
  virtual auto resolve_concept(Perimortem::Core::View::Bytes name) const
      -> const Abstract&;

  using Visitor = Concept::Visitor<const Abstract&>;

  // The owner advertises the named answers visible at this boundary. It can
  // walk its own representation directly instead of building a Pack for the
  // caller to unpack. Names belong to this scope and can differ from the
  // selected Abstract's own name. Enumeration order carries no meaning.
  //
  // Calls are synchronous. The receiver must not invalidate the provider's
  // traversed state. A name need only survive its callback, so a consumer that
  // retains it makes its own copy. Abstracts keep their normal graph lifetime.
  virtual auto visit_concepts(Visitor visitor) const -> void;

  // Explicit erased values ask the candidate owner whether it satisfies one
  // exact semantic requirement. The answer records only the higher order
  // relationship. Interface negotiation and every physical Projection remain
  // with the concrete language and Terminal that understand them.
  virtual auto satisfies(const Abstract& requirement) const -> Bool;

  // Returns the documentation visible at this exact Abstract. The concrete
  // object may own authored prose, expose a generated comment, forward another
  // object's documentation, or compose several sources. This query does not
  // resolve identity implicitly.
  //
  // The returned object and every borrowed line remain valid for the lifetime
  // of this Abstract. Missing documentation is represented by an empty
  // Documentation object, never Unknown or a nullable reference.
  virtual constexpr auto get_documentation() const
      -> const Concept::Documentation& = 0;
};

}  // namespace Ttx::Concept

// Keep each derived category declaration beside its direct semantic base while
// preserving the shared live proof implementation.
#define TTX_CONTRACT(type, base)                                      \
  using ClassCatagory = type;                                         \
  constexpr auto implements(::U64 requested) const -> Bool override { \
    return requested == Ttx::Concept::get_type_identity<type>() ||    \
           base::implements(requested);                               \
  }

// Compact exact implementations of Abstract's universal presentation slots.
#define TTX_NAME(expression)                                                  \
  constexpr auto get_name() const -> Perimortem::Core::View::Bytes override { \
    return expression;                                                        \
  }
