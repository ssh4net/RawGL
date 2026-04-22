// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "jpg_backend.h"

#include "gl_utils.h"

#include <algorithm>
#include <array>
#include <csetjmp>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#if defined(RAWGL_HAS_LIBJPEG)
#include <jpeglib.h>
#endif

namespace rawgl::io {
namespace {

#if defined(RAWGL_HAS_LIBJPEG)
struct JpegErrorState {
    jpeg_error_mgr base;
    jmp_buf jumpBuffer;
    char message[JMSG_LENGTH_MAX];
};

static bool
host_is_little_endian()
{
    const uint16_t value = 1u;
    return *reinterpret_cast<const uint8_t*>(&value) == 1u;
}

static void
jpeg_error_exit(j_common_ptr commonPtr)
{
    JpegErrorState* state = reinterpret_cast<JpegErrorState*>(commonPtr->err);
    (*commonPtr->err->format_message)(commonPtr, state->message);
    longjmp(state->jumpBuffer, 1);
}

static void
initialize_jpeg_error_state(JpegErrorState& state)
{
    jpeg_std_error(&state.base);
    state.base.error_exit = jpeg_error_exit;
    state.message[0] = '\0';
}

static int
parse_jpeg_quality(const std::map<std::string, std::string>& attributes)
{
    const auto explicitJpeg = attributes.find("jpeg:quality");
    if (explicitJpeg != attributes.end()) {
        return std::clamp(std::atoi(explicitJpeg->second.c_str()), 1, 100);
    }

    const auto explicitJpg = attributes.find("jpg:quality");
    if (explicitJpg != attributes.end()) {
        return std::clamp(std::atoi(explicitJpg->second.c_str()), 1, 100);
    }

    const auto legacyCompression = attributes.find("oiio:Compression");
    if (legacyCompression != attributes.end()) {
        const std::string& value = legacyCompression->second;
        const std::string jpegPrefix = "jpeg:";
        const std::string jpgPrefix = "jpg:";
        if (value.rfind(jpegPrefix, 0) == 0) {
            return std::clamp(std::atoi(value.substr(jpegPrefix.size()).c_str()), 1, 100);
        }
        if (value.rfind(jpgPrefix, 0) == 0) {
            return std::clamp(std::atoi(value.substr(jpgPrefix.size()).c_str()), 1, 100);
        }
    }

    return 95;
}

static bool
parse_jpeg_progressive(const std::map<std::string, std::string>& attributes)
{
    auto it = attributes.find("jpeg:progressive");
    if (it == attributes.end()) {
        it = attributes.find("jpg:progressive");
    }
    if (it == attributes.end()) {
        return false;
    }

    const std::string& value = it->second;
    return value == "1" || value == "true" || value == "True" || value == "TRUE";
}

static float
half_to_float(const uint16_t value)
{
    const uint32_t sign = static_cast<uint32_t>(value & 0x8000u) << 16u;
    uint32_t exponent = static_cast<uint32_t>(value & 0x7c00u) >> 10u;
    uint32_t mantissa = static_cast<uint32_t>(value & 0x03ffu);

    uint32_t bits = 0u;
    if (exponent == 0u) {
        if (mantissa != 0u) {
            exponent = 1u;
            while ((mantissa & 0x0400u) == 0u) {
                mantissa <<= 1u;
                --exponent;
            }
            mantissa &= 0x03ffu;
            bits = sign | ((exponent + 112u) << 23u) | (mantissa << 13u);
        } else {
            bits = sign;
        }
    } else if (exponent == 31u) {
        bits = sign | 0x7f800000u | (mantissa << 13u);
    } else {
        bits = sign | ((exponent + 112u) << 23u) | (mantissa << 13u);
    }

    float result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

static float
read_source_sample(const HostImageData& image, const size_t sampleIndex, bool& supported)
{
    supported = true;

    switch (image.glType) {
    case GL_UNSIGNED_BYTE: {
        const uint8_t value = reinterpret_cast<const uint8_t*>(image.bytes.data())[sampleIndex];
        return static_cast<float>(value) / 255.0f;
    }
    case GL_UNSIGNED_SHORT: {
        uint16_t value = 0u;
        std::memcpy(&value, image.bytes.data() + sampleIndex * sizeof(uint16_t), sizeof(value));
        if (!host_is_little_endian()) {
            value = static_cast<uint16_t>((value >> 8u) | (value << 8u));
        }
        return static_cast<float>(value) / 65535.0f;
    }
    case GL_FLOAT: {
        float value = 0.0f;
        std::memcpy(&value, image.bytes.data() + sampleIndex * sizeof(float), sizeof(value));
        return value;
    }
    case GL_HALF_FLOAT: {
        uint16_t value = 0u;
        std::memcpy(&value, image.bytes.data() + sampleIndex * sizeof(uint16_t), sizeof(value));
        if (!host_is_little_endian()) {
            value = static_cast<uint16_t>((value >> 8u) | (value << 8u));
        }
        return half_to_float(value);
    }
    default: break;
    }

    supported = false;
    return 0.0f;
}

static uint8_t
quantize_unit_float_to_u8(float value)
{
    if (!std::isfinite(value)) {
        value = 0.0f;
    }
    value = std::clamp(value, 0.0f, 1.0f);
    return static_cast<uint8_t>(value * 255.0f + 0.5f);
}

static bool
convert_host_image_to_jpeg_bytes(const HostImageData& image,
                                 int explicitAlphaChannel,
                                 int& outputChannels,
                                 std::vector<uint8_t>& bytes,
                                 std::string& errorMessage)
{
    if (image.width <= 0 || image.height <= 0 || image.channels < 1 || image.channels > 4) {
        errorMessage = "invalid host image dimensions or channel count for JPEG";
        return false;
    }

    const int resolvedAlphaChannel = explicitAlphaChannel >= 0 ? explicitAlphaChannel : image.alphaChannel;
    if (image.channels == 1 || image.channels == 3) {
        outputChannels = image.channels;
    } else if ((image.channels == 2 || image.channels == 4) && resolvedAlphaChannel >= 0
               && resolvedAlphaChannel < image.channels) {
        outputChannels = image.channels - 1;
    } else if (image.channels == 2) {
        outputChannels = 1;
    } else if (image.channels == 4) {
        outputChannels = 3;
    } else {
        errorMessage = "unsupported JPEG channel layout";
        return false;
    }

    if (outputChannels != 1 && outputChannels != 3) {
        errorMessage = "native JPEG write only supports grayscale or RGB output";
        return false;
    }

    const size_t pixelCount = static_cast<size_t>(image.width) * static_cast<size_t>(image.height);
    bytes.resize(pixelCount * static_cast<size_t>(outputChannels));

    for (size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
        size_t destinationComponent = 0u;
        for (int sourceChannel = 0; sourceChannel < image.channels; ++sourceChannel) {
            if (sourceChannel == resolvedAlphaChannel) {
                continue;
            }
            if (destinationComponent >= static_cast<size_t>(outputChannels)) {
                break;
            }
            const size_t sampleIndex = pixelIndex * static_cast<size_t>(image.channels)
                                       + static_cast<size_t>(sourceChannel);
            bool supported = false;
            const float value = read_source_sample(image, sampleIndex, supported);
            if (!supported) {
                errorMessage = "unsupported host image type for native JPEG write";
                return false;
            }
            bytes[pixelIndex * static_cast<size_t>(outputChannels) + destinationComponent] =
                quantize_unit_float_to_u8(value);
            ++destinationComponent;
        }
    }

    return true;
}

static bool
close_file(FILE* file)
{
    if (file != nullptr) {
        fclose(file);
    }
    return true;
}
#endif

}  // namespace

DecodedImageData
decode_jpg_file(const std::string& path)
{
    DecodedImageData result;

#if !defined(RAWGL_HAS_LIBJPEG)
    result.errorMessage = "libjpeg support is not available";
    return result;
#else
    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        result.errorMessage = "can't open JPEG file";
        return result;
    }

    jpeg_decompress_struct cinfo {};
    JpegErrorState errorState {};
    initialize_jpeg_error_state(errorState);
    cinfo.err = &errorState.base;

    if (setjmp(errorState.jumpBuffer) != 0) {
        jpeg_destroy_decompress(&cinfo);
        close_file(file);
        result.errorMessage = errorState.message[0] != '\0' ? errorState.message : "libjpeg read failed";
        return result;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, file);
    jpeg_read_header(&cinfo, TRUE);

    if (cinfo.jpeg_color_space == JCS_GRAYSCALE) {
        cinfo.out_color_space = JCS_GRAYSCALE;
    } else if (cinfo.jpeg_color_space == JCS_RGB || cinfo.jpeg_color_space == JCS_YCbCr) {
        cinfo.out_color_space = JCS_RGB;
    } else {
        jpeg_destroy_decompress(&cinfo);
        close_file(file);
        result.errorMessage = "unsupported JPEG color space";
        return result;
    }

    jpeg_start_decompress(&cinfo);

    const int width = static_cast<int>(cinfo.output_width);
    const int height = static_cast<int>(cinfo.output_height);
    const int channels = static_cast<int>(cinfo.output_components);
    const size_t rowStride = static_cast<size_t>(width) * static_cast<size_t>(channels);

    result.width = width;
    result.height = height;
    result.channels = channels;
    result.alphaChannel = -1;
    result.componentType = ImageComponentType::U8;
    result.bytes.resize(static_cast<size_t>(height) * rowStride);

    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW rowPointer =
            reinterpret_cast<JSAMPROW>(result.bytes.data() + static_cast<size_t>(cinfo.output_scanline) * rowStride);
        jpeg_read_scanlines(&cinfo, &rowPointer, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    close_file(file);

    result.success = true;
    return result;
#endif
}

bool
encode_jpg_file(const std::string& path,
                const std::map<std::string, std::string>& attributes,
                int alphaChannel,
                const HostImageData& image,
                std::string& errorMessage)
{
#if !defined(RAWGL_HAS_LIBJPEG)
    errorMessage = "libjpeg support is not available";
    return false;
#else
    int outputChannels = 0;
    std::vector<uint8_t> encodedBytes;
    if (!convert_host_image_to_jpeg_bytes(image, alphaChannel, outputChannels, encodedBytes, errorMessage)) {
        return false;
    }

    FILE* file = fopen(path.c_str(), "wb");
    if (file == nullptr) {
        errorMessage = "can't open JPEG file for writing";
        return false;
    }

    jpeg_compress_struct cinfo {};
    JpegErrorState errorState {};
    initialize_jpeg_error_state(errorState);
    cinfo.err = &errorState.base;

    if (setjmp(errorState.jumpBuffer) != 0) {
        jpeg_destroy_compress(&cinfo);
        close_file(file);
        errorMessage = errorState.message[0] != '\0' ? errorState.message : "libjpeg write failed";
        return false;
    }

    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, file);

    cinfo.image_width = static_cast<JDIMENSION>(image.width);
    cinfo.image_height = static_cast<JDIMENSION>(image.height);
    cinfo.input_components = outputChannels;
    cinfo.in_color_space = outputChannels == 1 ? JCS_GRAYSCALE : JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, parse_jpeg_quality(attributes), TRUE);
    if (parse_jpeg_progressive(attributes)) {
        jpeg_simple_progression(&cinfo);
    }

    jpeg_start_compress(&cinfo, TRUE);

    const size_t rowStride = static_cast<size_t>(image.width) * static_cast<size_t>(outputChannels);
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW rowPointer =
            reinterpret_cast<JSAMPROW>(encodedBytes.data() + static_cast<size_t>(cinfo.next_scanline) * rowStride);
        jpeg_write_scanlines(&cinfo, &rowPointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    close_file(file);
    return true;
#endif
}

}  // namespace rawgl::io
