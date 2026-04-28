// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_io.h"

#include "png_backend.h"

#include <GL/glew.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <vector>

static rawgl::HostImageData
make_u8_rgb_image()
{
    rawgl::HostImageData image;
    image.width = 4;
    image.height = 4;
    image.channels = 3;
    image.alphaChannel = -1;
    image.glInternalFormat = GL_RGB8;
    image.glType = GL_UNSIGNED_BYTE;
    image.bytes.resize(static_cast<size_t>(image.width) * static_cast<size_t>(image.height)
                       * static_cast<size_t>(image.channels));

    for (size_t index = 0; index < image.bytes.size(); ++index) {
        image.bytes[index] = static_cast<std::byte>((index * 37u) & 0xffu);
    }

    return image;
}

static bool
expect_save_failure(const std::filesystem::path& path,
                    const rawgl::HostImageData& image,
                    const int bits,
                    const std::vector<rawgl::Attribute>& attributes,
                    const char* label)
{
    std::error_code removeError;
    std::filesystem::remove(path, removeError);

    rawgl::io::ImageSaveRequest request;
    request.path = path.string();
    request.bits = bits;
    request.attributes = attributes;
    request.image = image;

    const rawgl::io::ImageSaveResult result = rawgl::io::SaveImageFile(request);
    if (result.success) {
        std::cerr << label << " unexpectedly succeeded." << std::endl;
        return false;
    }
    if (result.errorMessage.empty()) {
        std::cerr << label << " failed without an error message." << std::endl;
        return false;
    }

    std::filesystem::remove(path, removeError);
    return true;
}

static bool
expect_png_backend_failure(const std::filesystem::path& path, const rawgl::HostImageData& image)
{
    std::error_code removeError;
    std::filesystem::remove(path, removeError);

    rawgl::io::ImageEncodeSettings settings;
    settings.codec = rawgl::io::ImageCodecFamily::Png;
    settings.componentType = rawgl::io::ImageComponentType::F32;

    std::map<std::string, std::string> attributes;
    std::string errorMessage;
    if (rawgl::io::encode_png_file(path.string(), attributes, -1, image, settings, errorMessage)) {
        std::cerr << "PNG unsupported component type unexpectedly succeeded." << std::endl;
        return false;
    }
    if (errorMessage.empty()) {
        std::cerr << "PNG unsupported component type failed without an error message." << std::endl;
        return false;
    }

    std::filesystem::remove(path, removeError);
    return true;
}

int
main()
{
    const rawgl::HostImageData image = make_u8_rgb_image();

    if (!expect_save_failure("tests/outputs/rawgl_io_error_tiff_tile.tif",
                             image,
                             16,
                             { { "tiff:tiled", "true" }, { "tiff:tile_width", "0" } },
                             "Invalid TIFF tile width")) {
        return 1;
    }

    if (!expect_save_failure("tests/outputs/rawgl_io_error_tiff_layout.tif",
                             image,
                             16,
                             { { "tiff:layout", "not_a_layout" } },
                             "Invalid TIFF layout")) {
        return 1;
    }

    if (!expect_save_failure("tests/outputs/rawgl_io_error_tiff_layout_conflict.tif",
                             image,
                             16,
                             { { "tiff:layout", "strips" }, { "tiff:tile_width", "16" } },
                             "Conflicting TIFF layout")) {
        return 1;
    }

    if (!expect_save_failure("tests/outputs/rawgl_io_error_tiff_rows_with_tiles.tif",
                             image,
                             16,
                             { { "tiff:layout", "tiled" }, { "tiff:rows_per_strip", "4" } },
                             "TIFF rows-per-strip with tiles")) {
        return 1;
    }

    if (!expect_save_failure("tests/outputs/rawgl_io_error_tiff_compression.tif",
                             image,
                             16,
                             { { "tiff:compression", "not_a_codec" } },
                             "Invalid TIFF compression")) {
        return 1;
    }

    if (!expect_save_failure("tests/outputs/rawgl_io_error_tiff_predictor.tif",
                             image,
                             16,
                             { { "tiff:predictor", "not_a_predictor" } },
                             "Invalid TIFF predictor")) {
        return 1;
    }

    if (!expect_save_failure("tests/outputs/rawgl_io_error_tiff_zip_level.tif",
                             image,
                             16,
                             { { "tiff:compression", "deflate" }, { "tiff:zip_level", "42" } },
                             "Invalid TIFF Deflate level")) {
        return 1;
    }

    if (!expect_save_failure("tests/outputs/rawgl_io_error_tiff_wrong_level.tif",
                             image,
                             16,
                             { { "tiff:compression", "lzw" }, { "tiff:zip_level", "5" } },
                             "Wrong TIFF compression level")) {
        return 1;
    }

    if (!expect_png_backend_failure("tests/outputs/rawgl_io_error_png_f32.png", image)) {
        return 1;
    }

    if (!expect_save_failure("tests/outputs/rawgl_io_error_exr_compression.exr",
                             image,
                             16,
                             { { "openexr:compression", "not_a_codec" } },
                             "Invalid OpenEXR compression")) {
        return 1;
    }

    return 0;
}
