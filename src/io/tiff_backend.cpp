// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "tiff_backend.h"

#include "gl_utils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(RAWGL_HAS_LIBTIFF)
#include <tiffio.h>
#endif

namespace rawgl::io {
namespace {

#if defined(RAWGL_HAS_LIBTIFF)
static char
to_lower_ascii(const unsigned char c)
{
    return static_cast<char>(std::tolower(c));
}

static std::string
to_lower_copy(const std::string& value)
{
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(), to_lower_ascii);
    return result;
}

static const std::string*
find_attribute_value(const std::map<std::string, std::string>& attributes, const char* key)
{
    const auto it = attributes.find(key);
    if (it == attributes.end()) {
        return nullptr;
    }
    return &it->second;
}

static uint16_t
default_tiff_compression()
{
    return COMPRESSION_NONE;
}

static bool
parse_tiff_compression(const std::map<std::string, std::string>& attributes,
                       uint16_t& compression,
                       std::string& errorMessage)
{
    const std::string* value = find_attribute_value(attributes, "tiff:compression");
    if (!value) {
        value = find_attribute_value(attributes, "compression");
    }
    if (!value) {
        value = find_attribute_value(attributes, "oiio:Compression");
    }
    if (!value) {
        compression = default_tiff_compression();
        return true;
    }

    const std::string normalized = to_lower_copy(*value);
    if (normalized == "none") {
        compression = COMPRESSION_NONE;
    } else if (normalized == "lzw") {
        compression = COMPRESSION_LZW;
    } else if (normalized == "packbits") {
        compression = COMPRESSION_PACKBITS;
    } else if (normalized == "zip" || normalized == "deflate" || normalized == "adobe_deflate") {
#if defined(COMPRESSION_ADOBE_DEFLATE)
        compression = COMPRESSION_ADOBE_DEFLATE;
#else
        compression = COMPRESSION_DEFLATE;
#endif
    } else {
        errorMessage = "unsupported TIFF compression mode";
        return false;
    }

    if (!TIFFIsCODECConfigured(compression)) {
        errorMessage = "requested TIFF compression codec is not available";
        return false;
    }

    return true;
}

static bool
has_unassociated_alpha_hint(const std::map<std::string, std::string>& attributes)
{
    const std::string* value = find_attribute_value(attributes, "oiio:UnassociatedAlpha");
    if (!value) {
        return false;
    }

    const std::string normalized = to_lower_copy(*value);
    if (normalized.empty() || normalized == "0" || normalized == "false" || normalized == "off") {
        return false;
    }
    return true;
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

static float
read_source_sample_as_unit_float(const HostImageData& image, const size_t sampleIndex, bool& supported)
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

static bool
resolve_tiff_channel_layout(const HostImageData& image,
                            const int explicitAlphaChannel,
                            int& outputChannels,
                            int& resolvedAlphaChannel,
                            std::array<int, 4>& sourceChannelMap,
                            std::string& errorMessage)
{
    if (image.width <= 0 || image.height <= 0 || image.channels < 1 || image.channels > 4) {
        errorMessage = "invalid host image dimensions or channel count for TIFF";
        return false;
    }

    outputChannels = image.channels;
    resolvedAlphaChannel = explicitAlphaChannel >= 0 ? explicitAlphaChannel : image.alphaChannel;

    if ((image.channels == 2 || image.channels == 4) && resolvedAlphaChannel < 0) {
        resolvedAlphaChannel = image.channels - 1;
    }

    if (resolvedAlphaChannel >= image.channels) {
        errorMessage = "invalid TIFF alpha channel index";
        return false;
    }

    int destinationIndex = 0;
    if (resolvedAlphaChannel >= 0) {
        for (int sourceChannel = 0; sourceChannel < image.channels; ++sourceChannel) {
            if (sourceChannel == resolvedAlphaChannel) {
                continue;
            }
            sourceChannelMap[destinationIndex++] = sourceChannel;
        }
        sourceChannelMap[destinationIndex++] = resolvedAlphaChannel;
    } else {
        for (int sourceChannel = 0; sourceChannel < image.channels; ++sourceChannel) {
            sourceChannelMap[destinationIndex++] = sourceChannel;
        }
    }

    if (destinationIndex != outputChannels) {
        errorMessage = "unsupported TIFF channel layout";
        return false;
    }

    const int colorChannels = resolvedAlphaChannel >= 0 ? (outputChannels - 1) : outputChannels;
    if (colorChannels != 1 && colorChannels != 3) {
        errorMessage = "native TIFF write only supports grayscale or RGB channel layouts";
        return false;
    }

    return true;
}

static bool
convert_host_image_to_tiff_bytes(const HostImageData& image,
                                 const ImageEncodeSettings& settings,
                                 const int explicitAlphaChannel,
                                 int& outputChannels,
                                 int& resolvedAlphaChannel,
                                 std::array<int, 4>& sourceChannelMap,
                                 std::vector<std::byte>& bytes,
                                 std::string& errorMessage)
{
    if (!resolve_tiff_channel_layout(
            image, explicitAlphaChannel, outputChannels, resolvedAlphaChannel, sourceChannelMap, errorMessage)) {
        return false;
    }

    const size_t pixelCount = static_cast<size_t>(image.width) * static_cast<size_t>(image.height);
    const size_t sampleCount = pixelCount * static_cast<size_t>(outputChannels);

    if (settings.componentType == ImageComponentType::U8) {
        bytes.resize(sampleCount);
        uint8_t* destination = reinterpret_cast<uint8_t*>(bytes.data());
        for (size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
            for (int destinationChannel = 0; destinationChannel < outputChannels; ++destinationChannel) {
                const size_t sampleIndex = pixelIndex * static_cast<size_t>(image.channels)
                                           + static_cast<size_t>(sourceChannelMap[destinationChannel]);
                bool supported = false;
                const float value = read_source_sample_as_unit_float(image, sampleIndex, supported);
                if (!supported) {
                    errorMessage = "unsupported host image type for native TIFF write";
                    return false;
                }
                destination[pixelIndex * static_cast<size_t>(outputChannels) + static_cast<size_t>(destinationChannel)]
                    = quantize_unit_float_to_u8(value);
            }
        }
        return true;
    }

    if (settings.componentType == ImageComponentType::U16) {
        bytes.resize(sampleCount * sizeof(uint16_t));
        uint16_t* destination = reinterpret_cast<uint16_t*>(bytes.data());
        for (size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
            for (int destinationChannel = 0; destinationChannel < outputChannels; ++destinationChannel) {
                const size_t sampleIndex = pixelIndex * static_cast<size_t>(image.channels)
                                           + static_cast<size_t>(sourceChannelMap[destinationChannel]);
                bool supported = false;
                const float value = read_source_sample_as_unit_float(image, sampleIndex, supported);
                if (!supported) {
                    errorMessage = "unsupported host image type for native TIFF write";
                    return false;
                }
                destination[pixelIndex * static_cast<size_t>(outputChannels) + static_cast<size_t>(destinationChannel)]
                    = quantize_unit_float_to_u16(value);
            }
        }
        return true;
    }

    if (settings.componentType == ImageComponentType::F32) {
        bytes.resize(sampleCount * sizeof(float));
        float* destination = reinterpret_cast<float*>(bytes.data());
        for (size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
            for (int destinationChannel = 0; destinationChannel < outputChannels; ++destinationChannel) {
                const size_t sampleIndex = pixelIndex * static_cast<size_t>(image.channels)
                                           + static_cast<size_t>(sourceChannelMap[destinationChannel]);
                bool supported = false;
                const float value = read_source_sample_as_unit_float(image, sampleIndex, supported);
                if (!supported) {
                    errorMessage = "unsupported host image type for native TIFF write";
                    return false;
                }
                destination[pixelIndex * static_cast<size_t>(outputChannels) + static_cast<size_t>(destinationChannel)]
                    = value;
            }
        }
        return true;
    }

    errorMessage = "native TIFF write currently supports only 8-bit, 16-bit, and 32-bit float output";
    return false;
}

static bool
resolve_tiff_decode_layout(const uint16_t samplesPerPixel,
                           const uint16_t photometric,
                           const uint16_t planarConfig,
                           const bool isTiled,
                           int& alphaChannel,
                           std::string& errorMessage)
{
    if (samplesPerPixel < 1u || samplesPerPixel > 4u) {
        errorMessage = "unsupported TIFF sample count";
        return false;
    }
    if (planarConfig != PLANARCONFIG_CONTIG) {
        errorMessage = "native TIFF decode requires contiguous planar layout";
        return false;
    }
    if (isTiled) {
        errorMessage = "native TIFF decode does not support tiled images yet";
        return false;
    }

    if ((samplesPerPixel == 1u || samplesPerPixel == 2u)
        && photometric != PHOTOMETRIC_MINISBLACK
        && photometric != PHOTOMETRIC_MINISWHITE) {
        errorMessage = "native TIFF decode supports grayscale images only for 1-2 channels";
        return false;
    }
    if ((samplesPerPixel == 3u || samplesPerPixel == 4u) && photometric != PHOTOMETRIC_RGB) {
        errorMessage = "native TIFF decode supports RGB images only for 3-4 channels";
        return false;
    }

    alphaChannel = (samplesPerPixel == 2u || samplesPerPixel == 4u) ? (static_cast<int>(samplesPerPixel) - 1) : -1;
    return true;
}

static bool
resolve_tiff_component_type(const uint16_t bitsPerSample,
                            const uint16_t sampleFormat,
                            ImageComponentType& componentType,
                            std::string& errorMessage)
{
    const uint16_t effectiveSampleFormat = sampleFormat == SAMPLEFORMAT_VOID ? SAMPLEFORMAT_UINT : sampleFormat;

    if (effectiveSampleFormat == SAMPLEFORMAT_UINT) {
        if (bitsPerSample == 8u) {
            componentType = ImageComponentType::U8;
            return true;
        }
        if (bitsPerSample == 16u) {
            componentType = ImageComponentType::U16;
            return true;
        }
        if (bitsPerSample == 32u) {
            componentType = ImageComponentType::U32;
            return true;
        }
    } else if (effectiveSampleFormat == SAMPLEFORMAT_IEEEFP) {
        if (bitsPerSample == 32u) {
            componentType = ImageComponentType::F32;
            return true;
        }
    }

    errorMessage = "unsupported TIFF bit depth or sample format";
    return false;
}

static void
copy_tiff_row_samples(const uint8_t* rowBytes,
                      const size_t rowPixelCount,
                      const uint16_t samplesPerPixel,
                      const uint16_t bitsPerSample,
                      const uint16_t photometric,
                      const ImageComponentType componentType,
                      std::byte* destinationRow)
{
    if (componentType == ImageComponentType::U8) {
        uint8_t* destination = reinterpret_cast<uint8_t*>(destinationRow);
        for (size_t sampleIndex = 0; sampleIndex < rowPixelCount * static_cast<size_t>(samplesPerPixel); ++sampleIndex) {
            uint8_t value = rowBytes[sampleIndex];
            if (photometric == PHOTOMETRIC_MINISWHITE) {
                value = static_cast<uint8_t>(255u - value);
            }
            destination[sampleIndex] = value;
        }
        return;
    }

    if (componentType == ImageComponentType::U16) {
        const uint16_t* source = reinterpret_cast<const uint16_t*>(rowBytes);
        uint16_t* destination = reinterpret_cast<uint16_t*>(destinationRow);
        for (size_t sampleIndex = 0; sampleIndex < rowPixelCount * static_cast<size_t>(samplesPerPixel); ++sampleIndex) {
            uint16_t value = source[sampleIndex];
            if (photometric == PHOTOMETRIC_MINISWHITE) {
                value = static_cast<uint16_t>(65535u - value);
            }
            destination[sampleIndex] = value;
        }
        return;
    }

    if (componentType == ImageComponentType::U32) {
        const uint32_t* source = reinterpret_cast<const uint32_t*>(rowBytes);
        uint32_t* destination = reinterpret_cast<uint32_t*>(destinationRow);
        for (size_t sampleIndex = 0; sampleIndex < rowPixelCount * static_cast<size_t>(samplesPerPixel); ++sampleIndex) {
            uint32_t value = source[sampleIndex];
            if (photometric == PHOTOMETRIC_MINISWHITE) {
                value = 0xffffffffu - value;
            }
            destination[sampleIndex] = value;
        }
        return;
    }

    if (componentType == ImageComponentType::F32 && bitsPerSample == 32u) {
        const float* source = reinterpret_cast<const float*>(rowBytes);
        float* destination = reinterpret_cast<float*>(destinationRow);
        for (size_t sampleIndex = 0; sampleIndex < rowPixelCount * static_cast<size_t>(samplesPerPixel); ++sampleIndex) {
            float value = source[sampleIndex];
            if (photometric == PHOTOMETRIC_MINISWHITE) {
                value = 1.0f - value;
            }
            destination[sampleIndex] = value;
        }
    }
}
#endif

}  // namespace

DecodedImageData
decode_tiff_file(const std::string& path)
{
    DecodedImageData result;

#if !defined(RAWGL_HAS_LIBTIFF)
    (void)path;
    result.errorMessage = "libtiff support is not available";
    return result;
#else
    TIFF* tif = TIFFOpen(path.c_str(), "r");
    if (tif == nullptr) {
        result.errorMessage = "can't open TIFF file";
        return result;
    }

    uint32_t width = 0u;
    uint32_t height = 0u;
    uint16_t samplesPerPixel = 1u;
    uint16_t bitsPerSample = 1u;
    uint16_t sampleFormat = SAMPLEFORMAT_UINT;
    uint16_t photometric = PHOTOMETRIC_MINISBLACK;
    uint16_t planarConfig = PLANARCONFIG_CONTIG;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesPerPixel);
    TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &bitsPerSample);
    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLEFORMAT, &sampleFormat);
    TIFFGetFieldDefaulted(tif, TIFFTAG_PHOTOMETRIC, &photometric);
    TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planarConfig);

    int alphaChannel = -1;
    std::string errorMessage;
    if (!resolve_tiff_decode_layout(
            samplesPerPixel, photometric, planarConfig, TIFFIsTiled(tif) != 0, alphaChannel, errorMessage)) {
        TIFFClose(tif);
        result.errorMessage = errorMessage;
        return result;
    }

    ImageComponentType componentType = ImageComponentType::Unknown;
    if (!resolve_tiff_component_type(bitsPerSample, sampleFormat, componentType, errorMessage)) {
        TIFFClose(tif);
        result.errorMessage = errorMessage;
        return result;
    }

    const size_t bytesPerComponent = byte_size_for_image_component(componentType);
    if (bytesPerComponent == 0u) {
        TIFFClose(tif);
        result.errorMessage = "unsupported TIFF component type";
        return result;
    }

    const tsize_t scanlineSize = TIFFScanlineSize(tif);
    if (scanlineSize <= 0) {
        TIFFClose(tif);
        result.errorMessage = "invalid TIFF scanline size";
        return result;
    }

    result.width = static_cast<int>(width);
    result.height = static_cast<int>(height);
    result.channels = static_cast<int>(samplesPerPixel);
    result.alphaChannel = alphaChannel;
    result.componentType = componentType;
    result.bytes.resize(static_cast<size_t>(width) * static_cast<size_t>(height)
                        * static_cast<size_t>(samplesPerPixel) * bytesPerComponent);

    std::vector<uint8_t> rowBuffer(static_cast<size_t>(scanlineSize));
    const size_t destinationRowBytes = static_cast<size_t>(width) * static_cast<size_t>(samplesPerPixel)
                                       * bytesPerComponent;

    for (uint32_t row = 0u; row < height; ++row) {
        if (TIFFReadScanline(tif, rowBuffer.data(), row, 0) != 1) {
            TIFFClose(tif);
            result = DecodedImageData();
            result.errorMessage = "can't read TIFF scanline";
            return result;
        }

        std::byte* destinationRow = result.bytes.data() + static_cast<size_t>(row) * destinationRowBytes;
        copy_tiff_row_samples(
            rowBuffer.data(), static_cast<size_t>(width), samplesPerPixel, bitsPerSample, photometric, componentType, destinationRow);
    }

    TIFFClose(tif);
    result.success = true;
    return result;
#endif
}

bool
encode_tiff_file(const std::string& path,
                 const std::map<std::string, std::string>& attributes,
                 int alphaChannel,
                 const HostImageData& image,
                 const ImageEncodeSettings& settings,
                 std::string& errorMessage)
{
#if !defined(RAWGL_HAS_LIBTIFF)
    (void)path;
    (void)attributes;
    (void)alphaChannel;
    (void)image;
    (void)settings;
    errorMessage = "libtiff support is not available";
    return false;
#else
    uint16_t compression = default_tiff_compression();
    if (!parse_tiff_compression(attributes, compression, errorMessage)) {
        return false;
    }

    int outputChannels = 0;
    int resolvedAlphaChannel = -1;
    std::array<int, 4> sourceChannelMap = { 0, 1, 2, 3 };
    std::vector<std::byte> encodedBytes;
    if (!convert_host_image_to_tiff_bytes(image,
                                          settings,
                                          alphaChannel,
                                          outputChannels,
                                          resolvedAlphaChannel,
                                          sourceChannelMap,
                                          encodedBytes,
                                          errorMessage)) {
        return false;
    }

    TIFF* tif = TIFFOpen(path.c_str(), "w");
    if (tif == nullptr) {
        errorMessage = "can't open TIFF file for writing";
        return false;
    }

    const int colorChannels = resolvedAlphaChannel >= 0 ? (outputChannels - 1) : outputChannels;
    const uint16_t photometric = colorChannels == 1 ? PHOTOMETRIC_MINISBLACK : PHOTOMETRIC_RGB;
    const uint16_t samplesPerPixel = static_cast<uint16_t>(outputChannels);

    uint16_t bitsPerSample = 0u;
    uint16_t sampleFormat = SAMPLEFORMAT_UINT;
    switch (settings.componentType) {
    case ImageComponentType::U8:
        bitsPerSample = 8u;
        sampleFormat = SAMPLEFORMAT_UINT;
        break;
    case ImageComponentType::U16:
        bitsPerSample = 16u;
        sampleFormat = SAMPLEFORMAT_UINT;
        break;
    case ImageComponentType::F32:
        bitsPerSample = 32u;
        sampleFormat = SAMPLEFORMAT_IEEEFP;
        break;
    default:
        TIFFClose(tif);
        errorMessage = "unsupported TIFF output component type";
        return false;
    }

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, static_cast<uint32_t>(image.width));
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, static_cast<uint32_t>(image.height));
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, samplesPerPixel);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bitsPerSample);
    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, sampleFormat);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, photometric);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, compression);

    if (resolvedAlphaChannel >= 0) {
        const uint16_t extraSample = has_unassociated_alpha_hint(attributes) ? EXTRASAMPLE_UNASSALPHA
                                                                              : EXTRASAMPLE_ASSOCALPHA;
        TIFFSetField(tif, TIFFTAG_EXTRASAMPLES, 1, &extraSample);
    }

    const uint16_t rowsPerStrip = TIFFDefaultStripSize(tif, 0u);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, rowsPerStrip);

    const size_t rowBytes = static_cast<size_t>(image.width) * static_cast<size_t>(outputChannels)
                            * byte_size_for_image_component(settings.componentType);
    for (int row = 0; row < image.height; ++row) {
        std::byte* rowData = encodedBytes.data() + static_cast<size_t>(row) * rowBytes;
        if (TIFFWriteScanline(tif, rowData, static_cast<uint32_t>(row), 0) != 1) {
            TIFFClose(tif);
            errorMessage = "can't write TIFF scanline";
            return false;
        }
    }

    TIFFClose(tif);
    return true;
#endif
}

}  // namespace rawgl::io
