// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/core/reader/binary.hpp"

#include "validation/unit_test.hpp"

#include "perimortem/core/static/bytes.hpp"
#include "perimortem/core/static/vector.hpp"
#include "perimortem/core/null_terminated.hpp"
#include "perimortem/core/writer/binary.hpp"

using namespace Perimortem::Core;
using namespace Validation;

static Harness CoreBinaryReader = {
  .name = "Core::Reader::Binary"_view,
};

template <typename T>
static auto expect_read(Option<T> actual, T expected, Test::TestResult& result)
    -> void {
  actual.visit(
      [&] { EXPECT(false); }, [&](T value) { EXPECT_EQ(value, expected); });
}

PERIMORTEM_UNIT_TEST(CoreBinaryReader, little_unsigned) {
  using Reader = Reader::Binary<Data::ByteOrder::Little>;
  Static::Bytes<15> source(
      "\xAB"                                   // Read 1 byte.
      "IREP"                                   // Read 4 bytes.
      "\x34\x12"                               // Read 2 bytes.
      "\xEF\xCD\xAB\x89\x67\x45\x23\x01"_view  // Read 8 bytes.
  );
  Reader reader(source);

  expect_read(reader.read_u8(), U8(0xAB), result);
  expect_read(reader.read_u32(), U32('PERI'), result);
  expect_read(reader.read_u16(), U16(0x1234), result);
  expect_read(reader.read_u64(), U64(0x0123456789ABCDEF), result);
  EXPECT_EQ(reader.get_location(), reader.get_size());
}

PERIMORTEM_UNIT_TEST(CoreBinaryReader, little_endian_signed) {
  using Reader = Reader::Binary<Data::ByteOrder::Little>;
  Static::Bytes<15> source(
      "\xD6"
      "\x18\xFC"
      "\x60\x79\xFE\xFF"
      "\x00\x36\x65\xC4\xFF\xFF\xFF\xFF"_view);
  Reader reader(source);

  expect_read(reader.read_s8(), S8(-42), result);
  expect_read(reader.read_s16(), S16(-1000), result);
  expect_read(reader.read_s32(), S32(-100000), result);
  expect_read(reader.read_s64(), S64(-1000000000LL), result);
  EXPECT_EQ(reader.get_location(), reader.get_size());
}

PERIMORTEM_UNIT_TEST(CoreBinaryReader, little_endian_reals) {
  using Reader = Reader::Binary<Data::ByteOrder::Little>;
  Static::Bytes<12> source(
      "\x00\x00\x40\x40"                       // R32.
      "\x00\x00\x00\x00\x00\x00\xF8\x3F"_view  // R64
  );
  Reader reader(source);

  expect_read(reader.read_r32(), R32(3.0f), result);
  expect_read(reader.read_r64(), R64(1.5), result);
  EXPECT_EQ(reader.get_location(), reader.get_size());
}

PERIMORTEM_UNIT_TEST(CoreBinaryReader, big_endian_unsigned) {
  using Reader = Reader::Binary<Data::ByteOrder::Big>;
  Static::Bytes<15> source(
      "\xAB"                                   // Read 1 byte.
      "PERI"                                   // Read 4 bytes.
      "\x12\x34"                               // Read 2 bytes.
      "\x01\x23\x45\x67\x89\xAB\xCD\xEF"_view  // Read 8 bytes.
  );
  Reader reader(source);

  expect_read(reader.read_u8(), U8(0xAB), result);
  expect_read(reader.read_u32(), U32('PERI'), result);
  expect_read(reader.read_u16(), U16(0x1234), result);
  expect_read(reader.read_u64(), U64(0x0123456789ABCDEF), result);
  EXPECT_EQ(reader.get_location(), reader.get_size());
}

PERIMORTEM_UNIT_TEST(CoreBinaryReader, big_endian_signed) {
  using Reader = Reader::Binary<Data::ByteOrder::Big>;
  Static::Bytes<15> source(
      "\xD6"
      "\xFC\x18"
      "\xFF\xFE\x79\x60"
      "\xFF\xFF\xFF\xFF\xC4\x65\x36\x00"_view);
  Reader reader(source);

  expect_read(reader.read_s8(), S8(-42), result);
  expect_read(reader.read_s16(), S16(-1000), result);
  expect_read(reader.read_s32(), S32(-100000), result);
  expect_read(reader.read_s64(), S64(-1000000000LL), result);
  EXPECT_EQ(reader.get_location(), reader.get_size());
}

PERIMORTEM_UNIT_TEST(CoreBinaryReader, big_endian_reals) {
  using Reader = Reader::Binary<Data::ByteOrder::Big>;
  Static::Bytes<12> source(
      "\x40\x40\x00\x00"                       // R32.
      "\x3F\xF8\x00\x00\x00\x00\x00\x00"_view  // R64
  );
  Reader reader(source);

  expect_read(reader.read_r32(), R32(3.0f), result);
  expect_read(reader.read_r64(), R64(1.5), result);
  EXPECT_EQ(reader.get_location(), reader.get_size());
}

PERIMORTEM_UNIT_TEST(CoreBinaryReader, raw_bytes) {
  using Reader = Reader::Binary<Data::ByteOrder::Big>;
  Reader reader("Hello, World!"_view);

  expect_read(reader.read_bytes(5), "Hello"_view, result);
  EXPECT_EQ(reader.get_location(), Count(5));
  reader.read_bytes(2);
  expect_read(reader.read_bytes(5), "World"_view, result);
  EXPECT_NOT(reader.read_bytes(12));
  EXPECT_EQ(reader.get_location(), Count(12));
  expect_read(reader.read_bytes(1), "!"_view, result);
}

PERIMORTEM_UNIT_TEST(CoreBinaryReader, overflow_read) {
  using Reader = Reader::Binary<Data::ByteOrder::Little>;
  Reader reader("\xAB\xCD"_view);

  // A wider read cannot consume part of its scalar. The caller can retry at
  // the same position with a type that fits the remaining bytes.
  EXPECT_NOT(reader.read_u32());
  EXPECT_EQ(reader.get_location(), Count(0));
  expect_read(reader.read_u8(), U8(0xAB), result);

  EXPECT_NOT(reader.read_u16());
  EXPECT_EQ(reader.get_location(), Count(1));
  expect_read(reader.read_u8(), U8(0xCD), result);
}

PERIMORTEM_UNIT_TEST(CoreBinaryReader, set_location) {
  using Reader = Reader::Binary<Data::ByteOrder::Little>;
  Static::Bytes<6> source("\x0A\x00\x14\x00\x1E\x00"_view);
  Reader reader(source);

  expect_read(reader.read_u16(), U16(0x0A), result);

  // Read from an arbitrary byte offset. Binary readers do not realign.
  reader.set_location(1);
  expect_read(reader.read_u16(), U16(0x1400), result);
}

PERIMORTEM_UNIT_TEST(CoreBinaryReader, invalid_pointer) {
  using Reader = Reader::Binary<Data::ByteOrder::Little>;
  Reader reader("\x0A\x00"_view);

  reader.set_location(Count(-1));
  EXPECT_EQ(reader.get_location(), Count(-1));
  EXPECT_NOT(reader.read_u16());
  EXPECT_EQ(reader.get_location(), Count(-1));
}

PERIMORTEM_UNIT_TEST(CoreBinaryReader, multiple_readers) {
  using Reader = Reader::Binary<Data::ByteOrder::Little>;
  Static::Bytes<6> source("\x01\x00\x02\x00\x03\x00"_view);
  Reader readers[] = {Reader(source), Reader(source)};

  expect_read(readers[0].read_u16(), U16(1), result);
  expect_read(readers[1].read_u16(), U16(1), result);
  EXPECT_EQ(readers[0].get_location(), Count(2));
  EXPECT_EQ(readers[1].get_location(), Count(2));

  expect_read(readers[0].read_u16(), U16(0x02), result);
  EXPECT_EQ(readers[0].get_location(), Count(4));
  EXPECT_EQ(readers[1].get_location(), Count(2));
}

// Zero and an empty view are answers, including an empty view at the end of
// input. Absence means the requested bytes were unavailable. Exercise this
// distinction during constant evaluation as well as through the runtime API.
static constexpr auto optional_reads() -> Bool {
  const U8 bytes[] = {0, 0, 0, 0};
  auto reader = Reader::Binary<Data::ByteOrder::Little>(View::Bytes(bytes));
  const auto zero = reader.read_u32();
  const auto empty = reader.read_bytes(0);
  if (!zero || *zero != 0 || !empty || !empty->is_empty()) {
    return False;
  }

  if (reader.read_u8() || reader.get_location() != sizeof(bytes)) {
    return False;
  }

  const auto retry = reader.read_bytes(0);
  if (!retry || !retry->is_empty()) {
    return False;
  }

  reader.reset();
  const auto restored = reader.read_u32();
  return restored && *restored == 0;
}

static_assert(optional_reads());

PERIMORTEM_UNIT_TEST(CoreBinaryReader, optional_values) {
  EXPECT(optional_reads());

  auto reader = Reader::Binary<Data::ByteOrder::Little>(View::Bytes());
  EXPECT_NOT(reader.read_u8());
  EXPECT_NOT(reader.read_u16());
  EXPECT_NOT(reader.read_u32());
  EXPECT_NOT(reader.read_u64());
  EXPECT_NOT(reader.read_s8());
  EXPECT_NOT(reader.read_s16());
  EXPECT_NOT(reader.read_s32());
  EXPECT_NOT(reader.read_s64());
  EXPECT_NOT(reader.read_r32());
  EXPECT_NOT(reader.read_r64());
  EXPECT_EQ(reader.get_location(), Count(0));
}

static Harness CoreBinaryWriter = {
  .name = "Core::Writer::Binary"_view,
};

PERIMORTEM_UNIT_TEST(CoreBinaryWriter, little_unsigned) {
  using Writer = Writer::Binary<Data::ByteOrder::Little>;
  Static::Bytes<15> buffer;
  Writer writer(buffer);

  writer << U8(0xAB) << U16(0x1234) << U32('PERI') << U64(0x0123456789ABCDEF);

  EXPECT(writer.is_valid());
  EXPECT_EQ(writer.get_location(), Count(15));
  EXPECT_HEX(
      buffer,
      "\xAB"
      "\x34\x12"
      "IREP"
      "\xEF\xCD\xAB\x89\x67\x45\x23\x01"_view);
}

PERIMORTEM_UNIT_TEST(CoreBinaryWriter, little_endian_signed) {
  using Writer = Writer::Binary<Data::ByteOrder::Little>;
  Static::Bytes<15> buffer;
  Writer writer(buffer);

  writer << S8(-42) << S16(-1000) << S32(-100000) << S64(-1000000000LL);

  EXPECT(writer.is_valid());
  EXPECT_HEX(
      buffer,
      "\xD6"
      "\x18\xFC"
      "\x60\x79\xFE\xFF"
      "\x00\x36\x65\xC4\xFF\xFF\xFF\xFF"_view);
}

PERIMORTEM_UNIT_TEST(CoreBinaryWriter, little_endian_reals) {
  // R32 value 3 uses bits 0x40400000 and R64 value 1.5 uses bits
  // 0x3FF8000000000000.
  using Writer = Writer::Binary<Data::ByteOrder::Little>;
  Static::Bytes<12> buffer;
  Writer writer(buffer);

  writer << R32(3.0f) << R64(1.5);

  EXPECT(writer.is_valid());
  EXPECT_HEX(
      buffer,
      "\x00\x00\x40\x40"
      "\x00\x00\x00\x00\x00\x00\xF8\x3F"_view);
}

PERIMORTEM_UNIT_TEST(CoreBinaryWriter, big_endian_unsigned) {
  using Writer = Writer::Binary<Data::ByteOrder::Big>;
  Static::Bytes<15> buffer;
  Writer writer(buffer);

  writer << U8(0xAB) << U16(0x1234) << U32('PERI') << U64(0x0123456789ABCDEF);

  EXPECT(writer.is_valid());
  EXPECT_EQ(writer.get_location(), Count(15));
  EXPECT_HEX(
      buffer,
      "\xAB"
      "\x12\x34"
      "PERI"
      "\x01\x23\x45\x67\x89\xAB\xCD\xEF"_view);
}

PERIMORTEM_UNIT_TEST(CoreBinaryWriter, big_endian_signed) {
  using Writer = Writer::Binary<Data::ByteOrder::Big>;
  Static::Bytes<15> buffer;
  Writer writer(buffer);

  writer << S8(-42) << S16(-1000) << S32(-100000) << S64(-1000000000LL);

  EXPECT(writer.is_valid());
  EXPECT_HEX(
      buffer,
      "\xD6"
      "\xFC\x18"
      "\xFF\xFE\x79\x60"
      "\xFF\xFF\xFF\xFF\xC4\x65\x36\x00"_view);
}

PERIMORTEM_UNIT_TEST(CoreBinaryWriter, big_endian_reals) {
  // R32 value 3 uses bits 0x40400000 and R64 value 1.5 uses bits
  // 0x3FF8000000000000.
  using Writer = Writer::Binary<Data::ByteOrder::Big>;
  Static::Bytes<12> buffer;
  Writer writer(buffer);

  writer << R32(3.0f) << R64(1.5);

  EXPECT(writer.is_valid());
  EXPECT_HEX(
      buffer,
      "\x40\x40\x00\x00"
      "\x3F\xF8\x00\x00\x00\x00\x00\x00"_view);
}

PERIMORTEM_UNIT_TEST(CoreBinaryWriter, big_endian_vector) {
  using Writer = Writer::Binary<Data::ByteOrder::Big>;
  Static::Vector<U16, 3> values = {{
    U16(0x0102),
    U16(0x0304),
    U16(0x0506),
  }};
  Static::Bytes<6> buffer;
  Writer writer(buffer);

  writer << values.get_view();

  EXPECT(writer.is_valid());
  EXPECT_HEX(buffer, "\x01\x02\x03\x04\x05\x06"_view);
}

PERIMORTEM_UNIT_TEST(CoreBinaryWriter, raw_bytes) {
  using Writer = Writer::Binary<Data::ByteOrder::Native>;
  Static::Bytes<9> buffer;
  Writer writer(buffer);

  writer << "blob data"_view;

  EXPECT(writer.is_valid());
  EXPECT_TEXT(buffer, "blob data"_view);
}

PERIMORTEM_UNIT_TEST(CoreBinaryWriter, overflow) {
  using Writer = Writer::Binary<Data::ByteOrder::Native>;
  Static::Bytes<3> buffer;
  Writer writer(buffer);

  writer << U32('PERI');

  EXPECT_NOT(writer.is_valid());
}

PERIMORTEM_UNIT_TEST(CoreBinaryWriter, set_pointer) {
  using Writer = Writer::Binary<Data::ByteOrder::Little>;
  Static::Bytes<6> buffer;
  Writer writer(buffer);

  writer << U16(0x0A0B) << U16(0x0C0D);
  EXPECT_EQ(writer.get_location(), Count(4));

  writer.set_pointer(0);
  writer << U16(0x1234);

  EXPECT(writer.is_valid());
  EXPECT_HEX(
      buffer,
      "\x34\x12"
      "\x0D\x0C\0\0"_view);
}

PERIMORTEM_UNIT_TEST(CoreBinaryWriter, multiple_writers) {
  using Writer = Writer::Binary<Data::ByteOrder::Little>;
  Static::Bytes<4> buffer;
  Writer writers[] = {
    Writer(buffer),
    Writer(buffer),
  };

  writers[0] << U16(0xAAAA);
  // Both writers begin at zero, so the second writer replaces the first value.
  writers[1] << U16(0xBBBB);
  // The first writer retains its own cursor and continues at position two.
  writers[0] << U16(0xCCCC);

  EXPECT(writers[0].is_valid());
  EXPECT(writers[1].is_valid());
  EXPECT_HEX(buffer, "\xBB\xBB\xCC\xCC"_view);
}
