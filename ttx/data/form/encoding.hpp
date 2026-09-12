// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/reader/binary.hpp"
#include "perimortem/core/writer/binary.hpp"

#include "ttx/data/form/schema.hpp"

namespace Ttx::Data::Form {

// Compiler and Representation meet at these block fields. Keeping their bit
// arithmetic together makes publication and observation follow the same wire
// format, without retaining a native object graph beside the encoded bytes.
// Header and Element are temporary decoded values. Only Block is serialized.
class Encoding {
 public:
  // The containing object supplies extent and alignment independently of its
  // occupied members. These facts preserve padding when runs are compacted.
  struct Header {
    Count count;
    Count alignment;
    Count extent;

    constexpr Header(Count count, Count alignment, Count extent)
        : count(count), alignment(alignment), extent(extent) {}
  };

  // One progression describes repeated primitives or repeated struct bodies.
  // Compiler first uses an internal body ID here, then substitutes the final
  // absolute block index when it publishes the descriptor.
  struct Element {
    Count count;
    Count offset;
    Count stride;
    Count type;
    Bool composite;

    constexpr Element(
        Count count = 0,
        Count offset = 0,
        Count stride = 0,
        Count type = 0,
        Bool composite = False)
        : count(count),
          offset(offset),
          stride(stride),
          type(type),
          composite(composite) {}
  };

  // The largest profile occupies fifteen U32 chunks. Native U64 limbs let us
  // emit the specified greedy sequence without requiring a native U96 type.
  // Fields wider than Count simply have zero high bits on this implementation.
  class Block {
   public:
    constexpr Block() = default;

    constexpr auto insert(Count value, Count first) -> void {
      const Count limb = first / 64;
      const Count shift = first % 64;
      chunks[limb] |= value << shift;
      if (shift && limb + 1 < 8) {
        chunks[limb + 1] |= value >> (64 - shift);
      }
    }

    constexpr auto extract(Count first, Count width) const -> Count {
      const Count limb = first / 64;
      const Count shift = first % 64;
      Count value = chunks[limb] >> shift;
      if (shift && limb + 1 < 8) {
        value |= chunks[limb + 1] << (64 - shift);
      }

      return width >= 64 ? value : value & ((Count(1) << width) - 1);
    }

    constexpr auto write(
        Perimortem::Core::Writer::Binary<
            Perimortem::Core::Data::ByteOrder::Little>& writer,
        U8 depth) const -> void {
      for (Count i = 0; i < depth / 2; ++i) {
        writer << chunks[i];
      }

      if (depth % 2) {
        writer << U32(chunks[depth / 2]);
      }
    }

    static constexpr auto read(
        Perimortem::Core::View::Bytes bytes,
        Count index,
        U8 depth) -> Block {
      Perimortem::Core::Reader::Binary<
          Perimortem::Core::Data::ByteOrder::Little>
          reader(bytes);
      Block result;
      reader.set_location(index * 4 * depth);

      // The prepared Representation guarantees complete blocks at these
      // coordinates, so each word read can use its value directly.
      for (Count i = 0; i < depth / 2; ++i) {
        result.chunks[i] = *reader.read_u64();
      }

      if (depth % 2) {
        result.chunks[depth / 2] = *reader.read_u32();
      }

      return result;
    }

   private:
    U64 chunks[8] = {};
  };

  static constexpr auto header(Header value, U8 depth) -> Block {
    const Count q = 8 * depth;
    Block block;
    block.insert(depth, 0);
    block.insert(value.count, 4);
    block.insert(value.alignment, 4 + q);
    block.insert(value.extent, 4 + 2 * q);
    return block;
  }

  static constexpr auto element(Element value, U8 depth) -> Block {
    const Count q = 8 * depth;
    Block block;
    block.insert(value.type, 0);
    block.insert(value.composite.value, q - 1);
    block.insert(value.stride, q);
    block.insert(value.offset, 2 * q);
    block.insert(value.count, 3 * q);
    return block;
  }

  static constexpr auto header(const Block& block, U8 depth) -> Header {
    const Count q = 8 * depth;
    return Header(
        block.extract(4, q), block.extract(4 + q, q),
        block.extract(4 + 2 * q, 16 * depth - 4));
  }

  static constexpr auto element(const Block& block, U8 depth) -> Element {
    const Count q = 8 * depth;
    return Element(
        block.extract(3 * q, q), block.extract(2 * q, q), block.extract(q, q),
        block.extract(0, q - 1), block.extract(q - 1, 1));
  }

};

}  // namespace Ttx::Data::Form
