// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "ttx/data/form/representation.hpp"
#include "ttx/data/form/schema.hpp"

namespace Ttx::Data::Form {

// The compiler is used to encode the canonical TTX format for transferring
// structured data descriptors. The wire format is a sequence of fixed width
// blocks, always encoded in little endian order. The whole buffer uses one
// width, but compilation promotes that width when any value needs more room.
// We do the normalization before publishing so consumers can compare bytes or
// follow offsets without reconstructing the source objects.
//
// Struct blocks describe the number of following element descriptors, the
// extent of the object including tail padding, and its required alignment.
// For a 32 bit block the fields, shown from most to least significant, are:
//
//   EEEEEEEE EEEESSSS SSSSCCCC CCCCFFFF
//
//   F: Encoding depth, in U32 chunks. One means 32 bits, two means 64,
//      three means 96, and so on through fifteen. Zero is invalid.
//   C: Number of direct element descriptor blocks following this header.
//      This counts compact descriptors, not their expanded repetitions.
//   S: Required alignment in bytes, expressed as a nonzero power of two.
//   E: Object extent in bytes, including padding after the last member.
//
// F always occupies bits zero through three, including in wider formats.
// Reading the first byte and masking with 0x0F establishes the block size as
// 4 * F bytes. The other nibble can contain part of C without making that
// bootstrap ambiguous. Every struct header repeats the same F because a
// reference needs one common block size throughout the publication.
//
// Element blocks describe a primitive or a reference to another struct body.
// For a 32 bit block their fields are:
//
//   AAAAAAAA OOOOOOOO SSSSSSSS CPPPPPPP
//
//   A: Number of occurrences. One describes a single element. Zero is never
//      emitted, so no zero value changes the meaning of multiplication.
//   O: Byte offset of the first occurrence relative to its enclosing struct.
//   S: Byte stride between the starts of repeated occurrences. This can exceed
//      the element's size without creating another struct or a hidden member.
//   C: Zero selects a primitive code in P. One selects a struct header index.
//   P: Primitive code or absolute block index measured from the buffer start.
//      A reference reaches byte P * 4 * F, not a location relative to itself.
//
// These two S fields answer different questions. A struct aligned to eight
// bytes can occupy 24 bytes. An array of that struct has stride 24, while its
// header still reports alignment eight. Likewise the two C fields are local
// to their block kind: a header's C is a count, an element's C is a selector.
//
// Wider encodings keep this division regular. Let q be eight times F, and let
// r be q minus one. Element A, O and S each occupy q bits. Its selector takes
// one bit and P occupies r bits. Struct C and S each occupy q bits, F stays
// four bits, and E occupies the remaining 16 * F minus four bits.
//
//   F   Block bits   Struct C/S/E bits   Element A/O/S/P bits
//   1       32             8/8/12                 8/8/8/7
//   2       64           16/16/28             16/16/16/15
//   3       96           24/24/44             24/24/24/23
//   4      128           32/32/60             32/32/32/31
//   8      256          64/64/124             64/64/64/63
//
// In every profile the numeric block is assembled with these shifts:
//
//   struct  = F | (C << 4) | (S << (4 + q)) | (E << (4 + 2*q))
//   element = P | (C << r) | (S << q) | (O << (2*q)) | (A << (3*q))
//
// All fields are unsigned. Wider profiles do not imply a native U96 or U128
// C++ type. Read and write each block greedily as F / 2 U64 chunks followed by
// F % 2 U32 chunks. Start with the least significant bits of the block, so an
// odd depth leaves its most significant 32 bits for the final U32 operation:
//
//   F   Operations in stream order
//   1   U32
//   2   U64
//   3   U64, U32
//   4   U64, U64
//
// Perimortem::Core::Writer::Binary encodes each chunk in little endian order,
// and Core::Reader::Binary reads the same sequence with host conversion. The
// chunks occupy consecutive bytes without alignment padding between them or
// between blocks. This keeps F in the first byte even when a block spans
// several operations. Grouping the chunks changes the number of operations,
// while the resulting bytes remain the block's little endian encoding.
// Unused high bits are zero, with no native struct padding, pointers or
// allocation addresses in the encoded buffer. Serialization does not allocate
// a separate object for each primitive or referenced body.
//
// Primitive codes use the Data vocabulary, independently of declaration order.
// The low six bits identify the primitive. Bit six specifies big endian
// payload storage when set, or little endian storage when clear. Higher P bits
// are zero for primitives, even in a wider profile. For a reference all P bits
// belong to the index instead. The descriptor words themselves remain little
// endian regardless of the payload order they describe.
//
//   Code   Primitive   Bytes
//     1       U8         1
//     2       U16        2
//     3       U32        4
//     4       U64        8
//     5       S8         1
//     6       S16        2
//     7       S32        4
//     8       S64        8
//     9       R32        4
//    10       R64        8
//    12       Pointer    8
//
// Codes zero, eleven and the unassigned base codes are reserved. Byte order
// cannot distinguish a one byte value, so U8 and S8 always clear bit six.
// Names, source identity, callable signatures and calling conventions belong
// to Semantic contracts acquired through Data transport. Pointer storage does
// not itself establish any of those meanings and pointers do not encode the
// schema they point to, only that they exist.
//
// A struct header is followed immediately by its C direct element blocks.
// Other struct bodies follow in first use depth first order. For example,
// after the root header and its descriptors come C1, C1's children C1C1 and
// C1C2, then C2 and its child C2C1. Each body includes its header and direct
// descriptors before any newly reached child body. References can therefore
// point forward or backward to an already emitted body. References aren't
// relative and are direct offsets from the root as the format is not meant to
// be self syncing in anyway.
//
// One stored body may serve many occurrences: each reference preserves its
// own offset, count and stride, and still denotes a struct boundary for every
// occurrence. Sharing the body never merges those occurrences into one object.
// References land only on header blocks within this buffer. Containment is
// acyclic, so a back reference can reuse a completed body but cannot introduce
// a recursive object with no finite extent. This is one of the main reasons
// pointers don't encode their schema information.
//
// Encoded buffers can be compared for equivalence by checking their lengths
// and comparing every byte. Equal canonical bytes establish the same data
// format, including its struct boundaries and geometry. They do not compare
// the runtime values being transported or establish a higher semantic type.
// To make equivalent schemas produce that one representation, compilation
// applies the following canonicalization rules before choosing the depth:
//
// 1. Keep struct boundaries, primitive identities, payload byte order,
//    offsets, extents and alignments. The root describes the whole object
//    directly. A struct reference is not replaced with its primitive children,
//    even when it contains only one member. Padding is described by offsets
//    and extents rather than invented members or synthetic structs.
//
// 2. Normalize each body before its parent compares or shares it. Its direct
//    occurrences are ordered by byte offset. Invalid primitive codes, cycles,
//    impossible geometry and overlapping occupied placements are rejected.
//    Names and source enumeration order do not affect the resulting body.
//
// 3. Compact from the lowest offset. When the next occurrence has the same
//    normalized type, their start difference establishes a candidate stride.
//    Take the longest consecutive run of that type at that stride, then
//    continue with the first occurrence outside the run. Struct type equality
//    here includes the normalized child body, extent and alignment. Primitive
//    type equality includes its payload byte order.
//
// 4. Source range boundaries do not stop that compaction. Compare and consume
//    matching prefixes arithmetically, splitting a source run when necessary.
//    U32 starts 0, 4, 8, 16 normalize to a count of three at stride four and
//    one singleton at 16, even if the input grouped them as two pairs. Nested
//    repetition can be combined only while preserving the same starts and
//    struct occurrences. A count never moves through a struct reference into
//    its members. Large counts remain compact throughout this work.
//
// 5. For A greater than one, S is the actual distance between starts. For A
//    equal to one, S is the intrinsic element size: primitive width or the
//    referenced header's E. An unused source stride is discarded. Thus one
//    U32 in an eight byte aligned, eight byte struct has element stride four.
//    An array of those structs has reference stride eight. This leaves no
//    discretionary padding value that could change the bytes of a singleton.
//
// 6. Validate the final occupied end using the last start plus the element
//    size. A * S is not necessarily that end, because it includes a stride
//    after the last occurrence. Three U32s at stride eight end at byte 20.
//    A root extent of 24 preserves the remaining four bytes of tail padding.
//
// 7. Elide zero occurrences and empty members. With no values, the only result
//    is one root header with F one, C zero, S one and E zero. A nonzero extent
//    without values is invalid. No unreachable empty body or zero A element
//    is added to the buffer to remember how that unit was authored.
//
// 8. Intern structurally equal normalized bodies, including bodies authored
//    independently. Equality uses child structure rather than source pointers,
//    hash values or names. Assign their absolute block indices by first use
//    depth first traversal after compaction. Emit each distinct reachable body
//    once, with no extra body inventory or unreferenced records at the end.
//
// 9. Scan all normalized fields and assigned indices for the smallest F that
//    holds every value in its profile. One overflowing count, offset, stride,
//    reference, extent, alignment or descriptor count promotes the entire
//    buffer. A run is not split merely to avoid promotion. All bodies repeat
//    the chosen F. Block indices were assigned in blocks, so widening changes
//    byte addresses without changing traversal or reference numbering.
//
// 10. Write only that final depth, with unused bits zero. Hashing, runtime
//     versus constant evaluation, and allocation policy cannot affect the
//     output. The Driver owns working storage and the final buffer so those
//     construction choices disappear from the published descriptor.
//
// For example, U8 at offsets zero, one and two followed by U32 at offset four
// becomes one U8 descriptor with A three and S one, then one U32 descriptor
// with A one and S four. The header's E eight retains the surrounding padding.
// U32s at offsets zero, eight and sixteen use one descriptor with A three
// and S eight. Referencing three one member structs uses C one, while
// that flat primitive run uses C zero, so the two layouts cannot collide.
//
// Comparison is linear in the encoded byte length and requires no decoding.
// Access reads the depth from the first byte, then follows header counts,
// offsets and references. Repetition computes an instance start from O and S
// without storing an entry for each value. Enumeration necessarily visits each
// requested primitive occurrence, not merely each compressed descriptor.
//
// TODO: Implement this packed format in place of the publication graph.
template <typename Driver>
class Compiler {
 public:
  constexpr explicit Compiler(Driver& driver) : driver(driver) {}

  constexpr auto compile(const Schema* source) -> Status {
    if (!source) {
      return Status::Invalid;
    }

    return driver.find(source).visit(
        [&]() {
          const Count alignment = source->get_alignment();
          if (!alignment || (alignment & (alignment - 1))) {
            return Status::Invalid;
          }

          driver.cache(source, nullptr);
          Representation result(
              Representation::Kind::Sequence, source->get_extent(), alignment);
          switch (source->get_kind()) {
          case Schema::Kind::Value:
            return compile_value(*source, result);
          case Schema::Kind::Mapping:
            return compile_mapping(*source, result);
          case Schema::Kind::Composite:
          case Schema::Kind::Range:
            return compile_sequence(*source, result);
          }

          return Status::Invalid;
        },
        [](const Representation* result) {
          return result ? Status::Success : Status::Invalid;
        });
  }

 private:
  using Elements = typename Driver::Elements;
  using Composites = typename Driver::Composites;
  using Element = Representation::Element;
  using Composite = Representation::Composite;

  constexpr auto compile_value(const Schema& source, Representation& result)
      -> Status {
    const auto primitive = Schema::primitive(source.get_value());
    if (!primitive.get_extent()) {
      return Status::Invalid;
    }

    if (source.get_extent() != primitive.get_extent() ||
        source.get_alignment() != primitive.get_alignment()) {
      return Status::Invalid;
    }

    if (source.get_byte_order() > Schema::ByteOrder::Big) {
      return Status::Invalid;
    }

    result.kind = static_cast<U8>(Representation::Kind::Value);
    result.count = 1;
    result.data.value.type = static_cast<U8>(source.get_value());
    result.data.value.byte_order = static_cast<U8>(source.get_byte_order());
    return publish(source, result);
  }

  constexpr auto compile_mapping(const Schema& source, Representation& result)
      -> Status {
    if (source.get_extent() != 8 || source.get_alignment() != 8) {
      return Status::Invalid;
    }

    const auto& mapping = source.get_mapping();
    const auto input = compile(mapping.get_input());
    if (input != Status::Success) {
      return input;
    }

    const auto output = compile(mapping.get_output());
    if (output != Status::Success) {
      return output;
    }

    result.kind = static_cast<U8>(Representation::Kind::Mapping);
    result.count = 1;
    result.data.mapping.input = *driver.find(mapping.get_input());
    result.data.mapping.output = *driver.find(mapping.get_output());
    return publish(source, result);
  }

  constexpr auto compile_sequence(const Schema& source, Representation& result)
      -> Status {
    Elements elements;
    Composites composites;
    const auto status =
        source.get_kind() == Schema::Kind::Composite
            ? emit_composite(source, 0, result.count, elements, composites)
            : emit_range(source, 0, result.count, elements, composites);
    if (status != Status::Success) {
      return status;
    }

    // Canonical interval order is preorder: starts ascend, and enclosing
    // intervals precede their children. Equal adjacent intervals represent the
    // same boundary, even when several authored wrappers supplied it.
    Composites compact;
    for (auto entry : composites.get_view()) {
      // Every publication already supplies its whole interval through count.
      // A nested record retains that interval when placed inside a larger one.
      if (entry.count == 1 && !entry.first && entry.end == result.count) {
        entry.end = entry.first;
      }

      if (entry.first == entry.end && !entry.pattern) {
        continue;
      }

      if (compact.get_size()) {
        const auto& previous = compact[compact.get_size() - 1];
        if (!entry.pattern && !previous.pattern && entry.count == 1 &&
            previous.count == 1 && entry.first == previous.first &&
            entry.end == previous.end) {
          continue;
        }
      }

      append_composite(compact, entry);
    }

    for (Count i = 0; i < compact.get_size(); ++i) {
      auto& entry = compact[i];
      entry.ordinal = result.composite_count;
      const Count width = entry.pattern ? entry.pattern->composite_count +
                                              Count(entry.first != entry.end)
                                        : 1;
      Count count;
      if (__builtin_mul_overflow(entry.count, width, &count) ||
          __builtin_add_overflow(
              result.composite_count, count, &result.composite_count)) {
        return Status::Overflow;
      }
    }

    result.data.sequence.size = elements.get_size();
    result.data.sequence.elements =
        driver.publish_elements(elements.get_view());
    result.composite_size = compact.get_size();
    result.composites = driver.publish_composites(compact.get_view());
    return publish(source, result);
  }

  // Each placement is emitted once into the root's work buffers. A Composite
  // reserves its interval before its children, then closes it once their total
  // logical width is known. No intermediate position arrays are copied upward.
  constexpr auto emit_composite(
      const Schema& source,
      Count offset,
      Count& index,
      Elements& elements,
      Composites& composites) -> Status {
    const auto children = source.get_positions();
    if (children.get_size() && !children.get_data()) {
      return Status::Invalid;
    }

    const Count slot = composites.get_size();
    const Count first = index;
    composites.insert(Composite(first));
    Count end = 0;
    for (const auto& placement : children) {
      const auto* child = placement.get_schema();
      if (!child || placement.get_offset() < end) {
        return Status::Invalid;
      }

      if (__builtin_add_overflow(
              placement.get_offset(), child->get_extent(), &end)) {
        return Status::Overflow;
      }

      if (end > source.get_extent()) {
        return Status::Invalid;
      }

      const auto status = emit(
          *child, offset + placement.get_offset(), index, elements, composites);
      if (status != Status::Success) {
        return status;
      }
    }

    composites[slot].end = index;
    return first == index && source.get_extent() ? Status::Invalid
                                                 : Status::Success;
  }

  constexpr auto emit(
      const Schema& source,
      Count offset,
      Count& index,
      Elements& elements,
      Composites& composites) -> Status {
    if (source.get_kind() == Schema::Kind::Composite) {
      return driver.find(&source).visit(
          [&]() {
            const Count alignment = source.get_alignment();
            if (!alignment || (alignment & (alignment - 1))) {
              return Status::Invalid;
            }

            driver.cache(&source, nullptr);
            const auto status =
                emit_composite(source, offset, index, elements, composites);
            driver.forget(&source);
            return status;
          },
          [&](const Representation* ready) {
            if (!ready) {
              return Status::Invalid;
            }

            composites.insert(Composite(index, index + ready->count));
            return emit_prepared(*ready, offset, index, elements, composites);
          });
    }

    const auto status = compile(&source);
    if (status != Status::Success) {
      return status;
    }

    return emit_prepared(
        **driver.find(&source), offset, index, elements, composites);
  }

  constexpr auto emit_prepared(
      const Representation& ready,
      Count offset,
      Count& index,
      Elements& elements,
      Composites& composites) -> Status {
    const Count first = index;
    if (__builtin_add_overflow(index, ready.count, &index)) {
      return Status::Overflow;
    }

    if (ready.get_kind() == Representation::Kind::Sequence) {
      for (const auto& entry : ready.get_elements()) {
        append_element(
            elements, Element(
                          entry.representation, offset + entry.offset,
                          first + entry.index, entry.count, entry.stride));
      }
    } else if (ready.count) {
      append_element(elements, Element(&ready, offset, first, 1, ready.extent));
    }

    for (const auto& entry : ready.get_composites()) {
      composites.insert(Composite(
          first + entry.first, first + entry.end, 0, entry.count, entry.stride,
          entry.pattern));
    }

    return Status::Success;
  }

  constexpr auto emit_range(
      const Schema& source,
      Count offset,
      Count& index,
      Elements& elements,
      Composites& composites) -> Status {
    const auto& range = source.get_range();
    const auto status = compile(range.get_element());
    if (status != Status::Success) {
      return status;
    }

    const auto* pattern = *driver.find(range.get_element());
    if (!range.get_count() || !pattern->count) {
      return source.get_extent() ? Status::Invalid : Status::Success;
    }

    Count count;
    if (__builtin_mul_overflow(range.get_count(), pattern->count, &count)) {
      return Status::Overflow;
    }

    Count last;
    if (__builtin_mul_overflow(
            range.get_count() - 1, range.get_stride(), &last)) {
      return Status::Overflow;
    }

    Count end;
    if (__builtin_add_overflow(last, pattern->extent, &end)) {
      return Status::Overflow;
    }

    if (end > source.get_extent()) {
      return Status::Invalid;
    }

    if (range.get_count() > 1 && range.get_stride() < pattern->extent) {
      return Status::Invalid;
    }

    const Count first = index;
    if (__builtin_add_overflow(index, count, &index)) {
      return Status::Overflow;
    }

    append_element(
        elements,
        Element(
            pattern, offset, first, range.get_count(),
            range.get_count() == 1 ? pattern->extent : range.get_stride()));
    const Bool record =
        range.get_element()->get_kind() == Schema::Kind::Composite;
    if (pattern->composite_count || record) {
      composites.insert(Composite(
          first, record ? first + pattern->count : first, 0, range.get_count(),
          pattern->count, pattern));
    }

    return Status::Success;
  }

  // Keep one pending progression at the end of the output. A new sibling can
  // extend it when it supplies the same element exactly at the next byte start.
  // Repetition of a one entry pattern first reduces to that entry's
  // progression.
  static constexpr auto append_element(Elements& output, Element value)
      -> void {
    while (value.representation->get_kind() == Representation::Kind::Sequence &&
           value.representation->data.sequence.size == 1) {
      const auto& inner = value.representation->data.sequence.elements[0];
      if (value.count == 1) {
        value.offset += inner.offset;
        value.count = inner.count;
        value.stride = inner.stride;
      } else if (inner.count == 1) {
        value.offset += inner.offset;
      } else {
        Count batch_stride;
        if (__builtin_mul_overflow(inner.count, inner.stride, &batch_stride)) {
          break;
        }

        if (value.stride != batch_stride) {
          break;
        }

        value.count *= inner.count;
        value.stride = inner.stride;
        value.offset += inner.offset;
      }

      value.representation = inner.representation;
    }

    if (output.get_size()) {
      if (merge_elements(output[output.get_size() - 1], value)) {
        return;
      }
    }

    output.insert(value);
  }

  static constexpr auto merge_elements(Element& last, const Element& value)
      -> Bool {
    if (!last.representation->compatible(*value.representation)) {
      return False;
    }

    const Count stride =
        last.count == 1 ? value.offset - last.offset : last.stride;
    if (value.count > 1 && value.stride != stride) {
      return False;
    }

    // The next start is a proposed coordinate beyond the current run. It can
    // overflow even though every occupied position in that run was admitted.
    Count next;
    if (__builtin_mul_overflow(last.count, stride, &next)) {
      return False;
    }

    if (__builtin_add_overflow(last.offset, next, &next)) {
      return False;
    }

    if (next != value.offset) {
      return False;
    }

    last.count += value.count;
    last.stride = stride;
    return True;
  }

  static constexpr auto append_composite(Composites& output, Composite value)
      -> void {
    if (value.pattern && !value.pattern->composite_count) {
      value.pattern = nullptr;
    }

    while (value.pattern && value.first == value.end &&
           value.pattern->composite_size == 1) {
      const auto& inner = value.pattern->composites[0];
      if (inner.count > 1) {
        Count period;
        if (__builtin_mul_overflow(inner.count, inner.stride, &period)) {
          break;
        }

        if (value.stride != period) {
          break;
        }
      }

      value.first += inner.first;
      value.end = value.first + inner.end - inner.first;
      if (inner.count > 1) {
        value.count *= inner.count;
        value.stride = inner.stride;
      }

      value.pattern = inner.pattern;
    }

    if (output.get_size()) {
      auto& last = output[output.get_size() - 1];
      if (!last.pattern && !value.pattern &&
          last.end - last.first == value.end - value.first) {
        const Count stride =
            last.count == 1 ? value.first - last.first : last.stride;
        if (value.first == last.first + last.count * stride &&
            (value.count == 1 || value.stride == stride)) {
          last.count += value.count;
          last.stride = stride;
          return;
        }
      }
    }

    output.insert(value);
  }

  constexpr auto publish(const Schema& source, Representation& result)
      -> Status {
    if (!result.count) {
      if (result.extent) {
        return Status::Invalid;
      }

      result = Representation();
    }

    driver.cache(&source, driver.publish(result));
    return Status::Success;
  }

  Driver& driver;
};

}  // namespace Ttx::Data::Form
