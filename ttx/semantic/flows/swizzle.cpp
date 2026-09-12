// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors
#include "ttx/semantic/flows/swizzle.hpp"

#include "ttx/semantic/operations/fragment.hpp"
using namespace Ttx::Semantic;
using namespace Ttx::Semantic::Flows;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;
using Ttx::Data::Form::Representation;
using Ttx::Data::Protocol::Block;
using Ttx::Data::Protocol::Fragment;

auto ttx_swizzle_mapping_check(ttx_swizzle_mapping mapping) -> ttx_data_status {
  if (!mapping.input || !mapping.output || !mapping.position) {
    return TTX_DATA_INVALID;
  }

  Count offset = 0;
  Representation::Position output, input;
  // The output representation supplies the complete destination inventory.
  // Walking it once asks the policy for every required source without
  // constructing and checking another list of potentially overlapping
  // destination assignments.
  while (ttx_representation_next(mapping.output, offset, &output) !=
         TTX_DATA_BOUNDS) {
    const Count coordinate = mapping.position(mapping.source, output.offset);
    if (ttx_representation_next(mapping.input, coordinate, &input) ||
        input.offset != coordinate) {
      return TTX_DATA_BOUNDS;
    }

    if (input.representation->kind != output.representation->kind) {
      return TTX_DATA_INCOMPATIBLE;
    }

    if (input.representation->kind == TTX_SCHEMA_VALUE) {
      if (input.representation->data.value.type !=
          output.representation->data.value.type) {
        return TTX_DATA_INCOMPATIBLE;
      }
    } else if (!input.representation->compatible(*output.representation)) {
      return TTX_DATA_INCOMPATIBLE;
    }

    offset = output.offset + output.representation->extent;
  }

  return TTX_DATA_SUCCESS;
}

static auto memory(const void* source, Swizzle::Mapping mapping, Storage target)
    -> Swizzle::Result {
  // Reordering known overlapping storage requires an explicit snapshot policy.
  // Decline before writing instead of adding a hidden input allocation.
  const Count from = reinterpret_cast<Count>(source);
  const Count to = reinterpret_cast<Count>(target.get_bytes().get_data());
  const Count input_extent = mapping.get_input().extent;
  const Count output_extent = mapping.get_output().extent;
  if (input_extent && output_extent &&
      (from <= to ? to - from < input_extent : from - to < output_extent)) {
    return Status::Unsupported;
  }

  Count offset = 0;
  Representation::Position input, output;
  while (ttx_representation_next(&mapping.get_output(), offset, &output) !=
         TTX_DATA_BOUNDS) {
    const Count coordinate = mapping.position(output.offset);
    ttx_representation_next(&mapping.get_input(), coordinate, &input);
    const auto* bytes = static_cast<const U8*>(source) + coordinate;
    auto* destination = target.get_bytes().get_data() + output.offset;
    const Bool reverse = output.representation->kind == TTX_SCHEMA_VALUE &&
                         input.representation->data.value.byte_order !=
                             output.representation->data.value.byte_order;
    if (!reverse) {
      Perimortem::Core::Data::copy(
          destination, bytes, output.representation->extent);
    } else {
      for (Count i = 0; i < output.representation->extent; ++i) {
        destination[i] = bytes[output.representation->extent - i - 1];
      }
    }

    offset = output.offset + output.representation->extent;
  }

  return Status::Success;
}

static auto fragments(
    Fragment::Access source,
    Swizzle::Mapping mapping,
    Storage target) -> Swizzle::Result {
  Count offset = 0;
  Representation::Position output;
  while (ttx_representation_next(&mapping.get_output(), offset, &output) !=
         TTX_DATA_BOUNDS) {
    const auto status = Operations::Fragment::read(
        source, *output.representation, mapping.position(output.offset),
        [&](auto value) { Operations::Fragment::put(target, output, value); });
    if (status != Status::Success) {
      return status;
    }

    offset = output.offset + output.representation->extent;
  }

  return Status::Success;
}

auto Swizzle::flow(const Flow& flow, Mapping mapping, Storage target)
    -> Result {
  if (!flow.get_representation().compatible(mapping.get_input()) ||
      !mapping.get_output().compatible(target.get_representation())) {
    return Status::Incompatible;
  }

  auto copy = [&](const void* source) {
    return memory(source, mapping, target);
  };
  return flow.visit(
      copy, copy,
      [](Block::View, Block::Access) -> Result { return Status::Unsupported; },
      [&](Fragment::Access source) {
        return fragments(source, mapping, target);
      });
}

auto ttx_swizzle(
    const ttx_flow* flow,
    ttx_swizzle_mapping mapping,
    ttx_storage target) -> ttx_swizzle_result {
  return {static_cast<ttx_data_status>(Swizzle::flow(
      *reinterpret_cast<const Flow*>(flow), Swizzle::Mapping(mapping),
      Storage(target)))};
}
