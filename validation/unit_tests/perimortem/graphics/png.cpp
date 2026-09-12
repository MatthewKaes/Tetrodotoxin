// # Tetrodotoxin
// Copyright (c) 2023-present Matt Kaes and contributors

#include "perimortem/graphics/formats/png.hpp"

#include "validation/unit_test.hpp"

#include "perimortem/core/static/bytes.hpp"
#include "perimortem/core/algorithm/search.hpp"
#include "perimortem/core/null_terminated.hpp"

#include "perimortem/system/file.hpp"

using namespace Perimortem::Core;
using namespace Perimortem::Graphics;
using namespace Perimortem::Memory;
using namespace Perimortem::System;
using namespace Validation;

static Harness GraphicsPng = {
  .name = "Graphics::Png"_view,
};

PERIMORTEM_UNIT_TEST(GraphicsPng, red_1x1_dimensions) {
  auto source = File::read("validation/data/pngs/red_1x1.png"_view);
  ASSERT(source);
  ASSERT_NOT((*source).is_empty());

  auto image = Formats::Png::decode(*source);

  EXPECT_EQ(image.get_width(), U32(1));
  EXPECT_EQ(image.get_height(), U32(1));
  EXPECT_EQ(image.get_color_depth(), U8(8));

  auto pixels = image.get_pixels();
  ASSERT_EQ(pixels.get_size(), Count(1));
  EXPECT_EQ(pixels.get_data()[0].red, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[0].green, U8(0x00));
  EXPECT_EQ(pixels.get_data()[0].blue, U8(0x00));
  EXPECT_EQ(pixels.get_data()[0].alpha, U8(0xFF));
}

PERIMORTEM_UNIT_TEST(GraphicsPng, checkerboard_2x2) {
  auto source = File::read("validation/data/pngs/checkerboard_2x2.png"_view);
  ASSERT(source);
  ASSERT_NOT((*source).is_empty());

  auto image = Formats::Png::decode(*source);
  auto pixels = image.get_pixels();

  ASSERT_EQ(pixels.get_size(), Count(4));
  EXPECT_EQ(pixels.get_data()[0].red, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[0].green, U8(0x00));
  EXPECT_EQ(pixels.get_data()[0].blue, U8(0x00));
  EXPECT_EQ(pixels.get_data()[0].alpha, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[1].red, U8(0x00));
  EXPECT_EQ(pixels.get_data()[1].green, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[1].blue, U8(0x00));
  EXPECT_EQ(pixels.get_data()[1].alpha, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[2].red, U8(0x00));
  EXPECT_EQ(pixels.get_data()[2].green, U8(0x00));
  EXPECT_EQ(pixels.get_data()[2].blue, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[2].alpha, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[3].red, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[3].green, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[3].blue, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[3].alpha, U8(0xFF));
}

PERIMORTEM_UNIT_TEST(GraphicsPng, unaligned_input) {
  auto source = File::read("validation/data/pngs/red_1x1.png"_view);
  ASSERT(source);

  // A PNG can arrive as a slice of another byte buffer. Move this fixture by
  // one byte so its header and chunk integers cannot rely on native alignment.
  // Sanitized builds must accept that input and still decode the same pixel.
  Dynamic::Vector<U8> storage;
  storage.resize(source->get_size() + 1);
  Data::copy(
      storage.get_data() + 1, source->get_view().get_data(), source->get_size());

  const View::Bytes shifted(storage.get_data() + 1, source->get_size());
  auto image = Formats::Png::decode(shifted);
  const auto pixels = image.get_pixels();

  ASSERT_EQ(pixels.get_size(), Count(1));
  EXPECT_EQ(pixels[0].red, U8(0xFF));
  EXPECT_EQ(pixels[0].green, U8(0));
  EXPECT_EQ(pixels[0].blue, U8(0));
  EXPECT_EQ(pixels[0].alpha, U8(0xFF));
}

PERIMORTEM_UNIT_TEST(GraphicsPng, decode_rgb_to_rgba) {
  auto source = File::read("validation/data/pngs/rgb_3x1.png"_view);
  ASSERT(source);
  ASSERT_NOT((*source).is_empty());

  auto image = Formats::Png::decode(*source);
  auto pixels = image.get_pixels();

  // RGB source: alpha must be synthesized as fully opaque.
  ASSERT_EQ(pixels.get_size(), Count(3));
  EXPECT_EQ(pixels.get_data()[0].red, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[0].alpha, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[1].green, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[1].alpha, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[2].blue, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[2].alpha, U8(0xFF));
}

PERIMORTEM_UNIT_TEST(GraphicsPng, gray_to_rgba) {
  auto source = File::read("validation/data/pngs/gray_2x2.png"_view);
  ASSERT(source);
  ASSERT_NOT((*source).is_empty());

  auto image = Formats::Png::decode(*source);
  auto pixels = image.get_pixels();

  // Greyscale source: gray value replicates to all three color channels.
  ASSERT_EQ(pixels.get_size(), Count(4));
  EXPECT_EQ(pixels.get_data()[0].red, U8(0x00));
  EXPECT_EQ(pixels.get_data()[0].green, U8(0x00));
  EXPECT_EQ(pixels.get_data()[0].blue, U8(0x00));
  EXPECT_EQ(pixels.get_data()[0].alpha, U8(0xFF));

  EXPECT_EQ(pixels.get_data()[1].red, U8(0x40));
  EXPECT_EQ(pixels.get_data()[1].green, U8(0x40));
  EXPECT_EQ(pixels.get_data()[1].blue, U8(0x40));
  EXPECT_EQ(pixels.get_data()[1].alpha, U8(0xFF));

  EXPECT_EQ(pixels.get_data()[2].red, U8(0x80));
  EXPECT_EQ(pixels.get_data()[2].green, U8(0x80));
  EXPECT_EQ(pixels.get_data()[2].blue, U8(0x80));
  EXPECT_EQ(pixels.get_data()[2].alpha, U8(0xFF));

  EXPECT_EQ(pixels.get_data()[3].red, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[3].green, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[3].blue, U8(0xFF));
  EXPECT_EQ(pixels.get_data()[3].alpha, U8(0xFF));
}

PERIMORTEM_UNIT_TEST(GraphicsPng, decode_gradient_4x4) {
  auto source = File::read("validation/data/pngs/gradient_4x4.png"_view);
  ASSERT(source);
  ASSERT_NOT((*source).is_empty());

  auto image = Formats::Png::decode(*source);
  auto pixels = image.get_pixels();

  ASSERT_EQ(pixels.get_size(), Count(16));
  EXPECT_EQ(pixels.get_data()[0].red, U8(0));
  EXPECT_EQ(pixels.get_data()[0].green, U8(0));
  EXPECT_EQ(pixels.get_data()[0].blue, U8(128));
  EXPECT_EQ(pixels.get_data()[15].red, U8(255));
  EXPECT_EQ(pixels.get_data()[15].green, U8(255));
  EXPECT_EQ(pixels.get_data()[15].blue, U8(128));
}

PERIMORTEM_UNIT_TEST(GraphicsPng, decode_pattern_8x1) {
  auto source = File::read("validation/data/pngs/pattern_8x1.png"_view);
  ASSERT(source);
  ASSERT_NOT((*source).is_empty());

  auto image = Formats::Png::decode(*source);
  auto pixels = image.get_pixels();

  ASSERT_EQ(pixels.get_size(), Count(8));
  EXPECT_EQ(pixels.get_data()[0].red, U8(100));
  EXPECT_EQ(pixels.get_data()[0].green, U8(200));
  EXPECT_EQ(pixels.get_data()[0].blue, U8(50));
  EXPECT_EQ(pixels.get_data()[0].alpha, U8(255));
  EXPECT_EQ(pixels.get_data()[4].red, U8(100));
  EXPECT_EQ(pixels.get_data()[4].green, U8(200));
}

PERIMORTEM_UNIT_TEST(GraphicsPng, decode_invalid) {
  constexpr Static::Bytes<8> garbage = {
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}};
  auto image = Formats::Png::decode(garbage);
  EXPECT_EQ(image.get_width(), U32(0));
  EXPECT_EQ(image.get_height(), U32(0));
}

PERIMORTEM_UNIT_TEST(GraphicsPng, roundtrip_1x1) {
  Dynamic::Vector<Pixel> source_pixels;
  source_pixels.insert(Pixel::from_rgba(0x12, 0x34, 0x56, 0x78));
  Image source_image(Data::take(source_pixels), 1, 1);

  auto encoded = Formats::Png::encode(source_image);
  ASSERT(encoded.get_size() > 0);

  auto decoded = Formats::Png::decode(encoded.get_view());
  EXPECT_EQ(decoded.get_width(), U32(1));
  EXPECT_EQ(decoded.get_height(), U32(1));

  auto decoded_pixels = decoded.get_pixels();
  ASSERT_EQ(decoded_pixels.get_size(), Count(1));
  EXPECT_EQ(decoded_pixels.get_data()[0].red, U8(0x12));
  EXPECT_EQ(decoded_pixels.get_data()[0].green, U8(0x34));
  EXPECT_EQ(decoded_pixels.get_data()[0].blue, U8(0x56));
  EXPECT_EQ(decoded_pixels.get_data()[0].alpha, U8(0x78));
}

PERIMORTEM_UNIT_TEST(GraphicsPng, roundtrip_checker) {
  Dynamic::Vector<Pixel> source_pixels;
  source_pixels.insert(Pixel::from_rgba(0xFF, 0x00, 0x00, 0xFF));
  source_pixels.insert(Pixel::from_rgba(0x00, 0xFF, 0x00, 0xFF));
  source_pixels.insert(Pixel::from_rgba(0x00, 0x00, 0xFF, 0xFF));
  source_pixels.insert(Pixel::from_rgba(0xFF, 0xFF, 0xFF, 0xFF));
  Image source_image(Data::take(source_pixels), 2, 2);

  auto encoded = Formats::Png::encode(source_image);
  ASSERT(encoded.get_size() > 0);

  auto decoded = Formats::Png::decode(encoded.get_view());
  EXPECT_EQ(decoded.get_width(), U32(2));
  EXPECT_EQ(decoded.get_height(), U32(2));

  auto decoded_pixels = decoded.get_pixels();
  ASSERT_EQ(decoded_pixels.get_size(), Count(4));
  EXPECT_EQ(decoded_pixels.get_data()[0].red, U8(0xFF));
  EXPECT_EQ(decoded_pixels.get_data()[1].green, U8(0xFF));
  EXPECT_EQ(decoded_pixels.get_data()[2].blue, U8(0xFF));
  EXPECT_EQ(decoded_pixels.get_data()[3].red, U8(0xFF));
}

PERIMORTEM_UNIT_TEST(GraphicsPng, roundtrip_64x64) {
  constexpr Count width = 1 << 6;
  constexpr Count height = 1 << 6;
  Dynamic::Vector<Pixel> source_pixels;
  source_pixels.resize(width * height);
  for (Count row = 0; row < height; row++) {
    for (Count col = 0; col < width; col++) {
      source_pixels[row * width + col] =
          Pixel::from_rgba(U8(col * 8), U8(row * 8), U8(128), U8(255));
    }
  }

  Image source_image(Data::take(source_pixels), width, height);

  auto encoded = Formats::Png::encode(source_image);
  ASSERT(encoded.get_size() > 0);

  auto decoded = Formats::Png::decode(encoded.get_view());
  auto source_view = source_image.get_pixels();
  auto decoded_pixels = decoded.get_pixels();
  const auto* source_data = source_view.get_data();
  const auto* decoded_data = decoded_pixels.get_data();
  ASSERT_EQ(decoded_pixels.get_size(), Count(width * height));
  for (Count pixel_index = 0; pixel_index < width * height; pixel_index++) {
    EXPECT_EQ(decoded_data[pixel_index].red, source_data[pixel_index].red);
    EXPECT_EQ(decoded_data[pixel_index].green, source_data[pixel_index].green);
    EXPECT_EQ(decoded_data[pixel_index].blue, source_data[pixel_index].blue);
    EXPECT_EQ(decoded_data[pixel_index].alpha, source_data[pixel_index].alpha);
  }
}

PERIMORTEM_UNIT_TEST(GraphicsPng, zero_dimensions) {
  Dynamic::Vector<Pixel> empty;
  Image zero_width(Data::take(empty), 0, 1);
  Dynamic::Vector<Pixel> empty2;
  Image zero_height(Data::take(empty2), 1, 0);
  auto encoded_w = Formats::Png::encode(zero_width);
  auto encoded_h = Formats::Png::encode(zero_height);
  EXPECT_EQ(encoded_w.get_size(), 0);
  EXPECT_EQ(encoded_h.get_size(), 0);
}

PERIMORTEM_UNIT_TEST(GraphicsPng, chunk_header_trunc) {
  // PNG with a valid IHDR that's shorter than the valid size.
  auto source =
      File::read("validation/data/pngs/error_truncated_header.png"_view);
  ASSERT(source);
  ASSERT_NOT((*source).is_empty());

  auto image = Formats::Png::decode(*source);

  EXPECT_EQ(image.get_width(), 0);
  EXPECT(
      Test::error_contains(
          "Png: Chunk at offset 33 truncated before header end"_view));
}

PERIMORTEM_UNIT_TEST(GraphicsPng, chunk_length_overrun) {
  // PNG with a chunk that claims a length of 4294967295 bytes.
  auto source = File::read("validation/data/pngs/error_overrun.png"_view);
  ASSERT(source);
  ASSERT_NOT((*source).is_empty());

  auto image = Formats::Png::decode(*source);

  EXPECT_EQ(image.get_width(), 0);
  EXPECT(
      Test::error_contains(
          "Png: Chunk at offset 33 with length 100 extends past end of stream"_view));
}

PERIMORTEM_UNIT_TEST(GraphicsPng, roundtrip_icon) {
  auto source = File::read("validation/data/pngs/perimortem_icon.png"_view);
  ASSERT(source);
  ASSERT_NOT((*source).is_empty());

  auto original = Formats::Png::decode(*source);
  ASSERT_EQ(original.get_width(), 128);
  ASSERT_EQ(original.get_height(), 128);

  auto original_pixels = original.get_pixels();
  ASSERT_EQ(original_pixels.get_size(), Count(128 * 128));

  auto encoded = Formats::Png::encode(original);
  ASSERT(encoded.get_size() > 0);

  auto decoded = Formats::Png::decode(encoded.get_view());
  ASSERT_EQ(decoded.get_width(), 128);
  ASSERT_EQ(decoded.get_height(), 128);

  auto decoded_pixels = decoded.get_pixels();
  const auto* decoded_data = decoded_pixels.get_data();
  const auto* original_data = original_pixels.get_data();
  ASSERT_EQ(decoded_pixels.get_size(), Count(128 * 128));
  for (Count i = 0; i < decoded_pixels.get_size(); i++) {
    EXPECT_EQ(decoded_data[i].red, original_data[i].red);
    EXPECT_EQ(decoded_data[i].green, original_data[i].green);
    EXPECT_EQ(decoded_data[i].blue, original_data[i].blue);
    EXPECT_EQ(decoded_data[i].alpha, original_data[i].alpha);
  }
}

#if PERI_DEBUG
PERIMORTEM_UNIT_TEST(GraphicsPng, crc_mismatch) {
  // PNG with corrupted chunk CRC should log the chunk type and offset so the
  // caller can identify which chunk was damaged.
  auto source = File::read("validation/data/pngs/error_crc_mismatch.png"_view);
  ASSERT(source);
  ASSERT_NOT((*source).is_empty());

  auto image = Formats::Png::decode(*source);

  EXPECT_EQ(image.get_width(), 0);
  EXPECT(
      Test::error_contains(
          "Png: CRC-32 mismatch for chunk 'IHDR' at offset 8"_view));
}
#endif
