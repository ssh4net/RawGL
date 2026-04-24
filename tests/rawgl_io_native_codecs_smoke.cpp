// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_io.h"

#include "exr_backend.h"
#include "jpg_backend.h"
#include "png_backend.h"

#include <GL/glew.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#if __has_include(<jpeglib.h>)
#    include <jpeglib.h>
#    define RAWGL_TEST_HAS_JPEGLIB 1
#endif

#if __has_include(<OpenEXR/ImfTiledInputFile.h>)
#    include <OpenEXR/ImfTiledInputFile.h>
#    define RAWGL_TEST_HAS_OPENEXR_TILES 1
#endif

static void
write_u8_sample(std::vector<std::byte>& bytes, const size_t sampleIndex, const uint8_t value)
{
    bytes[sampleIndex] = static_cast<std::byte>(value);
}

static void
write_u16_sample(std::vector<std::byte>& bytes, const size_t sampleIndex, const uint16_t value)
{
    std::memcpy(bytes.data() + sampleIndex * sizeof(uint16_t), &value, sizeof(value));
}

static void
write_f32_sample(std::vector<std::byte>& bytes, const size_t sampleIndex, const float value)
{
    std::memcpy(bytes.data() + sampleIndex * sizeof(float), &value, sizeof(value));
}

static uint16_t
make_u16_sample_value(const int x, const int y, const int channel)
{
    const uint32_t value = static_cast<uint32_t>(x * 1021 + y * 313 + channel * 8191);
    return static_cast<uint16_t>(value & 0xffffu);
}

static uint8_t
make_u8_sample_value(const int x, const int y, const int channel)
{
    const uint32_t value = static_cast<uint32_t>(x * 17 + y * 29 + channel * 73);
    return static_cast<uint8_t>(value & 0xffu);
}

static float
make_f32_sample_value(const int x, const int y, const int channel)
{
    return static_cast<float>(x) * 0.03125f + static_cast<float>(y) * 0.0625f
           + static_cast<float>(channel) * 0.125f;
}

static rawgl::HostImageData
make_u16_rgb_image(const int width, const int height)
{
    rawgl::HostImageData image;
    image.width = width;
    image.height = height;
    image.channels = 3;
    image.alphaChannel = -1;
    image.glInternalFormat = GL_RGB16;
    image.glType = GL_UNSIGNED_SHORT;
    image.bytes.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3u * sizeof(uint16_t));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                const size_t sampleIndex =
                    (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3u
                    + static_cast<size_t>(channel);
                write_u16_sample(image.bytes, sampleIndex, make_u16_sample_value(x, y, channel));
            }
        }
    }

    return image;
}

static rawgl::HostImageData
make_u8_rgb_image(const int width, const int height)
{
    rawgl::HostImageData image;
    image.width = width;
    image.height = height;
    image.channels = 3;
    image.alphaChannel = -1;
    image.glInternalFormat = GL_RGB8;
    image.glType = GL_UNSIGNED_BYTE;
    image.bytes.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3u);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                const size_t sampleIndex =
                    (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3u
                    + static_cast<size_t>(channel);
                write_u8_sample(image.bytes, sampleIndex, make_u8_sample_value(x, y, channel));
            }
        }
    }

    return image;
}

static rawgl::HostImageData
make_f32_rgb_image(const int width, const int height)
{
    rawgl::HostImageData image;
    image.width = width;
    image.height = height;
    image.channels = 3;
    image.alphaChannel = -1;
    image.glInternalFormat = GL_RGB32F;
    image.glType = GL_FLOAT;
    image.bytes.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 3u * sizeof(float));

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            for (int channel = 0; channel < 3; ++channel) {
                const size_t sampleIndex =
                    (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3u
                    + static_cast<size_t>(channel);
                write_f32_sample(image.bytes, sampleIndex, make_f32_sample_value(x, y, channel));
            }
        }
    }

    return image;
}

static bool
verify_decoded_shape(const rawgl::io::DecodedImageData& decoded,
                     const int width,
                     const int height,
                     const int channels,
                     const rawgl::io::ImageComponentType componentType,
                     const char* label)
{
    if (!decoded.success) {
        std::cerr << label << " decode failed: " << decoded.errorMessage << std::endl;
        return false;
    }

    if (decoded.width != width || decoded.height != height || decoded.channels != channels
        || decoded.componentType != componentType) {
        std::cerr << label << " decoded shape or component type is unexpected." << std::endl;
        return false;
    }

    return true;
}

static bool
verify_png_direct(const std::filesystem::path& path)
{
    const rawgl::HostImageData source = make_u16_rgb_image(13, 11);
    rawgl::io::ImageEncodeSettings settings;
    settings.codec = rawgl::io::ImageCodecFamily::Png;
    settings.componentType = rawgl::io::ImageComponentType::U16;

    std::map<std::string, std::string> attributes;
    attributes.insert({ "png:compressionLevel", "0" });
    attributes.insert({ "png:interlace", "true" });

    std::string errorMessage;
    if (!rawgl::io::encode_png_file(path.string(), attributes, -1, source, settings, errorMessage)) {
        std::cerr << "PNG encode failed: " << errorMessage << std::endl;
        return false;
    }

    const rawgl::io::DecodedImageData decoded = rawgl::io::decode_png_file(path.string());
    if (!verify_decoded_shape(decoded, source.width, source.height, source.channels, settings.componentType, "PNG")) {
        return false;
    }

    if (decoded.bytes != source.bytes) {
        std::cerr << "PNG round-trip bytes differ from source." << std::endl;
        return false;
    }

    return true;
}

static bool
verify_jpeg_direct(const std::filesystem::path& path)
{
    const rawgl::HostImageData source = make_u8_rgb_image(19, 17);

    std::map<std::string, std::string> attributes;
    attributes.insert({ "jpeg:quality", "97" });
    attributes.insert({ "jpeg:progressive", "true" });

    std::string errorMessage;
    if (!rawgl::io::encode_jpg_file(path.string(), attributes, -1, source, errorMessage)) {
        std::cerr << "JPEG encode failed: " << errorMessage << std::endl;
        return false;
    }

    const rawgl::io::DecodedImageData decoded = rawgl::io::decode_jpg_file(path.string());
    if (!verify_decoded_shape(decoded, source.width, source.height, source.channels, rawgl::io::ImageComponentType::U8, "JPEG")) {
        return false;
    }

    if (decoded.bytes.empty()) {
        std::cerr << "JPEG decoded bytes are empty." << std::endl;
        return false;
    }

#if defined(RAWGL_TEST_HAS_JPEGLIB)
    FILE* file = std::fopen(path.string().c_str(), "rb");
    if (file == nullptr) {
        std::cerr << "Failed to reopen JPEG for marker verification." << std::endl;
        return false;
    }

    jpeg_decompress_struct cinfo {};
    jpeg_error_mgr errorManager {};
    cinfo.err = jpeg_std_error(&errorManager);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, file);
    jpeg_read_header(&cinfo, TRUE);
    const bool progressive = cinfo.progressive_mode != 0;
    jpeg_destroy_decompress(&cinfo);
    std::fclose(file);

    if (!progressive) {
        std::cerr << "JPEG output is not progressive." << std::endl;
        return false;
    }
#endif

    return true;
}

static bool
verify_exr_direct(const std::filesystem::path& path)
{
    const rawgl::HostImageData source = make_f32_rgb_image(9, 7);
    rawgl::io::ImageEncodeSettings settings;
    settings.codec = rawgl::io::ImageCodecFamily::Exr;
    settings.componentType = rawgl::io::ImageComponentType::F32;

    std::map<std::string, std::string> attributes;
    attributes.insert({ "openexr:compression", "zip" });
    attributes.insert({ "openexr:tiled", "true" });
    attributes.insert({ "openexr:tileWidth", "8" });
    attributes.insert({ "openexr:tileHeight", "8" });
    attributes.insert({ "openexr:attribute:string:RawGLTest", "native-codecs" });

    std::string errorMessage;
    if (!rawgl::io::encode_exr_file(path.string(), attributes, -1, source, settings, errorMessage)) {
        std::cerr << "OpenEXR encode failed: " << errorMessage << std::endl;
        return false;
    }

    const rawgl::io::DecodedImageData decoded = rawgl::io::decode_exr_file(path.string());
    if (!verify_decoded_shape(decoded, source.width, source.height, source.channels, settings.componentType, "OpenEXR")) {
        return false;
    }

    if (decoded.bytes != source.bytes) {
        std::cerr << "OpenEXR round-trip bytes differ from source." << std::endl;
        return false;
    }

#if defined(RAWGL_TEST_HAS_OPENEXR_TILES)
    try {
        OPENEXR_IMF_NAMESPACE::TiledInputFile input(path.string().c_str());
        if (input.tileXSize() != 8u || input.tileYSize() != 8u) {
            std::cerr << "OpenEXR tile geometry is unexpected." << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to reopen tiled OpenEXR: " << e.what() << std::endl;
        return false;
    }
#endif

    return true;
}

int
main()
{
    const std::filesystem::path pngPath = "tests/outputs/rawgl_io_native_codecs_u16.png";
    const std::filesystem::path jpegPath = "tests/outputs/rawgl_io_native_codecs_progressive.jpg";
    const std::filesystem::path exrPath = "tests/outputs/rawgl_io_native_codecs_tiled.exr";

    std::error_code removeError;
    std::filesystem::remove(pngPath, removeError);
    std::filesystem::remove(jpegPath, removeError);
    std::filesystem::remove(exrPath, removeError);

    if (!verify_png_direct(pngPath)) {
        return 1;
    }
    if (!verify_jpeg_direct(jpegPath)) {
        return 1;
    }
    if (!verify_exr_direct(exrPath)) {
        return 1;
    }

    return 0;
}
