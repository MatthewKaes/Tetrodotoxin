// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/view/vector.hpp"

#include "ttx/data/form/schema.h"

namespace Ttx::Data::Form {

// C and C++ use the actual same descriptor record. Native methods do not turn
// a borrowed C descriptor into a different object or reinterpret an array of
// wrappers. Runtime construction has the same contract as constexpr use.
using Schema = ttx_schema;

}  // namespace Ttx::Data::Form

constexpr auto ttx_schema::primitive(Value type, ByteOrder order)
    -> ttx_schema {
  Count width = 0;
  switch (type) {
  case Value::U8:
  case Value::S8:
    width = 1;
    break;
  case Value::U16:
  case Value::S16:
    width = 2;
    break;
  case Value::U32:
  case Value::S32:
  case Value::R32:
    width = 4;
    break;
  case Value::U64:
  case Value::S64:
  case Value::R64:
  case Value::Pointer:
    width = 8;
    break;
  }

  return {
    width,
    width,
    static_cast<U8>(Kind::Value),
    {.value = {static_cast<U8>(type), static_cast<U8>(order)}}};
}

constexpr auto ttx_schema::composite(
    Perimortem::Core::View::Vector<Position> positions,
    Count extent,
    Count alignment) -> ttx_schema {
  return {
    extent,
    alignment,
    static_cast<U8>(Kind::Composite),
    {.composite = {positions.get_data(), positions.get_size()}}};
}

constexpr auto ttx_schema::range(
    const ttx_schema& element,
    Count repeats,
    Count stride,
    Count extent,
    Count alignment) -> ttx_schema {
  return {
    extent,
    alignment,
    static_cast<U8>(Kind::Range),
    {.range = {&element, repeats, stride}}};
}

constexpr auto ttx_schema::mapping(
    const ttx_schema& input,
    const ttx_schema& output) -> ttx_schema {
  return {8, 8, static_cast<U8>(Kind::Mapping), {.mapping = {&input, &output}}};
}
