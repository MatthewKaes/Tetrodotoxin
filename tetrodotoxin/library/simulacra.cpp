// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "tetrodotoxin/library/simulacra.hpp"

using namespace Perimortem::Core;
using namespace Ttx::Concept;
using Ttx::Semantic::Binding;
using namespace Tetrodotoxin::Library;

auto Simulacra::Handle::project(Abstract::Handle candidate) const
    -> Perimortem::Utility::Result<Simulacra, Failure> {
  return operations.project(source, candidate);
}

// Absence of one role does not disqualify an object from having another.
// Pending and rejected answers do stop projection, since treating them as
// absence would publish a weaker contract than the provider actually offers.
template <typename Contract>
static auto collect(
    Abstract::Handle source,
    Option<typename Contract::Handle>& destination)
    -> Option<Binding::Failure> {
  return source.bind<Contract>().visit(
      [&](const typename Contract::Handle& selected)
          -> Option<Binding::Failure> {
        destination = selected;
        return {};
      },
      [](Binding::Failure failure) -> Option<Binding::Failure> {
        if (failure == Binding::Failure::Unsupported) {
          return {};
        }
        return failure;
      });
}

auto Simulacra::project(Abstract::Handle source)
    -> Perimortem::Utility::Result<Simulacra, Failure> {
  Simulacra projection(source);

  // Ask about the boundary before asking about the imported implementation.
  // Even an acquired Import remains a dependency. Following its resolved Type
  // here would turn one edge into a copy of the other Library's declarations.
  auto failure =
      collect<Tetrodotoxin::Language::Import>(source, projection.dependency);
  if (failure) {
    return *failure;
  }
  if (projection.dependency) {
    return projection;
  }

  failure = collect<Tetrodotoxin::Language::Definition>(
      source, projection.definition);
  if (failure) {
    return *failure;
  }

  failure = collect<Ttx::Model::Type>(source, projection.type);
  if (failure) {
    return *failure;
  }

  failure = collect<Library::Language::Initialization>(
      source, projection.initialization);
  if (failure) {
    return *failure;
  }

  failure = collect<Ttx::Model::Callable>(source, projection.callable);
  if (failure) {
    return *failure;
  }

  failure = collect<Library::Language::Value>(source, projection.value);
  if (failure) {
    return *failure;
  }

  failure = collect<Ttx::Model::Addressable>(source, projection.addressable);
  if (failure) {
    return *failure;
  }

  // Every Abstract already supplies navigation. A namespace can therefore
  // contribute only its source view, including an empty context, without an
  // additional role or a traversal just to decide whether it is acceptable.
  return projection;
}
