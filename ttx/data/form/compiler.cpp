// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/form/compiler.hpp"

#include "perimortem/memory/dynamic/map.hpp"
#include "perimortem/memory/dynamic/vector.hpp"

using namespace Ttx::Data;
using namespace Ttx::Data::Form;
using namespace Perimortem::Memory;

// The Runtime Driver provides a built in way for building Schemas at runtime
// across the ABI.
//
// Compiled records must outlive this call so consumers can keep borrowing them
// so the driver publishes into the caller's allocation lifetime so the caller
// can manage the lifetime.
//
// All allocated working data is kept local to the driver and is automatically
// cleaned up after compilation.
struct RuntimeDriver {
  using Elements = Dynamic::Vector<Representation::Element>;
  using Composites = Dynamic::Vector<Representation::Composite>;
  ttx_representation_allocator allocator;
  Dynamic::Map<const Schema*, const Representation*> children;

  auto find(const Schema* source)
      -> Perimortem::Core::Option<const Representation*> {
    const auto entry = children.find(source);
    if (entry) {
      return entry->value;
    }

    return {};
  }

  auto cache(const Schema* source, const Representation* result) -> void {
    children.insert(source, result);
  }

  auto forget(const Schema* source) -> void { children.remove(source); }

  auto publish(const Representation& value) -> const Representation* {
    auto* storage = allocator.allocate(
        allocator.source, sizeof(Representation), alignof(Representation));
    return new (storage, Perimortem::Core::Placement::Construct)
        Representation(value);
  }

  template <typename T>
  auto publish_array(Perimortem::Core::View::Vector<T> entries) -> const T* {
    if (!entries.get_size()) {
      return nullptr;
    }

    auto* output = static_cast<T*>(allocator.allocate(
        allocator.source, entries.get_size() * sizeof(T), alignof(T)));
    for (Count i = 0; i < entries.get_size(); ++i) {
      new (output + i, Perimortem::Core::Placement::Construct) T(entries[i]);
    }

    return output;
  }

  auto publish_elements(
      Perimortem::Core::View::Vector<Representation::Element> entries)
      -> const Representation::Element* {
    return publish_array(entries);
  }

  auto publish_composites(
      Perimortem::Core::View::Vector<Representation::Composite> entries)
      -> const Representation::Composite* {
    return publish_array(entries);
  }
};

auto ttx_representation_compile(
    const ttx_schema* schema,
    ttx_representation_allocator allocator,
    const ttx_representation** result) -> ttx_data_status {
  if (!allocator.allocate || !result) {
    return TTX_DATA_INVALID;
  }

  RuntimeDriver driver(allocator);
  const auto status = Compiler(driver).compile(schema);
  if (status != Status::Success) {
    return static_cast<ttx_data_status>(status);
  }

  return driver.find(schema).visit(
      []() { return TTX_DATA_INVALID; },
      [&](const ttx_representation* schema) {
        *result = schema;
        return TTX_DATA_SUCCESS;
      });
}
