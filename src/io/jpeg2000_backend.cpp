// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "jpeg2000_backend.h"

#include "gl_utils.h"
#include "path_utils.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#if defined(RAWGL_HAS_OPENJPEG)
#    include <openjpeg.h>
#endif

namespace rawgl::io {
namespace {

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
parse_bool_text(const std::string& text, bool& value)
{
    const std::string normalized = to_lower_copy(text);
    if (normalized == "true" || normalized == "1" || normalized == "yes" || normalized == "on") {
        value = true;
        return true;
    }
    if (normalized == "false" || normalized == "0" || normalized == "no" || normalized == "off") {
        value = false;
        return true;
    }
    return false;
}

static bool
parse_u32_text(const std::string& text, uint32_t& value)
{
    errno = 0;
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0'
        || parsed > static_cast<unsigned long>(std::numeric_limits<uint32_t>::max())) {
        return false;
    }
    value = static_cast<uint32_t>(parsed);
    return true;
}

static bool
parse_positive_float_text(const std::string& text, float& value)
{
    errno = 0;
    char* end = nullptr;
    const float parsed = std::strtof(text.c_str(), &end);
    if (errno != 0 || end == text.c_str() || *end != '\0' || !std::isfinite(parsed) || parsed <= 0.0f) {
        return false;
    }
    value = parsed;
    return true;
}

static const std::string*
find_attribute_value(const std::map<std::string, std::string>& attributes, const char* firstKey, const char* secondKey)
{
    const std::map<std::string, std::string>::const_iterator first = attributes.find(firstKey);
    if (first != attributes.end()) {
        return &first->second;
    }
    if (secondKey == nullptr) {
        return nullptr;
    }
    const std::map<std::string, std::string>::const_iterator second = attributes.find(secondKey);
    if (second != attributes.end()) {
        return &second->second;
    }
    return nullptr;
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

#if defined(RAWGL_HAS_OPENJPEG)
struct OpenJpegMessageState {
    std::string error;
    std::string warning;
};

struct Jpeg2000LoadSettings {
    uint32_t reduceFactor = 0;
    uint32_t layerLimit = 0;
};

struct Jpeg2000SaveSettings {
    bool lossless = true;
    bool hasCompressionRatio = false;
    float compressionRatio = 0.0f;
    bool hasQuality = false;
    float quality = 0.0f;
};

static void
openjpeg_error_callback(const char* message, void* userData)
{
    if (userData == nullptr || message == nullptr) {
        return;
    }
    OpenJpegMessageState* state = static_cast<OpenJpegMessageState*>(userData);
    if (state->error.empty()) {
        state->error = message;
    }
}

static void
openjpeg_warning_callback(const char* message, void* userData)
{
    if (userData == nullptr || message == nullptr) {
        return;
    }
    OpenJpegMessageState* state = static_cast<OpenJpegMessageState*>(userData);
    if (state->warning.empty()) {
        state->warning = message;
    }
}

static OPJ_CODEC_FORMAT
resolve_openjpeg_codec_format(const std::string& path)
{
    const std::string extension = get_file_ext(path);
    if (extension == "j2k" || extension == "j2c" || extension == "jpc") {
        return OPJ_CODEC_J2K;
    }
    return OPJ_CODEC_JP2;
}

static bool
parse_jpeg2000_load_settings(const std::map<std::string, std::string>& attributes,
                             Jpeg2000LoadSettings& settings,
                             std::string& errorMessage)
{
    const std::string* reduce = find_attribute_value(attributes, "jpeg2000:reduce_factor", "jpeg2000:reduce");
    if (reduce != nullptr && !parse_u32_text(*reduce, settings.reduceFactor)) {
        errorMessage = "invalid JPEG-2000 reduce factor";
        return false;
    }

    const std::string* layer = find_attribute_value(attributes, "jpeg2000:layer_limit", "jpeg2000:layers");
    if (layer != nullptr && !parse_u32_text(*layer, settings.layerLimit)) {
        errorMessage = "invalid JPEG-2000 layer limit";
        return false;
    }

    return true;
}

static bool
parse_jpeg2000_save_settings(const std::map<std::string, std::string>& attributes,
                             Jpeg2000SaveSettings& settings,
                             std::string& errorMessage)
{
    const std::string* lossless = find_attribute_value(attributes, "jpeg2000:lossless", nullptr);
    if (lossless != nullptr && !parse_bool_text(*lossless, settings.lossless)) {
        errorMessage = "invalid JPEG-2000 lossless flag";
        return false;
    }

    const std::string* ratio = find_attribute_value(attributes, "jpeg2000:compression_ratio", "jpeg2000:rate");
    if (ratio != nullptr) {
        if (!parse_positive_float_text(*ratio, settings.compressionRatio)) {
            errorMessage = "invalid JPEG-2000 compression ratio";
            return false;
        }
        settings.hasCompressionRatio = true;
    }

    const std::string* quality = find_attribute_value(attributes, "jpeg2000:quality", "jpeg2000:psnr");
    if (quality != nullptr) {
        if (!parse_positive_float_text(*quality, settings.quality)) {
            errorMessage = "invalid JPEG-2000 quality value";
            return false;
        }
        settings.hasQuality = true;
    }

    if (settings.hasCompressionRatio && settings.hasQuality) {
        errorMessage = "JPEG-2000 compression ratio and quality cannot be used together";
        return false;
    }
    if (settings.lossless && (settings.hasCompressionRatio || settings.hasQuality)) {
        errorMessage = "JPEG-2000 lossless output cannot use lossy rate or quality options";
        return false;
    }

    return true;
}

static bool
validate_decoded_component_layout(const opj_image_t* image, int& width, int& height, ImageComponentType& componentType,
                                  std::string& errorMessage)
{
    if (image == nullptr || image->numcomps == 0u || image->numcomps > 4u) {
        errorMessage = "unsupported JPEG-2000 component count";
        return false;
    }

    const opj_image_comp_t& first = image->comps[0];
    if (first.w == 0u || first.h == 0u || first.sgnd != 0u || first.dx != 1u || first.dy != 1u) {
        errorMessage = "unsupported JPEG-2000 component layout";
        return false;
    }
    if (first.prec == 0u || first.prec > 16u) {
        errorMessage = "native JPEG-2000 decode currently supports 1-16 bit unsigned components";
        return false;
    }

    for (OPJ_UINT32 componentIndex = 1u; componentIndex < image->numcomps; ++componentIndex) {
        const opj_image_comp_t& component = image->comps[componentIndex];
        if (component.w != first.w || component.h != first.h || component.dx != first.dx || component.dy != first.dy
            || component.prec != first.prec || component.sgnd != first.sgnd) {
            errorMessage = "native JPEG-2000 decode requires matching component dimensions and precision";
            return false;
        }
    }

    width = static_cast<int>(first.w);
    height = static_cast<int>(first.h);
    componentType = first.prec <= 8u ? ImageComponentType::U8 : ImageComponentType::U16;
    return true;
}

static int
resolve_jpeg2000_alpha_channel(const opj_image_t* image)
{
    for (OPJ_UINT32 componentIndex = 0u; componentIndex < image->numcomps; ++componentIndex) {
        if (image->comps[componentIndex].alpha != 0u) {
            return static_cast<int>(componentIndex);
        }
    }
    if (image->numcomps == 2u || image->numcomps == 4u) {
        return static_cast<int>(image->numcomps - 1u);
    }
    return -1;
}

static uint32_t
scale_decoded_sample(const OPJ_INT32 value, const OPJ_UINT32 sourcePrecision, const OPJ_UINT32 targetPrecision)
{
    if (sourcePrecision == targetPrecision) {
        return static_cast<uint32_t>(std::max<OPJ_INT32>(value, 0));
    }

    const uint64_t sourceMax = (uint64_t { 1u } << sourcePrecision) - 1u;
    const uint64_t targetMax = (uint64_t { 1u } << targetPrecision) - 1u;
    const uint64_t clamped = static_cast<uint64_t>(
        std::clamp<OPJ_INT32>(value, 0, static_cast<OPJ_INT32>(sourceMax)));
    return static_cast<uint32_t>((clamped * targetMax + sourceMax / 2u) / sourceMax);
}

static bool
copy_openjpeg_image_to_decoded(const opj_image_t* image, DecodedImageData& result, std::string& errorMessage)
{
    ImageComponentType componentType = ImageComponentType::Unknown;
    int width = 0;
    int height = 0;
    if (!validate_decoded_component_layout(image, width, height, componentType, errorMessage)) {
        return false;
    }

    const OPJ_UINT32 targetPrecision = componentType == ImageComponentType::U8 ? 8u : 16u;
    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    const size_t channels = static_cast<size_t>(image->numcomps);
    const size_t bytesPerComponent = componentType == ImageComponentType::U8 ? 1u : 2u;

    result.width = width;
    result.height = height;
    result.channels = static_cast<int>(channels);
    result.alphaChannel = resolve_jpeg2000_alpha_channel(image);
    result.componentType = componentType;
    result.bytes.resize(pixelCount * channels * bytesPerComponent);

    for (size_t pixelIndex = 0u; pixelIndex < pixelCount; ++pixelIndex) {
        for (size_t channel = 0u; channel < channels; ++channel) {
            const opj_image_comp_t& component = image->comps[channel];
            const uint32_t sample =
                scale_decoded_sample(component.data[pixelIndex], component.prec, targetPrecision);
            const size_t destinationIndex = pixelIndex * channels + channel;
            if (componentType == ImageComponentType::U8) {
                result.bytes[destinationIndex] = static_cast<std::byte>(sample & 0xffu);
            } else {
                const uint16_t value = static_cast<uint16_t>(sample & 0xffffu);
                std::memcpy(result.bytes.data() + destinationIndex * sizeof(uint16_t), &value, sizeof(value));
            }
        }
    }

    return true;
}

static bool
fill_openjpeg_image_component_data(opj_image_t* image,
                                   const HostImageData& source,
                                   const ImageComponentType componentType,
                                   std::string& errorMessage)
{
    const size_t pixelCount = static_cast<size_t>(source.width) * static_cast<size_t>(source.height);
    const int bitDepth = componentType == ImageComponentType::U8 ? 8 : 16;

    for (size_t pixelIndex = 0u; pixelIndex < pixelCount; ++pixelIndex) {
        for (int channel = 0; channel < source.channels; ++channel) {
            const size_t sourceSampleIndex =
                pixelIndex * static_cast<size_t>(source.channels) + static_cast<size_t>(channel);
            uint32_t sample = 0u;
            if (source.glType == GL_UNSIGNED_BYTE && componentType == ImageComponentType::U8) {
                sample = reinterpret_cast<const uint8_t*>(source.bytes.data())[sourceSampleIndex];
            } else if (source.glType == GL_UNSIGNED_SHORT && componentType == ImageComponentType::U16) {
                uint16_t value = 0u;
                std::memcpy(&value, source.bytes.data() + sourceSampleIndex * sizeof(uint16_t), sizeof(value));
                sample = value;
            } else {
                bool supported = false;
                const float value = read_source_sample_as_unit_float(source, sourceSampleIndex, supported);
                if (!supported) {
                    errorMessage = "unsupported host image type for native JPEG-2000 write";
                    return false;
                }
                sample = bitDepth == 8 ? quantize_unit_float_to_u8(value) : quantize_unit_float_to_u16(value);
            }
            image->comps[channel].data[pixelIndex] = static_cast<OPJ_INT32>(sample);
        }
    }

    return true;
}

static opj_image_t*
create_openjpeg_image_from_host(const HostImageData& source,
                                const ImageComponentType componentType,
                                const int explicitAlphaChannel,
                                std::string& errorMessage)
{
    if (source.width <= 0 || source.height <= 0 || source.channels < 1 || source.channels > 4) {
        errorMessage = "invalid host image dimensions or channel count for JPEG-2000";
        return nullptr;
    }
    if (componentType != ImageComponentType::U8 && componentType != ImageComponentType::U16) {
        errorMessage = "native JPEG-2000 write currently supports only 8-bit and 16-bit output";
        return nullptr;
    }

    std::vector<opj_image_cmptparm_t> componentParameters(static_cast<size_t>(source.channels));
    const OPJ_UINT32 bitDepth = componentType == ImageComponentType::U8 ? 8u : 16u;
    for (int channel = 0; channel < source.channels; ++channel) {
        opj_image_cmptparm_t& component = componentParameters[static_cast<size_t>(channel)];
        component.dx = 1u;
        component.dy = 1u;
        component.w = static_cast<OPJ_UINT32>(source.width);
        component.h = static_cast<OPJ_UINT32>(source.height);
        component.x0 = 0u;
        component.y0 = 0u;
        component.prec = bitDepth;
        component.bpp = bitDepth;
        component.sgnd = 0u;
    }

    const OPJ_COLOR_SPACE colorSpace = source.channels <= 2 ? OPJ_CLRSPC_GRAY : OPJ_CLRSPC_SRGB;
    opj_image_t* image =
        opj_image_create(static_cast<OPJ_UINT32>(source.channels), componentParameters.data(), colorSpace);
    if (image == nullptr) {
        errorMessage = "OpenJPEG image allocation failed";
        return nullptr;
    }

    image->x0 = 0u;
    image->y0 = 0u;
    image->x1 = static_cast<OPJ_UINT32>(source.width);
    image->y1 = static_cast<OPJ_UINT32>(source.height);

    const int resolvedAlphaChannel = explicitAlphaChannel >= 0 ? explicitAlphaChannel : source.alphaChannel;
    if (resolvedAlphaChannel >= 0 && resolvedAlphaChannel < source.channels) {
        image->comps[resolvedAlphaChannel].alpha = 1u;
    }

    if (!fill_openjpeg_image_component_data(image, source, componentType, errorMessage)) {
        opj_image_destroy(image);
        return nullptr;
    }

    return image;
}

static bool
configure_openjpeg_encoder(opj_cparameters_t& parameters,
                           const Jpeg2000SaveSettings& settings,
                           const HostImageData& image,
                           std::string& errorMessage)
{
    const int minimumDimension = std::min(image.width, image.height);
    int resolutionCount = 1;
    int reducedDimension = minimumDimension;
    while (resolutionCount < parameters.numresolution && reducedDimension >= 4) {
        reducedDimension = (reducedDimension + 1) / 2;
        ++resolutionCount;
    }
    parameters.numresolution = resolutionCount;

    if (settings.lossless) {
        parameters.tcp_numlayers = 1;
        parameters.tcp_rates[0] = 0.0f;
        parameters.cp_disto_alloc = 1;
        parameters.irreversible = 0;
    } else if (settings.hasCompressionRatio) {
        parameters.tcp_numlayers = 1;
        parameters.tcp_rates[0] = settings.compressionRatio;
        parameters.cp_disto_alloc = 1;
        parameters.irreversible = 1;
    } else if (settings.hasQuality) {
        parameters.tcp_numlayers = 1;
        parameters.tcp_distoratio[0] = settings.quality;
        parameters.cp_fixed_quality = 1;
        parameters.irreversible = 1;
    } else {
        errorMessage = "JPEG-2000 lossy output requires compression ratio or quality";
        return false;
    }

    parameters.tcp_mct = image.channels >= 3 ? 1 : 0;
    return true;
}
#endif

}  // namespace

DecodedImageData
decode_jpeg2000_file(const std::string& path, const std::map<std::string, std::string>& attributes)
{
    DecodedImageData result;

#if !defined(RAWGL_HAS_OPENJPEG)
    (void)path;
    (void)attributes;
    result.errorMessage = "OpenJPEG support is not available";
    return result;
#else
    Jpeg2000LoadSettings settings;
    if (!parse_jpeg2000_load_settings(attributes, settings, result.errorMessage)) {
        return result;
    }

    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);
    parameters.cp_reduce = settings.reduceFactor;
    parameters.cp_layer = settings.layerLimit;

    OpenJpegMessageState messageState;
    opj_codec_t* codec = opj_create_decompress(resolve_openjpeg_codec_format(path));
    if (codec == nullptr) {
        result.errorMessage = "OpenJPEG decompressor allocation failed";
        return result;
    }
    opj_set_error_handler(codec, openjpeg_error_callback, &messageState);
    opj_set_warning_handler(codec, openjpeg_warning_callback, &messageState);

    opj_stream_t* stream = opj_stream_create_default_file_stream(path.c_str(), OPJ_TRUE);
    if (stream == nullptr) {
        opj_destroy_codec(codec);
        result.errorMessage = "can't open JPEG-2000 file";
        return result;
    }

    opj_image_t* image = nullptr;
    if (!opj_setup_decoder(codec, &parameters) || !opj_read_header(stream, codec, &image)
        || !opj_decode(codec, stream, image)) {
        if (image != nullptr) {
            opj_image_destroy(image);
        }
        opj_stream_destroy(stream);
        opj_destroy_codec(codec);
        result.errorMessage = messageState.error.empty() ? "OpenJPEG decode failed" : messageState.error;
        return result;
    }
    opj_end_decompress(codec, stream);

    std::string conversionError;
    if (!copy_openjpeg_image_to_decoded(image, result, conversionError)) {
        opj_image_destroy(image);
        opj_stream_destroy(stream);
        opj_destroy_codec(codec);
        result.errorMessage = conversionError;
        return result;
    }

    opj_image_destroy(image);
    opj_stream_destroy(stream);
    opj_destroy_codec(codec);

    result.success = true;
    return result;
#endif
}

bool
encode_jpeg2000_file(const std::string& path,
                     const std::map<std::string, std::string>& attributes,
                     int alphaChannel,
                     const HostImageData& image,
                     const ImageEncodeSettings& settings,
                     std::string& errorMessage)
{
#if !defined(RAWGL_HAS_OPENJPEG)
    (void)path;
    (void)attributes;
    (void)alphaChannel;
    (void)image;
    (void)settings;
    errorMessage = "OpenJPEG support is not available";
    return false;
#else
    Jpeg2000SaveSettings saveSettings;
    if (!parse_jpeg2000_save_settings(attributes, saveSettings, errorMessage)) {
        return false;
    }

    opj_image_t* openJpegImage = create_openjpeg_image_from_host(image, settings.componentType, alphaChannel,
                                                                 errorMessage);
    if (openJpegImage == nullptr) {
        return false;
    }

    opj_cparameters_t parameters;
    opj_set_default_encoder_parameters(&parameters);
    if (!configure_openjpeg_encoder(parameters, saveSettings, image, errorMessage)) {
        opj_image_destroy(openJpegImage);
        return false;
    }

    OpenJpegMessageState messageState;
    opj_codec_t* codec = opj_create_compress(resolve_openjpeg_codec_format(path));
    if (codec == nullptr) {
        opj_image_destroy(openJpegImage);
        errorMessage = "OpenJPEG compressor allocation failed";
        return false;
    }
    opj_set_error_handler(codec, openjpeg_error_callback, &messageState);
    opj_set_warning_handler(codec, openjpeg_warning_callback, &messageState);

    opj_stream_t* stream = opj_stream_create_default_file_stream(path.c_str(), OPJ_FALSE);
    if (stream == nullptr) {
        opj_destroy_codec(codec);
        opj_image_destroy(openJpegImage);
        errorMessage = "can't open JPEG-2000 file for writing";
        return false;
    }

    const bool encoded = opj_setup_encoder(codec, &parameters, openJpegImage)
                         && opj_start_compress(codec, openJpegImage, stream)
                         && opj_encode(codec, stream)
                         && opj_end_compress(codec, stream);

    opj_stream_destroy(stream);
    opj_destroy_codec(codec);
    opj_image_destroy(openJpegImage);

    if (!encoded) {
        errorMessage = messageState.error.empty() ? "OpenJPEG encode failed" : messageState.error;
        return false;
    }

    return true;
#endif
}

}  // namespace rawgl::io
