// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#pragma once

#include "perimortem/core/static/vector.hpp"

#include "perimortem/memory/const/vector.hpp"

#include "ttx/data/form/encoding.hpp"
#include "ttx/data/form/schema.hpp"
#include "ttx/data/status.hpp"

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
//    its members. Scalar progressions and repeated struct bodies stay compact.
//    Repeated batches with gaps can require a separate run for each batch, so
//    their descriptor count grows with the discontinuities being described.
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
//     output. Compiler owns temporary preparation and the caller owns the final
//     buffer, so construction storage disappears from the published descriptor.
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
class Compiler {
 public:
  constexpr Compiler() = default;
  Compiler(const Compiler&) = delete;
  constexpr Compiler(Compiler&&) = default;

  // Compilation keeps its construction inventory until publication. Success
  // establishes the precondition for size and write, while a failed attempt
  // leaves only temporary storage that the compiler will release normally.
  constexpr auto compile(const Schema& source) -> Status {
    bodies.resize(0);
    elements.resize(0);
    sources.resize(0);
    order.resize(0);
    buckets.resize(0);
    block_count = 0;
    depth = 0;
    for (Count i = 0; i < primitive_bodies.get_size(); ++i) {
      primitive_bodies[i] = 0;
    }

    Count root = 0;
    const auto status = prepare(source, root);
    if (status != Status::Success) {
      return status;
    }

    const auto numbering = number(root);
    if (numbering != Status::Success) {
      return numbering;
    }

    return choose_depth();
  }

  constexpr auto get_size() const -> Count { return block_count * 4 * depth; }
  constexpr auto get_depth() const -> U8 { return depth; }

  // The prepared records already contain every decision. This pass merely
  // replaces internal body IDs with their assigned block indices and writes
  // the bits. Check capacity once so a short target receives no partial form.
  constexpr auto write(Perimortem::Core::Access::Bytes target) const -> Status {
    if (target.get_size() < get_size()) {
      return Status::Bounds;
    }

    Perimortem::Core::Writer::Binary<Perimortem::Core::Data::ByteOrder::Little>
        writer(target);
    for (Count i = 0; i < order.get_size(); ++i) {
      const auto& body = bodies[order[i]];
      const auto header = Encoding::header(
          Encoding::Header(body.size, body.alignment, body.extent), depth);
      header.write(writer, depth);
      for (Count j = 0; j < body.size; ++j) {
        auto entry = elements[body.first + j];
        if (entry.composite) {
          entry.type = bodies[entry.type].block;
        }

        const auto block = Encoding::element(entry, depth);
        block.write(writer, depth);
      }
    }

    return writer.is_valid() ? Status::Success : Status::Bounds;
  }

 private:
  using Element = Encoding::Element;
  using Elements = Perimortem::Memory::Const::Vector<Element>;

  struct Body {
    Count extent;
    Count alignment;
    Count first;
    Count size;
    U64 hash;
    Count next = 0;
    Count block = Count(-1);

    constexpr Body(
        Count extent = 0,
        Count alignment = 1,
        Count first = 0,
        Count size = 0,
        U64 hash = 0)
        : extent(extent),
          alignment(alignment),
          first(first),
          size(size),
          hash(hash) {}
  };

  // Remembering a source before following children lets another encounter
  // detect a containment cycle. Completion replaces the pending marker with
  // the canonical body ID that later visits can reuse.
  struct Source {
    const Schema* schema = nullptr;
    Count body = Count(-1);

    constexpr Source(const Schema* schema = nullptr) : schema(schema) {}
  };

  // These helpers share the compiler's temporary inventories. Keeping them on
  // the owner makes their phase dependencies explicit without exposing a
  // Driver or passing a construction graph through every recursive call.
  constexpr auto prepare(const Schema& source, Count& result) -> Status {
    // Primitive facts are complete at this edge. Intern their one descriptor
    // directly instead of allocating a temporary vector and memo entry for
    // every independently authored spelling of the same scalar.
    if (source.get_kind() == Schema::Kind::Value) {
      return prepare_value(source, result);
    }

    for (Count i = 0; i < sources.get_size(); ++i) {
      if (sources[i].schema == &source) {
        result = sources[i].body;
        return result == Count(-1) ? Status::Invalid : Status::Success;
      }
    }

    const Count alignment = source.get_alignment();
    if (!alignment || (alignment & (alignment - 1))) {
      return Status::Invalid;
    }

    const Count slot = sources.get_size();
    sources.insert(Source(&source));
    Elements pending;
    const auto status = collect(source, pending);
    if (status != Status::Success) {
      return status;
    }

    Elements normalized;
    const auto normalization = normalize(pending, normalized);
    if (normalization != Status::Success) {
      return normalization;
    }

    if (normalized.is_empty() && source.get_extent()) {
      return Status::Invalid;
    }

    const Count extent = source.get_extent();
    result = intern(
        extent, normalized.is_empty() ? 1 : alignment, normalized.get_view());
    sources[slot].body = result;
    return Status::Success;
  }

  constexpr auto collect(const Schema& source, Elements& output) -> Status {
    switch (source.get_kind()) {
    case Schema::Kind::Value:
      return Status::Invalid;
    case Schema::Kind::Range:
      return collect_range(source, output);
    case Schema::Kind::Composite:
      return collect_composite(source, output);
    }

    return Status::Invalid;
  }

  constexpr auto prepare_value(const Schema& source, Count& result) -> Status {
    const Count width = Schema::get_width(source.get_value());
    if (!width || source.get_extent() != width) {
      return Status::Invalid;
    }

    if (source.get_alignment() != width) {
      return Status::Invalid;
    }

    const auto order = source.get_byte_order();
    if (order != Schema::ByteOrder::Little && order != Schema::ByteOrder::Big) {
      return Status::Invalid;
    }

    const Count code =
        static_cast<U8>(source.get_value()) |
        ((width > 1 && order == Schema::ByteOrder::Big) ? 64 : 0);
    if (primitive_bodies[code]) {
      result = primitive_bodies[code] - 1;
      return Status::Success;
    }

    const Element entry(1, 0, width, code);
    result = intern(
        width, width, Perimortem::Core::View::Vector<Element>(&entry, 1));
    primitive_bodies[code] = result + 1;
    return Status::Success;
  }

  constexpr auto collect_composite(const Schema& source, Elements& output)
      -> Status {
    const auto positions = source.get_positions();
    if (positions.get_size() && !positions.get_data()) {
      return Status::Invalid;
    }

    for (const auto position : positions) {
      const auto* child = position.get_schema();
      if (!child || position.get_offset() > source.get_extent()) {
        return Status::Invalid;
      }

      if (child->get_extent() > source.get_extent() - position.get_offset()) {
        return Status::Invalid;
      }

      Count body = 0;
      const auto status = prepare(*child, body);
      if (status != Status::Success) {
        return status;
      }

      append(*child, body, position.get_offset(), output);
    }

    return Status::Success;
  }

  // A real Composite remains a referenced body. Value and Range source nodes
  // contribute occurrences directly, so authored batching cannot add a new
  // boundary to the resulting format.
  constexpr auto append(
      const Schema& source,
      Count body,
      Count offset,
      Elements& output) const -> void {
    const auto& ready = bodies[body];
    if (!ready.size) {
      return;
    }

    if (source.get_kind() == Schema::Kind::Composite) {
      output.insert(Element(1, offset, ready.extent, body, True));
      return;
    }

    for (Count i = 0; i < ready.size; ++i) {
      auto entry = elements[ready.first + i];
      entry.offset += offset;
      output.insert(entry);
    }
  }

  constexpr auto collect_range(const Schema& source, Elements& output)
      -> Status {
    const auto range = source.get_range();
    const auto* child = range.get_element();
    if (!child) {
      return Status::Invalid;
    }

    Count body = 0;
    const auto status = prepare(*child, body);
    if (status != Status::Success) {
      return status;
    }

    const Count count = range.get_count();
    const auto ready = bodies[body];
    if (!count || !ready.size) {
      return source.get_extent() ? Status::Invalid : Status::Success;
    }

    // Check the last instance rather than count times stride. A final stride
    // is not occupied storage, and its padding need not be present in extent.
    if (ready.extent > source.get_extent()) {
      return Status::Invalid;
    }

    const Count stride = range.get_stride();
    if (count > 1) {
      if (!stride || stride < ready.extent) {
        return Status::Invalid;
      }

      if (count - 1 > (source.get_extent() - ready.extent) / stride) {
        return count - 1 > (Count(-1) - ready.extent) / stride
                   ? Status::Overflow
                   : Status::Invalid;
      }
    }

    if (child->get_kind() == Schema::Kind::Composite) {
      output.insert(
          Element(count, 0, count == 1 ? ready.extent : stride, body, True));
      return Status::Success;
    }

    if (count == 1) {
      append(*child, body, 0, output);
      return Status::Success;
    }

    // A repeated progression can stay one descriptor when the next batch
    // starts at its next expected element. Otherwise each batch contributes
    // its own runs, as required by the canonical format rather than by the
    // number of nodes in the authored Schema.
    if (ready.size == 1) {
      auto entry = elements[ready.first];
      if (entry.count == 1) {
        entry.count = count;
        entry.stride = stride;
        output.insert(entry);
        return Status::Success;
      }

      // The next batch must start where another element of this run would
      // start. Admission above already fitted at least two whole batches,
      // which also bounds this one past the end stride calculation.
      const Count next_batch = entry.count * entry.stride;
      if (stride == next_batch) {
        entry.count *= count;
        output.insert(entry);
        return Status::Success;
      }
    }

    for (Count i = 0; i < count; ++i) {
      append(*child, body, i * stride, output);
    }

    return Status::Success;
  }

  constexpr auto width(const Element& value) const -> Count {
    return value.composite ? bodies[value.type].extent
                           : Schema::get_width(Schema::Value(value.type & 63));
  }

  // A small heap merges run starts. It also handles interleaved primitive
  // ranges without expanding an entire range just to find the next member.
  // A consumed prefix leaves its remainder in the same inventory.
  static constexpr auto descend(Elements& heap, Count root) -> void {
    for (Count child = root * 2 + 1; child < heap.get_size();
         child = root * 2 + 1) {
      if (child + 1 < heap.get_size() &&
          heap[child + 1].offset < heap[child].offset) {
        ++child;
      }

      if (heap[root].offset <= heap[child].offset) {
        return;
      }

      Perimortem::Core::Data::swap(heap[root], heap[child]);
      root = child;
    }
  }

  static constexpr auto take(Elements& heap) -> Element {
    const auto result = heap[0];
    heap[0] = heap[heap.get_size() - 1];
    heap.resize(heap.get_size() - 1);
    descend(heap, 0);
    return result;
  }

  static constexpr auto insert(Elements& heap, Element value) -> void {
    Count index = heap.get_size();
    heap.insert(value);
    while (index) {
      const Count parent = (index - 1) / 2;
      if (heap[parent].offset <= heap[index].offset) {
        return;
      }

      Perimortem::Core::Data::swap(heap[parent], heap[index]);
      index = parent;
    }
  }

  constexpr auto normalize(Elements& pending, Elements& result) const
      -> Status {
    // Authored records commonly arrive in wire order. Consume that sequence
    // directly. The heap is needed only if a later run starts before the
    // preceding run ends, including otherwise valid interleaved progressions.
    Count end = 0;
    Bool ordered = True;
    for (Count i = 0; i < pending.get_size(); ++i) {
      const auto entry = pending[i];
      if (entry.offset < end) {
        ordered = False;
        break;
      }

      end = entry.offset + (entry.count - 1) * entry.stride + width(entry);
      merge(result, entry);
    }

    if (ordered) {
      return Status::Success;
    }

    result.resize(0);
    for (Count i = pending.get_size() / 2; i; --i) {
      descend(pending, i - 1);
    }

    end = 0;
    while (!pending.is_empty()) {
      auto current = take(pending);
      if (!pending.is_empty() && current.count > 1) {
        const Count next = pending[0].offset;
        if (next > current.offset) {
          const Count delta = next - current.offset;
          const Count prefix =
              delta / current.stride + (delta % current.stride != 0);
          if (prefix < current.count) {
            auto remaining = current;
            remaining.offset += prefix * current.stride;
            remaining.count -= prefix;
            current.count = prefix;
            insert(pending, remaining);
          }
        }
      }

      if (current.offset < end) {
        return Status::Invalid;
      }

      end = current.offset + (current.count - 1) * current.stride +
            width(current);
      merge(result, current);
    }

    return Status::Success;
  }

  // Greedy runs follow occurrence order, not authored range boundaries. When
  // only the first member of an incoming run fits, consume that member and
  // retain the rest as the next candidate. This is what makes 0,4 | 8,16 agree
  // with 0,4,8 | 16 without expanding either input progression.
  constexpr auto merge(Elements& output, Element current) const -> void {
    if (!output.is_empty()) {
      auto& previous = output[output.get_size() - 1];
      const Count stride = previous.count == 1
                               ? current.offset - previous.offset
                               : previous.stride;
      const Bool same = previous.type == current.type &&
                        previous.composite == current.composite;
      const Count distance = current.offset - previous.offset;
      if (same && stride && distance / stride == previous.count &&
          distance % stride == 0) {
        previous.stride = stride;
        ++previous.count;
        --current.count;
        if (!current.count) {
          return;
        }

        current.offset += current.stride;
        if (current.stride == stride) {
          previous.count += current.count;
          return;
        }
      }
    }

    if (current.count == 1) {
      current.stride = width(current);
    }

    output.insert(current);
  }

  static constexpr auto hash(U64 previous, Count value) -> U64 {
    return (previous ^ value) * U64(1099511628211);
  }

  constexpr auto equal(
      const Body& body,
      Perimortem::Core::View::Vector<Element> entries) const -> Bool {
    if (body.size != entries.get_size()) {
      return False;
    }

    for (Count i = 0; i < body.size; ++i) {
      const auto& a = elements[body.first + i];
      const auto& b = entries.get_data()[i];
      if (a.count != b.count || a.offset != b.offset || a.stride != b.stride ||
          a.type != b.type || a.composite != b.composite) {
        return False;
      }
    }

    return True;
  }

  constexpr auto intern(
      Count extent,
      Count alignment,
      Perimortem::Core::View::Vector<Element> entries) -> Count {
    U64 fingerprint = hash(hash(14695981039346656037ULL, extent), alignment);
    for (Count i = 0; i < entries.get_size(); ++i) {
      const auto& entry = entries.get_data()[i];
      fingerprint = hash(fingerprint, entry.count);
      fingerprint = hash(fingerprint, entry.offset);
      fingerprint = hash(fingerprint, entry.stride);
      fingerprint = hash(fingerprint, entry.type);
      fingerprint = hash(fingerprint, entry.composite.value);
    }

    if (buckets.get_size() <= bodies.get_size()) {
      const Count size =
          Perimortem::Core::Math::max(Count(16), buckets.get_size() * 2);
      buckets.resize(size);
      for (Count i = 0; i < size; ++i) {
        buckets[i] = 0;
      }

      for (Count i = 0; i < bodies.get_size(); ++i) {
        const Count bucket = bodies[i].hash % size;
        bodies[i].next = buckets[bucket];
        buckets[bucket] = i + 1;
      }
    }

    const Count bucket = fingerprint % buckets.get_size();
    for (Count id = buckets[bucket]; id; id = bodies[id - 1].next) {
      const auto& body = bodies[id - 1];
      if (body.hash == fingerprint && body.extent == extent &&
          body.alignment == alignment && equal(body, entries)) {
        return id - 1;
      }
    }

    Body body(
        extent, alignment, elements.get_size(), entries.get_size(),
        fingerprint);
    body.next = buckets[bucket];
    for (Count i = 0; i < entries.get_size(); ++i) {
      elements.insert(entries[i]);
    }

    const Count id = bodies.get_size();
    bodies.insert(body);
    buckets[bucket] = id + 1;
    return id;
  }

  constexpr auto number(Count id) -> Status {
    if (bodies[id].block != Count(-1)) {
      return Status::Success;
    }

    const auto body = bodies[id];
    if (body.size >= Count(-1) - block_count) {
      return Status::Overflow;
    }

    bodies[id].block = block_count;
    block_count += 1 + body.size;
    order.insert(id);
    for (Count i = 0; i < body.size; ++i) {
      const auto entry = elements[body.first + i];
      if (entry.composite) {
        const auto status = number(entry.type);
        if (status != Status::Success) {
          return status;
        }
      }
    }

    return Status::Success;
  }

  constexpr auto choose_depth() -> Status {
    Count common = 0;
    Count reference = 0;
    Count extent = 0;
    for (Count i = 0; i < order.get_size(); ++i) {
      const auto& body = bodies[order[i]];
      common = Perimortem::Core::Math::max(common, body.size);
      common = Perimortem::Core::Math::max(common, body.alignment);
      extent = Perimortem::Core::Math::max(extent, body.extent);
      for (Count j = 0; j < body.size; ++j) {
        const auto entry = elements[body.first + j];
        const Count type =
            entry.composite ? bodies[entry.type].block : entry.type;
        common = Perimortem::Core::Math::max(common, entry.count);
        common = Perimortem::Core::Math::max(common, entry.offset);
        common = Perimortem::Core::Math::max(common, entry.stride);
        reference = Perimortem::Core::Math::max(reference, type);
      }
    }

    // Five fields grow by eight bits per depth. P has one fewer bit, while E
    // grows by sixteen with four bits reserved for F. Round each requirement
    // up once, then take the largest. No candidate needs another schema scan.
    // Perimortem's log2 returns the occupied bit count, including zero for zero.
    const auto bits = [](Count value) -> Count {
      return Perimortem::Core::Math::log2(value);
    };
    const Count common_depth = (bits(common) + 7) / 8;
    const Count reference_depth = (bits(reference) + 8) / 8;
    const Count extent_depth = (bits(extent) + 19) / 16;
    const Count required = Perimortem::Core::Math::max(
        common_depth,
        Perimortem::Core::Math::max(reference_depth, extent_depth));
    if (required > 15 || block_count > Count(-1) / (4 * required)) {
      return Status::Overflow;
    }

    depth = U8(required);
    return Status::Success;
  }

  Perimortem::Memory::Const::Vector<Body> bodies;
  Elements elements;
  Perimortem::Memory::Const::Vector<Source> sources;
  Perimortem::Memory::Const::Vector<Count> buckets;
  Perimortem::Memory::Const::Vector<Count> order;
  // The seven bit primitive code includes byte order. Its finite vocabulary
  // gives scalar normalization a direct lookup independent of source identity.
  Perimortem::Core::Static::Vector<Count, 128> primitive_bodies;
  Count block_count = 0;
  U8 depth = 0;
};

}  // namespace Ttx::Data::Form
