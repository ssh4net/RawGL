// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "png_backend.h"

#include "gl_utils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#if defined(RAWGL_HAS_LIBPNG)
#include <png.h>
#endif

namespace rawgl::io {
namespace {

static bool
host_is_little_endian()
{
    const uint16_t value = 1u;
    return *reinterpret_cast<const uint8_t*>(&value) == 1u;
}

static int
parse_png_compression_level(const std::map<std::string, std::string>& attributes)
{
    const auto it = attributes.find("png:compressionLevel");
    if (it == attributes.end()) {
        return -1;
    }

    const int value = std::atoi(it->second.c_str());
    return std::clamp(value, 0, 9);
}

static bool
parse_png_interlaced(const std::map<std::string, std::string>& attributes)
{
    auto it = attributes.find("png:interlaced");
    if (it == attributes.end()) {
        it = attributes.find("png:interlace");
    }
    if (it == attributes.end()) {
        return false;
    }

    const std::string& value = it->second;
    return value == "1" || value == "true" || value == "True" || value == "TRUE";
}

static int
resolve_png_color_type(const int channels)
{
    switch (channels) {
    case 1: return PNG_COLOR_TYPE_GRAY;
    case 2: return PNG_COLOR_TYPE_GRAY_ALPHA;
    case 3: return PNG_COLOR_TYPE_RGB;
    case 4: return PNG_COLOR_TYPE_RGBA;
    default: break;
    }

    return -1;
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

static uint16_t
quantize_unit_float_to_u16(float value)
{
    if (!std::isfinite(value)) {
        value = 0.0f;
    }
    value = std::clamp(value, 0.0f, 1.0f);
    return static_cast<uint16_t>(value * 65535.0f + 0.5f);
}

static bool
convert_host_image_to_png_bytes(const HostImageData& image,
                                const ImageComponentType componentType,
                                std::vector<std::byte>& bytes,
                                std::string& errorMessage)
{
    if (image.width <= 0 || image.height <= 0 || image.channels < 1 || image.channels > 4) {
        errorMessage = "invalid host image dimensions or channel count for PNG";
        return false;
    }

    const size_t pixelCount = static_cast<size_t>(image.width) * static_cast<size_t>(image.height);
    const size_t sampleCount = pixelCount * static_cast<size_t>(image.channels);

    if (componentType == ImageComponentType::U8) {
        bytes.resize(sampleCount);
        if (image.glType == GL_UNSIGNED_BYTE) {
            std::memcpy(bytes.data(), image.bytes.data(), sampleCount);
            return true;
        }

        uint8_t* destination = reinterpret_cast<uint8_t*>(bytes.data());
        for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
            bool supported = false;
            const float value = read_source_sample(image, sampleIndex, supported);
            if (!supported) {
                errorMessage = "unsupported host image type for native PNG write";
                return false;
            }
            destination[sampleIndex] = quantize_unit_float_to_u8(value);
        }
        return true;
    }

    if (componentType == ImageComponentType::U16) {
        bytes.resize(sampleCount * sizeof(uint16_t));
        uint8_t* destination = reinterpret_cast<uint8_t*>(bytes.data());

        if (image.glType == GL_UNSIGNED_SHORT) {
            if (host_is_little_endian()) {
                for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
                    uint16_t value = 0u;
                    std::memcpy(&value, image.bytes.data() + sampleIndex * sizeof(uint16_t), sizeof(value));
                    destination[sampleIndex * 2u] = static_cast<uint8_t>((value >> 8u) & 0xffu);
                    destination[sampleIndex * 2u + 1u] = static_cast<uint8_t>(value & 0xffu);
                }
            } else {
                std::memcpy(bytes.data(), image.bytes.data(), sampleCount * sizeof(uint16_t));
            }
        } else {
            for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex) {
                bool supported = false;
                const float value = read_source_sample(image, sampleIndex, supported);
                if (!supported) {
                    errorMessage = "unsupported host image type for native PNG write";
                    return false;
                }
                const uint16_t quantized = quantize_unit_float_to_u16(value);
                destination[sampleIndex * 2u] = static_cast<uint8_t>((quantized >> 8u) & 0xffu);
                destination[sampleIndex * 2u + 1u] = static_cast<uint8_t>(quantized & 0xffu);
            }
        }
        return true;
    }

    errorMessage = "native PNG write currently supports only 8-bit and 16-bit output";
    return false;
}

#if defined(RAWGL_HAS_LIBPNG)
static void
close_file(FILE* file)
{
    if (file != nullptr) {
        fclose(file);
    }
}
#endif

}  // namespace

DecodedImageData
decode_png_file(const std::string& path)
{
    DecodedImageData result;

#if !defined(RAWGL_HAS_LIBPNG)
    result.errorMessage = "libpng support is not available";
    return result;
#else
    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        result.errorMessage = "can't open PNG file";
        return result;
    }

    std::array<png_byte, 8> signature {};
    const size_t signatureBytes = fread(signature.data(), 1u, signature.size(), file);
    if (signatureBytes != signature.size() || png_sig_cmp(signature.data(), 0u, signature.size()) != 0) {
        close_file(file);
        result.errorMessage = "file is not a valid PNG image";
        return result;
    }

    png_structp pngPtr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (pngPtr == nullptr) {
        close_file(file);
        result.errorMessage = "failed to create PNG read struct";
        return result;
    }

    png_infop infoPtr = png_create_info_struct(pngPtr);
    if (infoPtr == nullptr) {
        png_destroy_read_struct(&pngPtr, nullptr, nullptr);
        close_file(file);
        result.errorMessage = "failed to create PNG info struct";
        return result;
    }

    if (setjmp(png_jmpbuf(pngPtr)) != 0) {
        png_destroy_read_struct(&pngPtr, &infoPtr, nullptr);
        close_file(file);
        if (result.errorMessage.empty()) {
            result.errorMessage = "libpng read failed";
        }
        return result;
    }

    png_init_io(pngPtr, file);
    png_set_sig_bytes(pngPtr, static_cast<int>(signature.size()));
    png_read_info(pngPtr, infoPtr);

    png_uint_32 width = 0u;
    png_uint_32 height = 0u;
    int bitDepth = 0;
    int colorType = 0;
    int interlaceType = 0;
    png_get_IHDR(pngPtr, infoPtr, &width, &height, &bitDepth, &colorType, &interlaceType, nullptr, nullptr);

    if (colorType == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(pngPtr);
    }
    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) {
        png_set_expand_gray_1_2_4_to_8(pngPtr);
    }
    if (png_get_valid(pngPtr, infoPtr, PNG_INFO_tRNS) != 0u) {
        png_set_tRNS_to_alpha(pngPtr);
    }
    if (interlaceType != PNG_INTERLACE_NONE) {
        png_set_interlace_handling(pngPtr);
    }

    png_read_update_info(pngPtr, infoPtr);

    bitDepth = png_get_bit_depth(pngPtr, infoPtr);
    if (bitDepth != 8 && bitDepth != 16) {
        result.errorMessage = "unsupported PNG bit depth";
        png_destroy_read_struct(&pngPtr, &infoPtr, nullptr);
        close_file(file);
        return result;
    }

    if (bitDepth == 16 && host_is_little_endian()) {
        png_set_swap(pngPtr);
    }

    const int channels = png_get_channels(pngPtr, infoPtr);
    const png_size_t rowBytes = png_get_rowbytes(pngPtr, infoPtr);
    result.bytes.resize(static_cast<size_t>(rowBytes) * static_cast<size_t>(height));

    std::vector<png_bytep> rows(static_cast<size_t>(height));
    for (size_t rowIndex = 0; rowIndex < static_cast<size_t>(height); ++rowIndex) {
        rows[rowIndex] = reinterpret_cast<png_bytep>(result.bytes.data()) + rowIndex * static_cast<size_t>(rowBytes);
    }

    png_read_image(pngPtr, rows.data());
    png_read_end(pngPtr, infoPtr);

    png_destroy_read_struct(&pngPtr, &infoPtr, nullptr);
    close_file(file);

    result.width = static_cast<int>(width);
    result.height = static_cast<int>(height);
    result.channels = channels;
    result.alphaChannel = (channels == 2 || channels == 4) ? (channels - 1) : -1;
    result.componentType = (bitDepth == 16) ? ImageComponentType::U16 : ImageComponentType::U8;
    result.success = true;
    return result;
#endif
}

bool
encode_png_file(const std::string& path,
                const std::map<std::string, std::string>& attributes,
                int alphaChannel,
                const HostImageData& image,
                const ImageEncodeSettings& settings,
                std::string& errorMessage)
{
    (void)alphaChannel;

#if !defined(RAWGL_HAS_LIBPNG)
    errorMessage = "libpng support is not available";
    return false;
#else
    if (settings.componentType != ImageComponentType::U8 && settings.componentType != ImageComponentType::U16) {
        errorMessage = "native PNG write only supports 8-bit or 16-bit PNG output";
        return false;
    }

    const int colorType = resolve_png_color_type(image.channels);
    if (colorType < 0) {
        errorMessage = "unsupported PNG channel count";
        return false;
    }

    std::vector<std::byte> encodedBytes;
    if (!convert_host_image_to_png_bytes(image, settings.componentType, encodedBytes, errorMessage)) {
        return false;
    }

    FILE* file = fopen(path.c_str(), "wb");
    if (file == nullptr) {
        errorMessage = "can't open PNG file for writing";
        return false;
    }

    png_structp pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (pngPtr == nullptr) {
        close_file(file);
        errorMessage = "failed to create PNG write struct";
        return false;
    }

    png_infop infoPtr = png_create_info_struct(pngPtr);
    if (infoPtr == nullptr) {
        png_destroy_write_struct(&pngPtr, nullptr);
        close_file(file);
        errorMessage = "failed to create PNG info struct";
        return false;
    }

    if (setjmp(png_jmpbuf(pngPtr)) != 0) {
        png_destroy_write_struct(&pngPtr, &infoPtr);
        close_file(file);
        if (errorMessage.empty()) {
            errorMessage = "libpng write failed";
        }
        return false;
    }

    png_init_io(pngPtr, file);
    png_set_compression_level(pngPtr, parse_png_compression_level(attributes));

    const int interlaceType = parse_png_interlaced(attributes) ? PNG_INTERLACE_ADAM7 : PNG_INTERLACE_NONE;
    const int bitDepth = settings.componentType == ImageComponentType::U16 ? 16 : 8;
    png_set_IHDR(pngPtr,
                 infoPtr,
                 static_cast<png_uint_32>(image.width),
                 static_cast<png_uint_32>(image.height),
                 bitDepth,
                 colorType,
                 interlaceType,
                 PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(pngPtr, infoPtr);

    const png_size_t rowBytes = png_get_rowbytes(pngPtr, infoPtr);
    const size_t expectedRowBytes = static_cast<size_t>(image.width) * static_cast<size_t>(image.channels)
                                    * (bitDepth == 16 ? sizeof(uint16_t) : sizeof(uint8_t));
    if (static_cast<size_t>(rowBytes) != expectedRowBytes) {
        png_destroy_write_struct(&pngPtr, &infoPtr);
        close_file(file);
        errorMessage = "unexpected PNG row byte size";
        return false;
    }

    std::vector<png_bytep> rows(static_cast<size_t>(image.height));
    for (size_t rowIndex = 0; rowIndex < static_cast<size_t>(image.height); ++rowIndex) {
        rows[rowIndex] = reinterpret_cast<png_bytep>(encodedBytes.data()) + rowIndex * static_cast<size_t>(rowBytes);
    }

    png_write_image(pngPtr, rows.data());
    png_write_end(pngPtr, infoPtr);
    png_destroy_write_struct(&pngPtr, &infoPtr);
    close_file(file);
    return true;
#endif
}

}  // namespace rawgl::io
