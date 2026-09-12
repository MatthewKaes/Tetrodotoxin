// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/semantic/binding.hpp"
#include "ttx/semantic/bound.hpp"
#include "ttx/lexical/anchor.hpp"
#include "ttx/lexical/cursor.hpp"

namespace Tetrodotoxin::Source {

// A declaration can answer source questions without making its semantic value
// responsible for the language that created it. The source owner supplies this
// binding when an editor needs provenance or a source pass needs to complete
// the declaration. Memory and Type queries remain separate contracts.
//
// The enclosing source orders completion across its declarations. Keeping that
// order at the source allows every explicit Type to settle before inference,
// and every initializer to link before constant evaluation. The selected
// provider performs the work rather than exposing its implementation class.
//
// Anchor and Cursor make this a native payload agreement. The state is still
// opaque, and a caller cannot cast it to an authored declaration. The source
// transaction keeps both the provider and its source facts alive. Selecting a
// binding has no side effects. Calling complete requires the source owner's
// exclusive construction authority and may change the declaration.
class Declaration {
 public:
  static constexpr Perimortem::System::Uuid contract_id{
    0x01a087abcbf97905,
    0x9ea29543510280bb,
  };

  enum class Phase : U8 {
    Type,
    InferredType,
    Initializer,
    Constant,
    Signature,
    Body,
    Finalize,
    RestoredType,
    RestoredInitializer,
    RestoredSignature,
  };

  using Failure = Ttx::Semantic::Binding::Failure;
  using Completion = Perimortem::Utility::Result<Bool, Failure>;

  struct Operations {
    auto (*get_anchor)(const void*)
        -> Perimortem::Core::Option<Ttx::Lexical::Anchor>;
    auto (*complete)(const void*, Phase, Ttx::Lexical::Cursor*) -> Completion;
  };

  class Handle : public Ttx::Semantic::Bound<Operations> {
   public:
    using Bound::Bound;

    auto get_anchor() const
        -> Perimortem::Core::Option<Ttx::Lexical::Anchor> {
      return operations.get_anchor(source);
    }

    // Restored phases need no Cursor. Authored phases borrow one for this
    // synchronous call so diagnostics remain attached to their actual source.
    auto complete(Phase phase, Ttx::Lexical::Cursor* cursor = nullptr) const
        -> Completion {
      return operations.complete(source, phase, cursor);
    }
  };

  // A native source offers this only for declarations it constructed as
  // mutable objects. Const binding preserves observation during selection,
  // while the completion thunk recovers the owner's mutable source state.
  template <typename Provider>
  static auto provide(const Provider& provider) -> Ttx::Semantic::Binding {
    static const Operations operations = {
      [](const void* source)
          -> Perimortem::Core::Option<Ttx::Lexical::Anchor> {
        return static_cast<const Provider*>(source)->get_declaration_anchor();
      },
      [](const void* source, Phase phase, Ttx::Lexical::Cursor* cursor)
          -> Completion {
        if (phase > Phase::RestoredSignature ||
            (phase < Phase::RestoredType && cursor == nullptr)) {
          return Failure::Rejected;
        }
        auto& declaration =
            *const_cast<Provider*>(static_cast<const Provider*>(source));
        return declaration.complete_source(phase, cursor);
      },
    };
    return Ttx::Semantic::Binding::provide<Declaration>(&provider, operations);
  }
};

}  // namespace Tetrodotoxin::Source
