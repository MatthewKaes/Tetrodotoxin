// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/data/protocol/fragment.hpp"

using namespace Ttx::Data;
using namespace Ttx::Data::Protocol;

// The output belongs to this stack frame. A successful provider call supplies
// its value before returning, so neither the facade nor its consumer needs a
// continuation or retained request just to read one primitive.
template <typename Value>
static auto read(
    ttx_data_status (*operation)(const void*, Count, Value*),
    const void* source,
    Count position) -> Perimortem::Utility::Result<Value, Status> {
  Value result;
  const auto status = operation(source, position, &result);
  if (status != TTX_DATA_SUCCESS) {
    return static_cast<Status>(status);
  }

  return result;
}

auto Fragment::Access::get_u8(Count position) const
    -> Perimortem::Utility::Result<U8, Status> {
  return read(value.operations->get_u8, value.source, position);
}

auto Fragment::Access::get_u16(Count position) const
    -> Perimortem::Utility::Result<U16, Status> {
  return read(value.operations->get_u16, value.source, position);
}

auto Fragment::Access::get_u32(Count position) const
    -> Perimortem::Utility::Result<U32, Status> {
  return read(value.operations->get_u32, value.source, position);
}

auto Fragment::Access::get_u64(Count position) const
    -> Perimortem::Utility::Result<U64, Status> {
  return read(value.operations->get_u64, value.source, position);
}

auto Fragment::Access::get_s8(Count position) const
    -> Perimortem::Utility::Result<S8, Status> {
  return read(value.operations->get_s8, value.source, position);
}

auto Fragment::Access::get_s16(Count position) const
    -> Perimortem::Utility::Result<S16, Status> {
  return read(value.operations->get_s16, value.source, position);
}

auto Fragment::Access::get_s32(Count position) const
    -> Perimortem::Utility::Result<S32, Status> {
  return read(value.operations->get_s32, value.source, position);
}

auto Fragment::Access::get_s64(Count position) const
    -> Perimortem::Utility::Result<S64, Status> {
  return read(value.operations->get_s64, value.source, position);
}

auto Fragment::Access::get_r32(Count position) const
    -> Perimortem::Utility::Result<R32, Status> {
  return read(value.operations->get_r32, value.source, position);
}

auto Fragment::Access::get_r64(Count position) const
    -> Perimortem::Utility::Result<R64, Status> {
  return read(value.operations->get_r64, value.source, position);
}

auto Fragment::Access::get_function(Count position) const
    -> Perimortem::Utility::Result<Fragment::Function, Status> {
  return read(value.operations->get_function, value.source, position);
}

auto Fragment::Access::get_pointer(Count position) const
    -> Perimortem::Utility::Result<void*, Status> {
  return read(value.operations->get_pointer, value.source, position);
}
