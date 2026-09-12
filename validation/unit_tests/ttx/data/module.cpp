// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors
#include "validation/unit_tests/ttx/data/fixtures.hpp"

#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
using namespace Validation::FlowTests;
Module::Module(const char* library, const char* entry_name) {
  char path[4096];
  const auto length = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (length <= 0) {
    return;
  }
  path[length] = 0;
  auto* name = strrchr(path, '/');
  if (!name || sizeof(path) - (name + 1 - path) < strlen(library) + 1) {
    return;
  }
  memcpy(name + 1, library, strlen(library) + 1);
  module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (module) {
    auto entry = dlsym(module, entry_name);
    if (entry) {
      if (!strcmp(entry_name, "flow_provider_open")) {
        api = reinterpret_cast<
            const provider_api* (*)(decltype(&ttx_representation_compile))>(
            entry)(ttx_representation_compile);
      } else {
        heterogeneous = reinterpret_cast<
            const heterogeneous_provider* (*)(decltype(&ttx_representation_compile))>(
            entry)(ttx_representation_compile);
      }
    }
  }
}
Module::~Module() {
  if (module) {
    dlclose(module);
  }
}
auto Module::writer(State& state) const -> Query {
  return Query(api->writer(&state));
}
auto Module::legacy_writer(State& state) const -> Query {
  return Query(api->legacy_writer(&state));
}

auto Module::bootstrap_writer() const -> Query {
  return Query(api->bootstrap_writer());
}
// This is the explicit Direct/Shared cast permission, not a cast of opaque
// binding source state. The host checked the C Query schema before this call.
auto Module::import_query(const Flow& flow) const -> Query {
  const auto cast = [](const void* pointer) {
    return Query(*static_cast<const ttx_semantic_query*>(pointer));
  };
  return flow.visit(
      cast, cast,
      [](auto, auto) -> Query {
        Diagnostics::Log::fatal("Bootstrap requires a castable protocol."_view);
      },
      [](auto) -> Query {
        Diagnostics::Log::fatal("Bootstrap requires a castable protocol."_view);
      });
}
auto Module::selection() const -> Swizzle::Mapping {
  return Swizzle::Mapping::create(*api->selection())
      .visit(
          [](auto value) { return value; },
          [](Status) -> Swizzle::Mapping {
            Diagnostics::Log::fatal("Invalid C selection policy."_view);
          });
}
auto Module::select(const Flow& flow, Storage target) const -> Swizzle::Result {
  const provider_operations operations = {ttx_swizzle};
  const auto result =
      api->select(&operations, flow.get_abi(), target.get_abi());
  return static_cast<Status>(result.status);
}

auto Module::writer(Heterogeneous& state) const -> Query {
  return Query(heterogeneous->writer(&state));
}

auto Module::primitives() const -> Query {
  return Query(api->primitives());
}
auto Module::primitive_schema() const -> const Representation& {
  return *api->primitive_schema();
}
