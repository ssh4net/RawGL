// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_io.h"

#include "tiff_backend.h"

#include <GL/glew.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#if __has_include(<tiffio.h>)
#    include <tiffio.h>
#    define RAWGL_TEST_HAS_TIFFIO 1
#endif

static void
write_u16_sample(std::vector<std::byte>& bytes, const size_t sampleIndex, const uint16_t value)
{
    std::memcpy(bytes.data() + sampleIndex * sizeof(uint16_t), &value, sizeof(value));
}

static uint16_t
make_sample_value(const int x, const int y, const int channel)
{
    const uint32_t value = static_cast<uint32_t>(x * 257 + y * 911 + channel * 12345);
    return static_cast<uint16_t>(value & 0xffffu);
}

static rawgl::HostImageData
make_test_image()
{
    rawgl::HostImageData image;
    image.width = 17;
    image.height = 19;
    image.channels = 3;
    image.alphaChannel = -1;
    image.glInternalFormat = GL_RGB16;
    image.glType = GL_UNSIGNED_SHORT;
    image.bytes.resize(static_cast<size_t>(image.width) * static_cast<size_t>(image.height)
                       * static_cast<size_t>(image.channels) * sizeof(uint16_t));

    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            for (int channel = 0; channel < image.channels; ++channel) {
                const size_t sampleIndex =
                    (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x))
                        * static_cast<size_t>(image.channels)
                    + static_cast<size_t>(channel);
                write_u16_sample(image.bytes, sampleIndex, make_sample_value(x, y, channel));
            }
        }
    }

    return image;
}

static bool
save_tiled_tiff(const std::filesystem::path& path, const rawgl::HostImageData& image)
{
    std::map<std::string, std::string> attributes;
    attributes.insert({ "tiff:compression", "none" });
    attributes.insert({ "tiff:layout", "tiled" });
    attributes.insert({ "tiff:tile_width", "16" });
    attributes.insert({ "tiff:tile_height", "16" });

    rawgl::io::ImageEncodeSettings settings;
    settings.codec = rawgl::io::ImageCodecFamily::Tiff;
    settings.componentType = rawgl::io::ImageComponentType::U16;

    std::string errorMessage;
    if (!rawgl::io::encode_tiff_file(path.string(), attributes, -1, image, settings, errorMessage)) {
        std::cerr << "Tiled TIFF save failed: " << errorMessage << std::endl;
        return false;
    }

    return true;
}

static bool
save_stripped_tiff(const std::filesystem::path& path,
                   const rawgl::HostImageData& image,
                   uint16_t& expectedCompression)
{
    std::map<std::string, std::string> attributes;
    attributes.insert({ "tiff:layout", "strips" });
    attributes.insert({ "tiff:rows_per_strip", "4" });

#if defined(RAWGL_TEST_HAS_TIFFIO)
#    if defined(COMPRESSION_ADOBE_DEFLATE)
    expectedCompression = COMPRESSION_ADOBE_DEFLATE;
#    else
    expectedCompression = COMPRESSION_DEFLATE;
#    endif
    if (TIFFIsCODECConfigured(expectedCompression) != 0) {
        attributes.insert({ "tiff:compression", "deflate" });
        attributes.insert({ "tiff:zip_level", "6" });
        attributes.insert({ "tiff:predictor", "horizontal" });
    } else {
        expectedCompression = COMPRESSION_NONE;
        attributes.insert({ "tiff:compression", "none" });
    }
#else
    expectedCompression = 1u;
    attributes.insert({ "tiff:compression", "none" });
#endif

    rawgl::io::ImageEncodeSettings settings;
    settings.codec = rawgl::io::ImageCodecFamily::Tiff;
    settings.componentType = rawgl::io::ImageComponentType::U16;

    std::string errorMessage;
    if (!rawgl::io::encode_tiff_file(path.string(), attributes, -1, image, settings, errorMessage)) {
        std::cerr << "Stripped TIFF save failed: " << errorMessage << std::endl;
        return false;
    }

    return true;
}

static bool
verify_tiled_tiff_layout(const std::filesystem::path& path)
{
#if defined(RAWGL_TEST_HAS_TIFFIO)
    TIFF* tif = TIFFOpen(path.string().c_str(), "r");
    if (tif == nullptr) {
        std::cerr << "Failed to reopen TIFF for tile verification." << std::endl;
        return false;
    }

    if (TIFFIsTiled(tif) == 0) {
        TIFFClose(tif);
        std::cerr << "TIFF output is not tiled." << std::endl;
        return false;
    }

    uint32_t tileWidth = 0u;
    uint32_t tileLength = 0u;
    TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tileWidth);
    TIFFGetField(tif, TIFFTAG_TILELENGTH, &tileLength);
    TIFFClose(tif);

    if (tileWidth != 16u || tileLength != 16u) {
        std::cerr << "Unexpected TIFF tile geometry." << std::endl;
        return false;
    }
#else
    (void)path;
#endif

    return true;
}

static bool
verify_stripped_tiff_layout(const std::filesystem::path& path, const uint16_t expectedCompression)
{
#if defined(RAWGL_TEST_HAS_TIFFIO)
    TIFF* tif = TIFFOpen(path.string().c_str(), "r");
    if (tif == nullptr) {
        std::cerr << "Failed to reopen TIFF for strip verification." << std::endl;
        return false;
    }

    if (TIFFIsTiled(tif) != 0) {
        TIFFClose(tif);
        std::cerr << "TIFF output is tiled but stripped output was requested." << std::endl;
        return false;
    }

    uint32_t rowsPerStrip = 0u;
    uint16_t compression = 0u;
    TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip);
    TIFFGetField(tif, TIFFTAG_COMPRESSION, &compression);

    uint16_t predictor = PREDICTOR_NONE;
    TIFFGetFieldDefaulted(tif, TIFFTAG_PREDICTOR, &predictor);
    TIFFClose(tif);

    if (rowsPerStrip != 4u) {
        std::cerr << "Unexpected TIFF rows-per-strip value." << std::endl;
        return false;
    }
    if (compression != expectedCompression) {
        std::cerr << "Unexpected TIFF compression value." << std::endl;
        return false;
    }
    if (expectedCompression != COMPRESSION_NONE && predictor != PREDICTOR_HORIZONTAL) {
        std::cerr << "Unexpected TIFF predictor value." << std::endl;
        return false;
    }
#else
    (void)path;
    (void)expectedCompression;
#endif

    return true;
}

static bool
verify_round_trip(const std::filesystem::path& path, const rawgl::HostImageData& source)
{
    const rawgl::io::DecodedImageData result = rawgl::io::decode_tiff_file(path.string());
    if (!result.success) {
        std::cerr << "TIFF reload failed: " << result.errorMessage << std::endl;
        return false;
    }

    if (result.width != source.width || result.height != source.height || result.channels != source.channels
        || result.alphaChannel != source.alphaChannel || result.componentType != rawgl::io::ImageComponentType::U16) {
        std::cerr << "Reloaded TIFF shape differs from source." << std::endl;
        return false;
    }

    if (result.bytes != source.bytes) {
        std::cerr << "Reloaded TIFF pixels differ from source." << std::endl;
        return false;
    }

    return true;
}

int
main()
{
    const std::filesystem::path tiledOutputPath = "tests/outputs/rawgl_io_tiff_native_tiled_u16.tif";
    const std::filesystem::path strippedOutputPath = "tests/outputs/rawgl_io_tiff_native_stripped_u16.tif";

    std::error_code removeError;
    std::filesystem::remove(tiledOutputPath, removeError);
    std::filesystem::remove(strippedOutputPath, removeError);

    const rawgl::HostImageData image = make_test_image();
    if (!save_tiled_tiff(tiledOutputPath, image)) {
        return 1;
    }
    if (!verify_tiled_tiff_layout(tiledOutputPath)) {
        return 1;
    }
    if (!verify_round_trip(tiledOutputPath, image)) {
        return 1;
    }

    uint16_t expectedCompression = 0u;
    if (!save_stripped_tiff(strippedOutputPath, image, expectedCompression)) {
        return 1;
    }
    if (!verify_stripped_tiff_layout(strippedOutputPath, expectedCompression)) {
        return 1;
    }
    if (!verify_round_trip(strippedOutputPath, image)) {
        return 1;
    }

    return 0;
}
