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
    attributes.insert({ "tiff:tiled", "true" });
    attributes.insert({ "tiff:tileWidth", "16" });
    attributes.insert({ "tiff:tileLength", "16" });

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
verify_tiff_layout(const std::filesystem::path& path)
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
verify_round_trip(const std::filesystem::path& path, const rawgl::HostImageData& source)
{
    const rawgl::io::DecodedImageData result = rawgl::io::decode_tiff_file(path.string());
    if (!result.success) {
        std::cerr << "Tiled TIFF reload failed: " << result.errorMessage << std::endl;
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
    const std::filesystem::path outputPath = "tests/outputs/rawgl_io_tiff_native_tiled_u16.tif";

    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    const rawgl::HostImageData image = make_test_image();
    if (!save_tiled_tiff(outputPath, image)) {
        return 1;
    }
    if (!verify_tiff_layout(outputPath)) {
        return 1;
    }
    if (!verify_round_trip(outputPath, image)) {
        return 1;
    }

    return 0;
}
