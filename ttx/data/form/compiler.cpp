// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/form/compiler.hpp"

#include "ttx/data/form/representation.hpp"

using namespace Ttx::Data;
using namespace Ttx::Data::Form;

// The C facade keeps preparation local and requests one final allocation only
// after its exact size is known. The view and bytes share the supplied owner's
// lifetime. Compiler destruction releases all other construction storage.
auto ttx_representation_compile(
    const ttx_schema* schema,
    ttx_representation_allocator allocator,
    const ttx_representation** result) -> ttx_data_status {
  if (!schema || !allocator.allocate || !result) {
    return TTX_DATA_INVALID;
  }

  Compiler compiler;
  const auto status = compiler.compile(*schema);
  if (status != Status::Success) {
    return static_cast<ttx_data_status>(status);
  }

  const Count size = compiler.get_size();
  if (size > Count(-1) - sizeof(Representation)) {
    return TTX_DATA_OVERFLOW;
  }

  auto* allocation = allocator.allocate(
      allocator.source, sizeof(Representation) + size, alignof(Representation));
  if (!allocation) {
    return TTX_DATA_BOUNDS;
  }

  auto* bytes = static_cast<U8*>(allocation) + sizeof(Representation);
  const auto written =
      compiler.write(Perimortem::Core::Access::Bytes(bytes, size));
  if (written != Status::Success) {
    return static_cast<ttx_data_status>(written);
  }

  *result = new (allocation, Perimortem::Core::Placement::Construct)
      Representation(bytes, size);
  return TTX_DATA_SUCCESS;
}
