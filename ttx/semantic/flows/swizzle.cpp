// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "ttx/semantic/flows/swizzle.hpp"

#include "perimortem/core/algorithm/sort.hpp"

#include "perimortem/memory/dynamic/map.hpp"

#include "ttx/semantic/operations/fragment.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Memory;
using namespace Ttx::Semantic;
using namespace Ttx::Semantic::Flows;
using namespace Ttx::Data;
using namespace Ttx::Data::Form;
using Ttx::Data::Protocol::Block;
using Ttx::Data::Protocol::Fragment;

auto Swizzle::Mapping::create(ttx_swizzle_selection selection)
    -> Perimortem::Utility::Result<Mapping, Status> {
  if (!selection.input || !selection.output || !selection.position) {
    return Status::Invalid;
  }

  // Output traversal discovers groups in first selection order. Temporary
  // vectors collect repeated destinations before publication packs them into
  // one position array. The lookup table exists only during this preparation.
  struct Selection {
    Representation::Position input;
    Dynamic::Vector<Representation::Position> outputs;

    explicit Selection(Representation::Position input) : input(input) {}
  };

  Dynamic::Map<Count, Count> indices;
  Dynamic::Vector<Selection> selections;
  Dynamic::Vector<Count> coordinates;
  Count output_count = 0;

  const auto collected =
      selection.output->visit([&](Representation::Position output) {
        const Count coordinate =
            selection.position(selection.source, output.offset);
        const Count index =
            indices.find(coordinate)
                .visit(
                    [&] {
                      const Count index = selections.get_size();
                      selections.emplace(Selection(output));
                      indices.insert(coordinate, index);
                      coordinates.insert(coordinate);
                      return index;
                    },
                    [](const auto& entry) { return entry.value; });
        auto& group = selections[index];
        if (group.input.get_value() != output.get_value()) {
          return Status::Incompatible;
        }

        group.outputs.insert(output);
        ++output_count;
        return Status::Success;
      });
  if (collected != Status::Success) {
    return collected;
  }

  // Sort only the source coordinates used for discovery. The selections keep
  // their original order, which determines when a stateful provider is
  // observed. Ranges can skip directly to these coordinates without expanding
  // their count.
  Algorithm::sort(coordinates.get_access());
  const auto resolved = selection.input->visit(
      coordinates.get_view(), [&](Representation::Position input) {
        return indices.find(input.offset)
            .visit(
                [] { return Status::Bounds; },
                [&](const auto& entry) {
                  auto& group = selections[entry.value];
                  if (input.get_value() != group.input.get_value()) {
                    return Status::Incompatible;
                  }

                  group.input = input;
                  return Status::Success;
                });
      });
  if (resolved != Status::Success) {
    return resolved;
  }

  Mapping result(
      *selection.input, *selection.output, selections.get_size(), output_count);
  for (const auto& group : selections.get_view()) {
    const Count start = result.outputs.get_size();
    for (const auto& output : group.outputs.get_view()) {
      result.outputs.insert(output);
    }

    result.groups.insert(ttx_swizzle_group(
        group.input, result.outputs.get_data() + start,
        group.outputs.get_size()));
  }

  return Perimortem::Core::Data::take(result);
}

static auto memory(
    const void* source,
    ttx_swizzle_mapping mapping,
    Storage target) -> Status {
  // A permutation can overwrite a source before its group is observed. Keep
  // snapshot ownership explicit by declining known overlap before any write.
  const Count from = reinterpret_cast<Count>(source);
  const Count to = reinterpret_cast<Count>(target.get_bytes().get_data());
  const Count input_extent = mapping.input->get_extent();
  const Count output_extent = mapping.output->get_extent();
  if (input_extent && output_extent &&
      (from <= to ? to - from < input_extent : from - to < output_extent)) {
    return Status::Unsupported;
  }

  for (Count i = 0; i < mapping.count; ++i) {
    const auto& group = mapping.groups[i];
    const Count width = group.input.get_extent();
    Static::Bytes<8> observation;
    Data::copy(
        observation.get_data(),
        static_cast<const U8*>(source) + group.input.offset, width);
    for (Count j = 0; j < group.count; ++j) {
      const auto& output = group.outputs[j];
      auto* destination = target.get_bytes().get_data() + output.offset;
      if (group.input.get_byte_order() == output.get_byte_order()) {
        Data::copy(destination, observation.get_data(), width);
      } else {
        for (Count k = 0; k < width; ++k) {
          destination[k] = observation[width - k - 1];
        }
      }
    }
  }

  return Status::Success;
}

static auto fragments(
    Fragment::Access source,
    ttx_swizzle_mapping mapping,
    Storage target) -> Status {
  for (Count i = 0; i < mapping.count; ++i) {
    const auto& group = mapping.groups[i];
    const auto status = Operations::Fragment::read(
        source, group.input, group.input.offset, [&](auto value) {
          for (Count j = 0; j < group.count; ++j) {
            Operations::Fragment::put(target, group.outputs[j], value);
          }
        });
    if (status != Status::Success) {
      return status;
    }
  }

  return Status::Success;
}

static auto swizzle(
    const Flow& flow,
    ttx_swizzle_mapping mapping,
    Storage target) -> Status {
  if (!flow.get_representation().compatible(*mapping.input) ||
      !mapping.output->compatible(target.get_representation())) {
    return Status::Incompatible;
  }

  const auto copy = [&](const void* source) {
    return memory(source, mapping, target);
  };
  return flow.visit(
      copy, copy,
      [](Block::View, Block::Access) { return Status::Unsupported; },
      [&](Fragment::Access source) {
        return fragments(source, mapping, target);
      });
}

auto Swizzle::flow(const Flow& flow, const Mapping& mapping, Storage target)
    -> Status {
  return swizzle(flow, mapping.get_abi(), target);
}

auto ttx_swizzle(
    const ttx_flow* flow,
    ttx_swizzle_mapping mapping,
    ttx_storage target) -> ttx_data_status {
  return static_cast<ttx_data_status>(
      swizzle(*reinterpret_cast<const Flow*>(flow), mapping, Storage(target)));
}
