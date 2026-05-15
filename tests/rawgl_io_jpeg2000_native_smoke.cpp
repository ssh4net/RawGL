// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_io.h"

#include <GL/glew.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

struct Jpeg2000TestCase {
    const char* label = "";
    const char* path = "";
    int width = 0;
    int height = 0;
    int channels = 0;
    int alphaChannel = -1;
    int bits = 8;
    uint32_t glInternalFormat = GL_RGB8;
    uint32_t glType = GL_UNSIGNED_BYTE;
    bool exactRoundTrip = true;
    bool lossless = true;
    bool hasCompressionRatio = false;
    float compressionRatio = 0.0f;
    bool hasQuality = false;
    float quality = 0.0f;
    bool injectConflictingLosslessAttribute = false;
};

static void
write_u16_sample(std::vector<std::byte>& bytes, const size_t sampleIndex, const uint16_t value)
{
    std::memcpy(bytes.data() + sampleIndex * sizeof(uint16_t), &value, sizeof(value));
}

static uint8_t
make_u8_sample_value(const int x, const int y, const int channel)
{
    const uint32_t value = static_cast<uint32_t>(x * 29 + y * 71 + channel * 113);
    return static_cast<uint8_t>(value & 0xffu);
}

static uint16_t
make_u16_sample_value(const int x, const int y, const int channel)
{
    const uint32_t value = static_cast<uint32_t>(x * 733u + y * 1291u + channel * 8191u);
    return static_cast<uint16_t>(value & 0xffffu);
}

static rawgl::HostImageData
make_test_image(const Jpeg2000TestCase& testCase)
{
    rawgl::HostImageData image;
    image.width = testCase.width;
    image.height = testCase.height;
    image.channels = testCase.channels;
    image.alphaChannel = testCase.alphaChannel;
    image.glInternalFormat = testCase.glInternalFormat;
    image.glType = testCase.glType;

    const size_t sampleCount = static_cast<size_t>(image.width) * static_cast<size_t>(image.height)
                               * static_cast<size_t>(image.channels);
    const size_t bytesPerSample = image.glType == GL_UNSIGNED_SHORT ? sizeof(uint16_t) : sizeof(uint8_t);
    image.bytes.resize(sampleCount * bytesPerSample);

    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            for (int channel = 0; channel < image.channels; ++channel) {
                const size_t sampleIndex =
                    (static_cast<size_t>(y) * static_cast<size_t>(image.width) + static_cast<size_t>(x))
                        * static_cast<size_t>(image.channels)
                    + static_cast<size_t>(channel);
                if (image.glType == GL_UNSIGNED_SHORT) {
                    write_u16_sample(image.bytes, sampleIndex, make_u16_sample_value(x, y, channel));
                } else {
                    image.bytes[sampleIndex] = static_cast<std::byte>(make_u8_sample_value(x, y, channel));
                }
            }
        }
    }

    return image;
}

static rawgl::io::ImageCodecSaveOptions
make_save_options(const Jpeg2000TestCase& testCase)
{
    rawgl::io::Jpeg2000SaveOptions jpeg2000;
    jpeg2000.hasLossless = true;
    jpeg2000.lossless = testCase.lossless;
    jpeg2000.hasCompressionRatio = testCase.hasCompressionRatio;
    jpeg2000.compressionRatio = testCase.compressionRatio;
    jpeg2000.hasQuality = testCase.hasQuality;
    jpeg2000.quality = testCase.quality;

    rawgl::io::ImageCodecSaveOptions codecOptions;
    codecOptions.hasJpeg2000 = true;
    codecOptions.jpeg2000 = jpeg2000;
    return codecOptions;
}

static rawgl::io::ImageCodecLoadOptions
make_load_options()
{
    rawgl::io::Jpeg2000LoadOptions jpeg2000;
    jpeg2000.hasReduceFactor = true;
    jpeg2000.reduceFactor = 0;
    jpeg2000.hasLayerLimit = true;
    jpeg2000.layerLimit = 0;

    rawgl::io::ImageCodecLoadOptions codecOptions;
    codecOptions.hasBackendPolicy = true;
    codecOptions.backendPolicy = rawgl::io::ImageLoadBackendPolicy::NativeOnly;
    codecOptions.hasJpeg2000 = true;
    codecOptions.jpeg2000 = jpeg2000;
    return codecOptions;
}

static bool
save_jpeg2000_case(const Jpeg2000TestCase& testCase, const rawgl::HostImageData& image)
{
    rawgl::io::ImageSaveRequest request;
    request.path = testCase.path;
    request.bits = testCase.bits;
    request.codecOptions = make_save_options(testCase);
    request.image = image;
    if (testCase.injectConflictingLosslessAttribute) {
        request.attributes = { { "jpeg2000:lossless", testCase.lossless ? "false" : "true" } };
    }

    const rawgl::io::ImageSaveResult result = rawgl::io::SaveImageFile(request);
    if (!result.success) {
        std::cerr << testCase.label << " JPEG-2000 save failed: " << result.errorMessage << std::endl;
        return false;
    }

    return true;
}

static bool
load_jpeg2000_case(const Jpeg2000TestCase& testCase, rawgl::HostImageData& image)
{
    rawgl::io::ImageLoadRequest request;
    request.path = testCase.path;
    request.codecOptions = make_load_options();

    const rawgl::io::ImageLoadResult result = rawgl::io::LoadImageFile(request);
    if (!result.success) {
        std::cerr << testCase.label << " JPEG-2000 load failed: " << result.errorMessage << std::endl;
        return false;
    }

    image = result.image;
    return true;
}

static bool
verify_loaded_shape(const Jpeg2000TestCase& testCase, const rawgl::HostImageData& loaded)
{
    if (loaded.width != testCase.width || loaded.height != testCase.height || loaded.channels != testCase.channels
        || loaded.glType != testCase.glType || loaded.alphaChannel != testCase.alphaChannel) {
        std::cerr << testCase.label << " JPEG-2000 reloaded shape, alpha, or component type is unexpected."
                  << std::endl;
        return false;
    }
    if (loaded.bytes.empty()) {
        std::cerr << testCase.label << " JPEG-2000 reloaded payload is empty." << std::endl;
        return false;
    }

    return true;
}

static bool
verify_case_result(const Jpeg2000TestCase& testCase,
                   const rawgl::HostImageData& loaded,
                   const rawgl::HostImageData& source)
{
    if (!verify_loaded_shape(testCase, loaded)) {
        return false;
    }
    if (testCase.exactRoundTrip && loaded.bytes != source.bytes) {
        std::cerr << testCase.label << " JPEG-2000 lossless round-trip bytes differ from source." << std::endl;
        return false;
    }

    return true;
}

static bool
run_jpeg2000_case(const Jpeg2000TestCase& testCase)
{
    const std::filesystem::path outputPath = testCase.path;
    std::error_code error;
    std::filesystem::create_directories(outputPath.parent_path(), error);
    std::filesystem::remove(outputPath, error);

    const rawgl::HostImageData source = make_test_image(testCase);
    if (!save_jpeg2000_case(testCase, source)) {
        return false;
    }

    rawgl::HostImageData loaded;
    if (!load_jpeg2000_case(testCase, loaded)) {
        return false;
    }

    return verify_case_result(testCase, loaded, source);
}

static bool
expect_jpeg2000_save_failure(const char* label,
                             const char* path,
                             const rawgl::HostImageData& image,
                             const std::vector<rawgl::Attribute>& attributes)
{
    std::error_code error;
    std::filesystem::remove(path, error);

    rawgl::io::ImageSaveRequest request;
    request.path = path;
    request.bits = 8;
    request.attributes = attributes;
    request.image = image;

    const rawgl::io::ImageSaveResult result = rawgl::io::SaveImageFile(request);
    if (result.success) {
        std::cerr << label << " JPEG-2000 invalid save unexpectedly succeeded." << std::endl;
        return false;
    }
    if (result.errorMessage.empty()) {
        std::cerr << label << " JPEG-2000 invalid save failed without an error message." << std::endl;
        return false;
    }

    std::filesystem::remove(path, error);
    return true;
}

static bool
expect_jpeg2000_load_failure(const char* label,
                             const char* path,
                             const std::vector<rawgl::Attribute>& attributes)
{
    rawgl::io::ImageLoadRequest request;
    request.path = path;
    request.attributes = attributes;
    request.codecOptions.hasBackendPolicy = true;
    request.codecOptions.backendPolicy = rawgl::io::ImageLoadBackendPolicy::NativeOnly;

    const rawgl::io::ImageLoadResult result = rawgl::io::LoadImageFile(request);
    if (result.success) {
        std::cerr << label << " JPEG-2000 invalid load unexpectedly succeeded." << std::endl;
        return false;
    }
    if (result.errorMessage.empty()) {
        std::cerr << label << " JPEG-2000 invalid load failed without an error message." << std::endl;
        return false;
    }

    return true;
}

static bool
run_jpeg2000_error_cases()
{
    const rawgl::HostImageData image = make_test_image(Jpeg2000TestCase { "jp2-error-source",
                                                                          "",
                                                                          16,
                                                                          16,
                                                                          3,
                                                                          -1,
                                                                          8,
                                                                          GL_RGB8,
                                                                          GL_UNSIGNED_BYTE,
                                                                          false,
                                                                          true,
                                                                          false,
                                                                          0.0f,
                                                                          false,
                                                                          0.0f,
                                                                          false });
    const char* validPath = "tests/outputs/rawgl_io_jpeg2000_error_valid.jp2";

    if (!expect_jpeg2000_save_failure("invalid lossless flag",
                                      "tests/outputs/rawgl_io_jpeg2000_error_lossless.jp2",
                                      image,
                                      { { "jpeg2000:lossless", "maybe" } })) {
        return false;
    }
    if (!expect_jpeg2000_save_failure("conflicting rate and quality",
                                      "tests/outputs/rawgl_io_jpeg2000_error_rate_quality.jp2",
                                      image,
                                      { { "jpeg2000:lossless", "false" },
                                        { "jpeg2000:compression_ratio", "12" },
                                        { "jpeg2000:quality", "35" } })) {
        return false;
    }
    if (!expect_jpeg2000_save_failure("lossless with rate",
                                      "tests/outputs/rawgl_io_jpeg2000_error_lossless_rate.jp2",
                                      image,
                                      { { "jpeg2000:lossless", "true" }, { "jpeg2000:rate", "8" } })) {
        return false;
    }
    if (!expect_jpeg2000_save_failure("lossy missing rate",
                                      "tests/outputs/rawgl_io_jpeg2000_error_lossy_missing_rate.jp2",
                                      image,
                                      { { "jpeg2000:lossless", "false" } })) {
        return false;
    }
    if (!expect_jpeg2000_save_failure("invalid quality",
                                      "tests/outputs/rawgl_io_jpeg2000_error_quality.jp2",
                                      image,
                                      { { "jpeg2000:lossless", "false" }, { "jpeg2000:quality", "0" } })) {
        return false;
    }

    Jpeg2000TestCase validCase;
    validCase.label = "jp2-error-valid";
    validCase.path = validPath;
    validCase.width = 16;
    validCase.height = 16;
    validCase.channels = 3;
    validCase.alphaChannel = -1;
    validCase.bits = 8;
    validCase.glInternalFormat = GL_RGB8;
    validCase.glType = GL_UNSIGNED_BYTE;
    validCase.exactRoundTrip = true;
    validCase.lossless = true;
    if (!save_jpeg2000_case(validCase, image)) {
        return false;
    }
    if (!expect_jpeg2000_load_failure("invalid reduce factor",
                                      validPath,
                                      { { "jpeg2000:reduce_factor", "not_a_number" } })) {
        return false;
    }
    if (!expect_jpeg2000_load_failure("invalid layer limit",
                                      validPath,
                                      { { "jpeg2000:layer_limit", "not_a_number" } })) {
        return false;
    }

    return true;
}

int
main()
{
    const Jpeg2000TestCase testCases[] = {
        { "jp2-rgb16-lossless",
          "tests/outputs/rawgl_io_jpeg2000_native_rgb16_lossless.jp2",
          31,
          23,
          3,
          -1,
          16,
          GL_RGB16,
          GL_UNSIGNED_SHORT,
          true,
          true,
          false,
          0.0f,
          false,
          0.0f,
          true },
        { "j2k-gray8-lossless",
          "tests/outputs/rawgl_io_jpeg2000_native_gray8_lossless.j2k",
          37,
          29,
          1,
          -1,
          8,
          GL_R8,
          GL_UNSIGNED_BYTE,
          true,
          true,
          false,
          0.0f,
          false,
          0.0f,
          false },
        { "j2c-rgba8-lossless",
          "tests/outputs/rawgl_io_jpeg2000_native_rgba8_lossless.j2c",
          41,
          33,
          4,
          3,
          8,
          GL_RGBA8,
          GL_UNSIGNED_BYTE,
          true,
          true,
          false,
          0.0f,
          false,
          0.0f,
          false },
        { "jp2-rgb8-lossy-rate",
          "tests/outputs/rawgl_io_jpeg2000_native_rgb8_lossy_rate.jp2",
          64,
          48,
          3,
          -1,
          8,
          GL_RGB8,
          GL_UNSIGNED_BYTE,
          false,
          false,
          true,
          12.0f,
          false,
          0.0f,
          false },
        { "j2k-rgb8-lossy-quality",
          "tests/outputs/rawgl_io_jpeg2000_native_rgb8_lossy_quality.j2k",
          64,
          48,
          3,
          -1,
          8,
          GL_RGB8,
          GL_UNSIGNED_BYTE,
          false,
          false,
          false,
          0.0f,
          true,
          35.0f,
          false },
    };

    for (const Jpeg2000TestCase& testCase : testCases) {
        if (!run_jpeg2000_case(testCase)) {
            return 1;
        }
    }
    if (!run_jpeg2000_error_cases()) {
        return 1;
    }

    return 0;
}
