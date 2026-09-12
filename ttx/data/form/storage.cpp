// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors
#include "ttx/data/form/storage.hpp"


auto ttx_storage_check(ttx_storage storage) -> ttx_data_status {
  if (!storage.representation) {
    return TTX_DATA_INVALID;
  }

  if (storage.size < storage.representation->get_extent()) {
    return TTX_DATA_BOUNDS;
  }

  if (storage.representation->get_extent() && !storage.data) {
    return TTX_DATA_INVALID;
  }

  if (reinterpret_cast<Count>(storage.data) %
      storage.representation->get_alignment()) {
    return TTX_DATA_INVALID;
  }

  return TTX_DATA_SUCCESS;
}
