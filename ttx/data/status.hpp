// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/status.h"

namespace Ttx::Data {

enum class Status : U8 {
  Success = TTX_DATA_SUCCESS,
  Invalid = TTX_DATA_INVALID,
  Bounds = TTX_DATA_BOUNDS,
  Overflow = TTX_DATA_OVERFLOW,
  Incompatible = TTX_DATA_INCOMPATIBLE,
  Unsupported = TTX_DATA_UNSUPPORTED,
  Busy = TTX_DATA_BUSY,
  Denied = TTX_DATA_DENIED,
  IoError = TTX_DATA_IO_ERROR,
};

}  // namespace Ttx::Data
