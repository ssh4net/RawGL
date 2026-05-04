// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "jpg_backend.h"

#include "gl_utils.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <csetjmp>
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

enum class JpegSubsampling
{
    Default,
    S444,
    S422,
    S420,
    S440,
    S411,
};

enum class JpegLoadColorMode
{
    Auto,
    Grayscale,
    Rgb,
};

struct JpegSaveOptions {
    int quality = 95;
    bool progressive = false;
    bool optimizeCoding = false;
    bool hasOptimizeCoding = false;
    JpegSubsampling subsampling = JpegSubsampling::Default;
    bool hasSubsampling = false;
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

static bool
parse_unsigned_decimal_setting(const std::string& text,
                               const unsigned long minimumValue,
                               const unsigned long maximumValue,
                               unsigned long& value,
                               const char* errorText,
                               std::string& errorMessage)
{
    if (text.empty()) {
        errorMessage = errorText;
        return false;
    }
    for (const char digit : text) {
        if (!std::isdigit(static_cast<unsigned char>(digit))) {
            errorMessage = errorText;
            return false;
        }
    }

    errno = 0;
    char* end = nullptr;
    const unsigned long parsedValue = std::strtoul(text.c_str(), &end, 10);
    if (errno != 0 || end == nullptr || *end != '\0' || parsedValue < minimumValue || parsedValue > maximumValue) {
        errorMessage = errorText;
        return false;
    }

    value = parsedValue;
    return true;
}

static bool
parse_jpeg_quality_option(const std::map<std::string, std::string>& attributes,
                          JpegSaveOptions& options,
                          std::string& errorMessage)
{
    const std::string* value = nullptr;
    const auto explicitJpeg = attributes.find("jpeg:quality");
    if (explicitJpeg != attributes.end()) {
        value = &explicitJpeg->second;
    } else {
        const auto explicitJpg = attributes.find("jpg:quality");
        if (explicitJpg != attributes.end()) {
            value = &explicitJpg->second;
        }
    }

    std::string legacyQuality;
    if (value == nullptr) {
        const auto legacyCompression = attributes.find("oiio:Compression");
        if (legacyCompression != attributes.end()) {
            const std::string& legacyValue = legacyCompression->second;
            const std::string jpegPrefix = "jpeg:";
            const std::string jpgPrefix = "jpg:";
            if (legacyValue.rfind(jpegPrefix, 0) == 0) {
                legacyQuality = legacyValue.substr(jpegPrefix.size());
                value = &legacyQuality;
            } else if (legacyValue.rfind(jpgPrefix, 0) == 0) {
                legacyQuality = legacyValue.substr(jpgPrefix.size());
                value = &legacyQuality;
            } else {
                errorMessage = "unsupported JPEG legacy compression value";
                return false;
            }
        }
    }

    if (value == nullptr) {
        return true;
    }

    unsigned long parsedValue = 0ul;
    if (!parse_unsigned_decimal_setting(*value, 1ul, 100ul, parsedValue, "invalid JPEG quality value", errorMessage)) {
        return false;
    }

    options.quality = static_cast<int>(parsedValue);
    return true;
}

static bool
parse_jpeg_bool_setting(const std::map<std::string, std::string>& attributes,
                        const char* jpegKey,
                        const char* jpgKey,
                        bool& value,
                        bool& present,
                        const char* errorText,
                        std::string& errorMessage)
{
    present = false;
    value = false;

    const std::string* attribute = nullptr;
    const auto jpegValue = attributes.find(jpegKey);
    if (jpegValue != attributes.end()) {
        attribute = &jpegValue->second;
    } else {
        const auto jpgValue = attributes.find(jpgKey);
        if (jpgValue != attributes.end()) {
            attribute = &jpgValue->second;
        }
    }
    if (attribute == nullptr) {
        return true;
    }

    present = true;
    const std::string normalized = to_lower_copy(*attribute);
    if (normalized == "1" || normalized == "true" || normalized == "on" || normalized == "yes") {
        value = true;
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "off" || normalized == "no") {
        value = false;
        return true;
    }

    errorMessage = errorText;
    return false;
}

static bool
parse_jpeg_subsampling(const std::map<std::string, std::string>& attributes,
                       JpegSaveOptions& options,
                       std::string& errorMessage)
{
    options.hasSubsampling = false;
    options.subsampling = JpegSubsampling::Default;

    const std::string* attribute = nullptr;
    const auto jpegValue = attributes.find("jpeg:subsampling");
    if (jpegValue != attributes.end()) {
        attribute = &jpegValue->second;
    } else {
        const auto jpgValue = attributes.find("jpg:subsampling");
        if (jpgValue != attributes.end()) {
            attribute = &jpgValue->second;
        }
    }
    if (attribute == nullptr) {
        return true;
    }

    options.hasSubsampling = true;
    const std::string normalized = to_lower_copy(*attribute);
    if (normalized == "default" || normalized == "auto") {
        options.subsampling = JpegSubsampling::Default;
    } else if (normalized == "444" || normalized == "4:4:4" || normalized == "none") {
        options.subsampling = JpegSubsampling::S444;
    } else if (normalized == "422" || normalized == "4:2:2") {
        options.subsampling = JpegSubsampling::S422;
    } else if (normalized == "420" || normalized == "4:2:0") {
        options.subsampling = JpegSubsampling::S420;
    } else if (normalized == "440" || normalized == "4:4:0") {
        options.subsampling = JpegSubsampling::S440;
    } else if (normalized == "411" || normalized == "4:1:1") {
        options.subsampling = JpegSubsampling::S411;
    } else {
        errorMessage = "unsupported JPEG chroma subsampling mode";
        return false;
    }

    return true;
}

static bool
parse_jpeg_save_options(const std::map<std::string, std::string>& attributes,
                        JpegSaveOptions& options,
                        std::string& errorMessage)
{
    options = JpegSaveOptions();

    if (!parse_jpeg_quality_option(attributes, options, errorMessage)) {
        return false;
    }
    bool hasProgressive = false;
    if (!parse_jpeg_bool_setting(attributes,
                                 "jpeg:progressive",
                                 "jpg:progressive",
                                 options.progressive,
                                 hasProgressive,
                                 "invalid JPEG progressive value",
                                 errorMessage)) {
        return false;
    }
    (void)hasProgressive;
    if (!parse_jpeg_bool_setting(attributes,
                                 "jpeg:optimize",
                                 "jpg:optimize",
                                 options.optimizeCoding,
                                 options.hasOptimizeCoding,
                                 "invalid JPEG optimize value",
                                 errorMessage)) {
        return false;
    }
    if (!parse_jpeg_subsampling(attributes, options, errorMessage)) {
        return false;
    }

    return true;
}

static bool
apply_jpeg_subsampling(jpeg_compress_struct& cinfo,
                       const int outputChannels,
                       const JpegSaveOptions& options,
                       std::string& errorMessage)
{
    if (!options.hasSubsampling || options.subsampling == JpegSubsampling::Default) {
        return true;
    }
    if (outputChannels != 3 || cinfo.num_components < 3) {
        errorMessage = "JPEG chroma subsampling requires RGB output";
        return false;
    }

    int yHorizontal = 1;
    int yVertical = 1;
    switch (options.subsampling) {
    case JpegSubsampling::S444:
        yHorizontal = 1;
        yVertical = 1;
        break;
    case JpegSubsampling::S422:
        yHorizontal = 2;
        yVertical = 1;
        break;
    case JpegSubsampling::S420:
        yHorizontal = 2;
        yVertical = 2;
        break;
    case JpegSubsampling::S440:
        yHorizontal = 1;
        yVertical = 2;
        break;
    case JpegSubsampling::S411:
        yHorizontal = 4;
        yVertical = 1;
        break;
    case JpegSubsampling::Default:
        return true;
    }

    cinfo.comp_info[0].h_samp_factor = yHorizontal;
    cinfo.comp_info[0].v_samp_factor = yVertical;
    cinfo.comp_info[1].h_samp_factor = 1;
    cinfo.comp_info[1].v_samp_factor = 1;
    cinfo.comp_info[2].h_samp_factor = 1;
    cinfo.comp_info[2].v_samp_factor = 1;
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
parse_jpeg_load_color_mode(const std::map<std::string, std::string>& attributes,
                           JpegLoadColorMode& colorMode,
                           std::string& errorMessage)
{
    colorMode = JpegLoadColorMode::Auto;
    const auto transformValue = attributes.find("jpeg:color_transform");
    if (transformValue == attributes.end()) {
        return true;
    }

    const std::string normalized = to_lower_copy(transformValue->second);
    if (normalized == "auto") {
        colorMode = JpegLoadColorMode::Auto;
        return true;
    }
    if (normalized == "grayscale" || normalized == "gray") {
        colorMode = JpegLoadColorMode::Grayscale;
        return true;
    }
    if (normalized == "rgb") {
        colorMode = JpegLoadColorMode::Rgb;
        return true;
    }

    errorMessage = "unsupported JPEG load color transform";
    return false;
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
decode_jpg_file(const std::string& path, const std::map<std::string, std::string>& attributes)
{
    DecodedImageData result;

#if !defined(RAWGL_HAS_LIBJPEG)
    (void)path;
    (void)attributes;
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

    JpegLoadColorMode colorMode = JpegLoadColorMode::Auto;
    if (!parse_jpeg_load_color_mode(attributes, colorMode, result.errorMessage)) {
        jpeg_destroy_decompress(&cinfo);
        close_file(file);
        return result;
    }

    if (colorMode == JpegLoadColorMode::Grayscale) {
        cinfo.out_color_space = JCS_GRAYSCALE;
    } else if (colorMode == JpegLoadColorMode::Rgb) {
        cinfo.out_color_space = JCS_RGB;
    } else if (cinfo.jpeg_color_space == JCS_GRAYSCALE) {
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

    JpegSaveOptions options;
    if (!parse_jpeg_save_options(attributes, options, errorMessage)) {
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

    jpeg_set_quality(&cinfo, options.quality, TRUE);

    if (options.progressive) {
        jpeg_simple_progression(&cinfo);
    }

    if (options.hasOptimizeCoding) {
        cinfo.optimize_coding = options.optimizeCoding ? TRUE : FALSE;
    }

    if (!apply_jpeg_subsampling(cinfo, outputChannels, options, errorMessage)) {
        jpeg_destroy_compress(&cinfo);
        close_file(file);
        return false;
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
