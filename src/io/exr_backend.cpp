// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "exr_backend.h"

#include "gl_utils.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

#if defined(RAWGL_HAS_OPENEXR)
#include <Imath/half.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfCompression.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfStringAttribute.h>
#include <OpenEXR/ImfTileDescription.h>
#include <OpenEXR/ImfTiledInputFile.h>
#include <OpenEXR/ImfTiledOutputFile.h>
#endif

namespace rawgl::io {
namespace {

#if defined(RAWGL_HAS_OPENEXR)
static constexpr const char* kExrStringAttributePrefix = "openexr:attribute:string:";

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

static const std::string*
find_attribute_value(const std::map<std::string, std::string>& attributes, std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        const auto it = attributes.find(key);
        if (it != attributes.end()) {
            return &it->second;
        }
    }
    return nullptr;
}

enum class ExrStorageLayout
{
    Auto,
    Scanlines,
    Tiles,
};

static OPENEXR_IMF_NAMESPACE::Compression
default_exr_compression()
{
    return OPENEXR_IMF_NAMESPACE::ZIP_COMPRESSION;
}

static bool
parse_exr_compression(const std::map<std::string, std::string>& attributes,
                      OPENEXR_IMF_NAMESPACE::Compression& compression,
                      std::string& errorMessage)
{
    const std::string* value = find_attribute_value(attributes, { "openexr:compression", "compression", "oiio:Compression" });
    if (!value) {
        compression = default_exr_compression();
        return true;
    }

    const std::string normalized = to_lower_copy(*value);
    OPENEXR_IMF_NAMESPACE::Compression parsed = OPENEXR_IMF_NAMESPACE::NUM_COMPRESSION_METHODS;
    OPENEXR_IMF_NAMESPACE::getCompressionIdFromName(normalized, parsed);
    if (!OPENEXR_IMF_NAMESPACE::isValidCompression(static_cast<int>(parsed))) {
        errorMessage = "unsupported OpenEXR compression mode";
        return false;
    }

    compression = parsed;
    return true;
}

static bool
parse_exr_line_order(const std::map<std::string, std::string>& attributes,
                     OPENEXR_IMF_NAMESPACE::LineOrder& lineOrder,
                     bool& present,
                     std::string& errorMessage)
{
    present = false;
    lineOrder = OPENEXR_IMF_NAMESPACE::INCREASING_Y;

    const std::string* lineOrderValue =
        find_attribute_value(attributes, { "openexr:lineOrder", "openexr:line_order" });
    if (!lineOrderValue) {
        return true;
    }

    present = true;
    const std::string normalized = to_lower_copy(*lineOrderValue);
    if (normalized == "increasing_y" || normalized == "increasing") {
        lineOrder = OPENEXR_IMF_NAMESPACE::INCREASING_Y;
    } else if (normalized == "decreasing_y" || normalized == "decreasing") {
        lineOrder = OPENEXR_IMF_NAMESPACE::DECREASING_Y;
    } else if (normalized == "random_y" || normalized == "random") {
        lineOrder = OPENEXR_IMF_NAMESPACE::RANDOM_Y;
    } else {
        errorMessage = "unsupported OpenEXR line order";
        return false;
    }

    return true;
}

static bool
parse_exr_dwa_level(const std::map<std::string, std::string>& attributes,
                    const OPENEXR_IMF_NAMESPACE::Compression compression,
                    float& dwaLevel,
                    bool& present,
                    std::string& errorMessage)
{
    present = false;
    dwaLevel = 45.0f;

    const std::string* dwaLevelValue =
        find_attribute_value(attributes, { "openexr:dwaCompressionLevel", "openexr:dwa_compression_level" });
    if (!dwaLevelValue) {
        return true;
    }
    if (compression != OPENEXR_IMF_NAMESPACE::DWAA_COMPRESSION
        && compression != OPENEXR_IMF_NAMESPACE::DWAB_COMPRESSION) {
        errorMessage = "OpenEXR DWA compression level requires DWAA or DWAB compression";
        return false;
    }

    errno = 0;
    char* end = nullptr;
    const char* start = dwaLevelValue->c_str();
    const float parsedValue = std::strtof(start, &end);
    if (errno != 0 || end == nullptr || end == start || *end != '\0' || !std::isfinite(parsedValue)
        || parsedValue < 0.0f) {
        errorMessage = "invalid OpenEXR DWA compression level";
        return false;
    }

    present = true;
    dwaLevel = parsedValue;
    return true;
}

static bool
apply_exr_writer_attributes(OPENEXR_IMF_NAMESPACE::Header& header,
                            const std::map<std::string, std::string>& attributes,
                            const OPENEXR_IMF_NAMESPACE::Compression compression,
                            std::string& errorMessage)
{
    OPENEXR_IMF_NAMESPACE::LineOrder lineOrder = OPENEXR_IMF_NAMESPACE::INCREASING_Y;
    bool hasLineOrder = false;
    if (!parse_exr_line_order(attributes, lineOrder, hasLineOrder, errorMessage)) {
        return false;
    }
    if (hasLineOrder) {
        header.lineOrder() = lineOrder;
    }

    float dwaLevel = 45.0f;
    bool hasDwaLevel = false;
    if (!parse_exr_dwa_level(attributes, compression, dwaLevel, hasDwaLevel, errorMessage)) {
        return false;
    }
    if (hasDwaLevel) {
        header.dwaCompressionLevel() = dwaLevel;
    }

    for (const auto& attribute : attributes) {
        if (attribute.first.rfind(kExrStringAttributePrefix, 0) != 0) {
            continue;
        }

        const std::string attributeName = attribute.first.substr(std::strlen(kExrStringAttributePrefix));
        if (attributeName.empty()) {
            continue;
        }

        header.insert(attributeName, OPENEXR_IMF_NAMESPACE::StringAttribute(attribute.second));
    }

    return true;
}

static bool
parse_bool_attribute(const std::map<std::string, std::string>& attributes,
                     std::initializer_list<const char*> keys,
                     bool& value,
                     bool& present,
                     std::string& errorMessage)
{
    present = false;
    value = false;

    const std::string* attribute = find_attribute_value(attributes, keys);
    if (!attribute) {
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

    errorMessage = "invalid OpenEXR boolean option";
    return false;
}

static bool
parse_u32_attribute(const std::map<std::string, std::string>& attributes,
                    std::initializer_list<const char*> keys,
                    bool& present,
                    uint32_t& value,
                    const char* invalidValueMessage,
                    std::string& errorMessage)
{
    const std::string* attribute = find_attribute_value(attributes, keys);
    if (!attribute) {
        present = false;
        value = 0u;
        return true;
    }

    if (attribute->empty()) {
        errorMessage = invalidValueMessage;
        return false;
    }

    for (const char digit : *attribute) {
        if (!std::isdigit(static_cast<unsigned char>(digit))) {
            errorMessage = invalidValueMessage;
            return false;
        }
    }

    errno = 0;
    char* end = nullptr;
    const unsigned long parsedValue = std::strtoul(attribute->c_str(), &end, 10);
    if (errno != 0 || end == nullptr || *end != '\0' || parsedValue == 0ul
        || parsedValue > static_cast<unsigned long>(std::numeric_limits<uint32_t>::max())) {
        errorMessage = invalidValueMessage;
        return false;
    }

    present = true;
    value = static_cast<uint32_t>(parsedValue);
    return true;
}

static bool
parse_exr_storage_layout(const std::map<std::string, std::string>& attributes,
                         ExrStorageLayout& layout,
                         std::string& errorMessage)
{
    layout = ExrStorageLayout::Auto;

    const std::string* value =
        find_attribute_value(attributes, { "openexr:layout", "openexr:storageLayout", "openexr:storage_layout" });
    if (value) {
        const std::string normalized = to_lower_copy(*value);
        if (normalized == "auto") {
            layout = ExrStorageLayout::Auto;
        } else if (normalized == "scanline" || normalized == "scanlines") {
            layout = ExrStorageLayout::Scanlines;
        } else if (normalized == "tile" || normalized == "tiles" || normalized == "tiled") {
            layout = ExrStorageLayout::Tiles;
        } else {
            errorMessage = "unsupported OpenEXR storage layout";
            return false;
        }
    }

    bool tiledValue = false;
    bool hasTiledAttribute = false;
    if (!parse_bool_attribute(
            attributes, { "openexr:tiled" }, tiledValue, hasTiledAttribute, errorMessage)) {
        return false;
    }
    if (!hasTiledAttribute) {
        return true;
    }

    if (tiledValue) {
        if (layout == ExrStorageLayout::Scanlines) {
            errorMessage = "conflicting OpenEXR layout options";
            return false;
        }
        layout = ExrStorageLayout::Tiles;
        return true;
    }

    if (layout == ExrStorageLayout::Tiles) {
        errorMessage = "conflicting OpenEXR layout options";
        return false;
    }
    layout = ExrStorageLayout::Scanlines;
    return true;
}

static bool
resolve_exr_tile_layout(const std::map<std::string, std::string>& attributes,
                        const ExrStorageLayout storageLayout,
                        bool& useTiles,
                        uint32_t& tileWidth,
                        uint32_t& tileHeight,
                        std::string& errorMessage)
{
    bool hasTileWidth = false;
    uint32_t requestedTileWidth = 0u;
    if (!parse_u32_attribute(attributes,
                             { "openexr:tileWidth", "openexr:tile_width" },
                             hasTileWidth,
                             requestedTileWidth,
                             "invalid positive OpenEXR tile width",
                             errorMessage)) {
        return false;
    }

    bool hasTileHeight = false;
    uint32_t requestedTileHeight = 0u;
    if (!parse_u32_attribute(attributes,
                             { "openexr:tileHeight", "openexr:tile_height" },
                             hasTileHeight,
                             requestedTileHeight,
                             "invalid positive OpenEXR tile height",
                             errorMessage)) {
        return false;
    }

    if (!hasTileHeight) {
        if (!parse_u32_attribute(attributes,
                                 { "openexr:tileLength", "openexr:tile_length" },
                                 hasTileHeight,
                                 requestedTileHeight,
                                 "invalid positive OpenEXR tile length",
                                 errorMessage)) {
            return false;
        }
    }

    if (storageLayout == ExrStorageLayout::Scanlines && (hasTileWidth || hasTileHeight)) {
        errorMessage = "OpenEXR tile dimensions are not valid for scanline output";
        return false;
    }

    useTiles = storageLayout == ExrStorageLayout::Tiles || hasTileWidth || hasTileHeight;
    if (!useTiles) {
        tileWidth = 0u;
        tileHeight = 0u;
        return true;
    }

    constexpr uint32_t defaultTileSize = 64u;
    tileWidth = hasTileWidth ? requestedTileWidth
                             : (hasTileHeight ? requestedTileHeight : defaultTileSize);
    tileHeight = hasTileHeight ? requestedTileHeight : tileWidth;

    if (tileWidth == 0u || tileHeight == 0u) {
        errorMessage = "invalid OpenEXR tile dimensions";
        return false;
    }

    return true;
}

static const char*
to_exr_line_order_name(const OPENEXR_IMF_NAMESPACE::LineOrder lineOrder) noexcept
{
    switch (lineOrder) {
    case OPENEXR_IMF_NAMESPACE::INCREASING_Y: return "increasing_y";
    case OPENEXR_IMF_NAMESPACE::DECREASING_Y: return "decreasing_y";
    case OPENEXR_IMF_NAMESPACE::RANDOM_Y: return "random_y";
    case OPENEXR_IMF_NAMESPACE::NUM_LINEORDERS: break;
    }

    return "increasing_y";
}

static bool
select_exr_channels(const OPENEXR_IMF_NAMESPACE::ChannelList& channelList,
                    std::vector<std::string>& selectedNames,
                    int& alphaChannel,
                    std::string& errorMessage)
{
    selectedNames.clear();
    alphaChannel = -1;

    const OPENEXR_IMF_NAMESPACE::Channel* channelR = channelList.findChannel("R");
    const OPENEXR_IMF_NAMESPACE::Channel* channelG = channelList.findChannel("G");
    const OPENEXR_IMF_NAMESPACE::Channel* channelB = channelList.findChannel("B");
    const OPENEXR_IMF_NAMESPACE::Channel* channelA = channelList.findChannel("A");
    const OPENEXR_IMF_NAMESPACE::Channel* channelY = channelList.findChannel("Y");

    if (channelR && channelG && channelB) {
        selectedNames.push_back("R");
        selectedNames.push_back("G");
        selectedNames.push_back("B");
        if (channelA) {
            selectedNames.push_back("A");
            alphaChannel = 3;
        }
    } else if (channelY) {
        selectedNames.push_back("Y");
        if (channelA) {
            selectedNames.push_back("A");
            alphaChannel = 1;
        }
    } else {
        for (OPENEXR_IMF_NAMESPACE::ChannelList::ConstIterator it = channelList.begin(); it != channelList.end(); ++it) {
            selectedNames.push_back(it.name());
            if (selectedNames.size() == 4u) {
                break;
            }
        }

        if (selectedNames.empty()) {
            errorMessage = "OpenEXR file has no channels";
            return false;
        }

        for (size_t index = 0; index < selectedNames.size(); ++index) {
            if (selectedNames[index] == "A") {
                alphaChannel = static_cast<int>(index);
                break;
            }
        }
    }

    for (const std::string& channelName : selectedNames) {
        const OPENEXR_IMF_NAMESPACE::Channel* channel = channelList.findChannel(channelName);
        if (!channel) {
            errorMessage = "OpenEXR channel selection failed";
            return false;
        }
        if (channel->xSampling != 1 || channel->ySampling != 1) {
            errorMessage = "native OpenEXR decode does not support subsampled channels yet";
            return false;
        }
    }

    return true;
}

static bool
resolve_exr_component_type(const OPENEXR_IMF_NAMESPACE::ChannelList& channelList,
                           const std::vector<std::string>& selectedNames,
                           OPENEXR_IMF_NAMESPACE::PixelType& pixelType,
                           ImageComponentType& componentType)
{
    if (selectedNames.empty()) {
        return false;
    }

    OPENEXR_IMF_NAMESPACE::PixelType firstType = OPENEXR_IMF_NAMESPACE::NUM_PIXELTYPES;
    for (const std::string& channelName : selectedNames) {
        const OPENEXR_IMF_NAMESPACE::Channel* channel = channelList.findChannel(channelName);
        if (!channel) {
            return false;
        }
        if (firstType == OPENEXR_IMF_NAMESPACE::NUM_PIXELTYPES) {
            firstType = channel->type;
        } else if (channel->type != firstType) {
            firstType = OPENEXR_IMF_NAMESPACE::FLOAT;
            break;
        }
    }

    if (firstType == OPENEXR_IMF_NAMESPACE::HALF) {
        pixelType = OPENEXR_IMF_NAMESPACE::HALF;
        componentType = ImageComponentType::F16;
        return true;
    }
    if (firstType == OPENEXR_IMF_NAMESPACE::FLOAT) {
        pixelType = OPENEXR_IMF_NAMESPACE::FLOAT;
        componentType = ImageComponentType::F32;
        return true;
    }
    if (firstType == OPENEXR_IMF_NAMESPACE::UINT) {
        pixelType = OPENEXR_IMF_NAMESPACE::UINT;
        componentType = ImageComponentType::U32;
        return true;
    }

    return false;
}

static bool
prepare_exr_decode_target(const OPENEXR_IMF_NAMESPACE::Header& header,
                          IMATH_NAMESPACE::Box2i& dataWindow,
                          std::vector<std::string>& selectedNames,
                          OPENEXR_IMF_NAMESPACE::PixelType& pixelType,
                          DecodedImageData& result,
                          std::string& errorMessage)
{
    dataWindow = header.dataWindow();
    const int width = dataWindow.max.x - dataWindow.min.x + 1;
    const int height = dataWindow.max.y - dataWindow.min.y + 1;
    if (width <= 0 || height <= 0) {
        errorMessage = "invalid OpenEXR data window";
        return false;
    }

    int alphaChannel = -1;
    if (!select_exr_channels(header.channels(), selectedNames, alphaChannel, errorMessage)) {
        return false;
    }

    ImageComponentType componentType = ImageComponentType::Unknown;
    if (!resolve_exr_component_type(header.channels(), selectedNames, pixelType, componentType)) {
        errorMessage = "unsupported OpenEXR channel type";
        return false;
    }

    const size_t bytesPerComponent = byte_size_for_image_component(componentType);
    if (bytesPerComponent == 0u) {
        errorMessage = "unsupported OpenEXR component type";
        return false;
    }

    result.width = width;
    result.height = height;
    result.channels = static_cast<int>(selectedNames.size());
    result.alphaChannel = alphaChannel;
    result.componentType = componentType;
    result.bytes.resize(
        static_cast<size_t>(width) * static_cast<size_t>(height) * selectedNames.size() * bytesPerComponent);
    return true;
}

static OPENEXR_IMF_NAMESPACE::FrameBuffer
build_exr_interleaved_frame_buffer(const IMATH_NAMESPACE::Box2i& dataWindow,
                                   const std::vector<std::string>& selectedNames,
                                   OPENEXR_IMF_NAMESPACE::PixelType pixelType,
                                   size_t bytesPerComponent,
                                   std::byte* bytes)
{
    OPENEXR_IMF_NAMESPACE::FrameBuffer frameBuffer;
    const size_t xStride = selectedNames.size() * bytesPerComponent;
    const size_t yStride = static_cast<size_t>(dataWindow.max.x - dataWindow.min.x + 1) * xStride;
    for (size_t channelIndex = 0; channelIndex < selectedNames.size(); ++channelIndex) {
        char* base = reinterpret_cast<char*>(bytes + channelIndex * bytesPerComponent);
        frameBuffer.insert(selectedNames[channelIndex],
                           OPENEXR_IMF_NAMESPACE::Slice::Make(pixelType, base, dataWindow, xStride, yStride));
    }

    return frameBuffer;
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
read_source_sample_as_float(const HostImageData& image, const size_t sampleIndex, bool& supported)
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
    case GL_HALF_FLOAT: {
        uint16_t value = 0u;
        std::memcpy(&value, image.bytes.data() + sampleIndex * sizeof(uint16_t), sizeof(value));
        return half_to_float(value);
    }
    case GL_FLOAT: {
        float value = 0.0f;
        std::memcpy(&value, image.bytes.data() + sampleIndex * sizeof(float), sizeof(value));
        return value;
    }
    default: break;
    }

    supported = false;
    return 0.0f;
}

static bool
resolve_exr_output_layout(const HostImageData& image,
                          const int explicitAlphaChannel,
                          int& outputChannels,
                          int& resolvedAlphaChannel,
                          std::array<int, 4>& sourceChannelMap,
                          std::array<const char*, 4>& channelNames,
                          std::string& errorMessage)
{
    if (image.width <= 0 || image.height <= 0 || image.channels < 1 || image.channels > 4) {
        errorMessage = "invalid host image dimensions or channel count for OpenEXR";
        return false;
    }

    outputChannels = image.channels;
    resolvedAlphaChannel = explicitAlphaChannel >= 0 ? explicitAlphaChannel : image.alphaChannel;
    if ((image.channels == 2 || image.channels == 4) && resolvedAlphaChannel < 0) {
        resolvedAlphaChannel = image.channels - 1;
    }
    if (resolvedAlphaChannel >= image.channels) {
        errorMessage = "invalid OpenEXR alpha channel index";
        return false;
    }

    if (image.channels == 1) {
        sourceChannelMap = { 0, 0, 0, 0 };
        channelNames = { "Y", nullptr, nullptr, nullptr };
        return true;
    }
    if (image.channels == 2 && resolvedAlphaChannel >= 0) {
        const int valueChannel = resolvedAlphaChannel == 0 ? 1 : 0;
        sourceChannelMap = { valueChannel, resolvedAlphaChannel, 0, 0 };
        channelNames = { "Y", "A", nullptr, nullptr };
        return true;
    }
    if (image.channels == 3 && resolvedAlphaChannel < 0) {
        sourceChannelMap = { 0, 1, 2, 0 };
        channelNames = { "R", "G", "B", nullptr };
        return true;
    }
    if (image.channels == 4 && resolvedAlphaChannel >= 0) {
        int nextChannel = 0;
        for (int sourceChannel = 0; sourceChannel < image.channels; ++sourceChannel) {
            if (sourceChannel == resolvedAlphaChannel) {
                continue;
            }
            sourceChannelMap[nextChannel++] = sourceChannel;
        }
        sourceChannelMap[3] = resolvedAlphaChannel;
        channelNames = { "R", "G", "B", "A" };
        return true;
    }

    errorMessage = "native OpenEXR write supports only Y, YA, RGB, and RGBA layouts";
    return false;
}

static bool
convert_host_image_to_exr_bytes(const HostImageData& image,
                                const ImageEncodeSettings& settings,
                                const int explicitAlphaChannel,
                                int& outputChannels,
                                std::array<const char*, 4>& channelNames,
                                std::vector<std::byte>& bytes,
                                std::string& errorMessage)
{
    int resolvedAlphaChannel = -1;
    std::array<int, 4> sourceChannelMap = { 0, 1, 2, 3 };
    if (!resolve_exr_output_layout(
            image, explicitAlphaChannel, outputChannels, resolvedAlphaChannel, sourceChannelMap, channelNames, errorMessage)) {
        return false;
    }

    const size_t pixelCount = static_cast<size_t>(image.width) * static_cast<size_t>(image.height);
    const size_t sampleCount = pixelCount * static_cast<size_t>(outputChannels);

    if (settings.componentType == ImageComponentType::F16) {
        bytes.resize(sampleCount * sizeof(IMATH_NAMESPACE::half));
        IMATH_NAMESPACE::half* destination = reinterpret_cast<IMATH_NAMESPACE::half*>(bytes.data());
        for (size_t pixelIndex = 0; pixelIndex < pixelCount; ++pixelIndex) {
            for (int destinationChannel = 0; destinationChannel < outputChannels; ++destinationChannel) {
                const size_t sampleIndex = pixelIndex * static_cast<size_t>(image.channels)
                                           + static_cast<size_t>(sourceChannelMap[destinationChannel]);
                bool supported = false;
                const float value = read_source_sample_as_float(image, sampleIndex, supported);
                if (!supported) {
                    errorMessage = "unsupported host image type for native OpenEXR write";
                    return false;
                }
                destination[pixelIndex * static_cast<size_t>(outputChannels) + static_cast<size_t>(destinationChannel)]
                    = IMATH_NAMESPACE::half(value);
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
                const float value = read_source_sample_as_float(image, sampleIndex, supported);
                if (!supported) {
                    errorMessage = "unsupported host image type for native OpenEXR write";
                    return false;
                }
                destination[pixelIndex * static_cast<size_t>(outputChannels) + static_cast<size_t>(destinationChannel)]
                    = value;
            }
        }
        return true;
    }

    errorMessage = "native OpenEXR write currently supports only half and float output";
    return false;
}
#endif

}  // namespace

DecodedImageData
decode_exr_file(const std::string& path)
{
    DecodedImageData result;

#if !defined(RAWGL_HAS_OPENEXR)
    (void)path;
    result.errorMessage = "OpenEXR support is not available";
    return result;
#else
    try {
        OPENEXR_IMF_NAMESPACE::InputFile file(path.c_str());
        const OPENEXR_IMF_NAMESPACE::Header& header = file.header();
        IMATH_NAMESPACE::Box2i dataWindow;
        std::vector<std::string> selectedNames;
        std::string errorMessage;
        OPENEXR_IMF_NAMESPACE::PixelType pixelType = OPENEXR_IMF_NAMESPACE::NUM_PIXELTYPES;
        if (!prepare_exr_decode_target(header, dataWindow, selectedNames, pixelType, result, errorMessage)) {
            result.errorMessage = errorMessage;
            return result;
        }

        const size_t bytesPerComponent = byte_size_for_image_component(result.componentType);
        if (bytesPerComponent == 0u) {
            result.errorMessage = "unsupported OpenEXR component type";
            return result;
        }

        OPENEXR_IMF_NAMESPACE::FrameBuffer frameBuffer =
            build_exr_interleaved_frame_buffer(dataWindow, selectedNames, pixelType, bytesPerComponent, result.bytes.data());

        if (header.hasTileDescription()) {
            OPENEXR_IMF_NAMESPACE::TiledInputFile tiledFile(path.c_str());
            if (tiledFile.levelMode() != OPENEXR_IMF_NAMESPACE::ONE_LEVEL) {
                result.errorMessage = "native OpenEXR decode supports only one-level tiled images yet";
                return result;
            }
            tiledFile.setFrameBuffer(frameBuffer);
            tiledFile.readTiles(0, tiledFile.numXTiles(0) - 1, 0, tiledFile.numYTiles(0) - 1, 0);
        } else {
            file.setFrameBuffer(frameBuffer);
            file.readPixels(dataWindow.min.y, dataWindow.max.y);
        }

        result.success = true;
        return result;
    } catch (const std::exception& e) {
        result.errorMessage = e.what();
        return result;
    }
#endif
}

bool
encode_exr_file(const std::string& path,
                const std::map<std::string, std::string>& attributes,
                int alphaChannel,
                const HostImageData& image,
                const ImageEncodeSettings& settings,
                std::string& errorMessage)
{
#if !defined(RAWGL_HAS_OPENEXR)
    (void)path;
    (void)attributes;
    (void)alphaChannel;
    (void)image;
    (void)settings;
    errorMessage = "OpenEXR support is not available";
    return false;
#else
    OPENEXR_IMF_NAMESPACE::Compression compression = default_exr_compression();
    if (!parse_exr_compression(attributes, compression, errorMessage)) {
        return false;
    }

    ExrStorageLayout storageLayout = ExrStorageLayout::Auto;
    if (!parse_exr_storage_layout(attributes, storageLayout, errorMessage)) {
        return false;
    }

    bool useTiles = false;
    uint32_t tileWidth = 0u;
    uint32_t tileHeight = 0u;
    if (!resolve_exr_tile_layout(attributes, storageLayout, useTiles, tileWidth, tileHeight, errorMessage)) {
        return false;
    }

    int outputChannels = 0;
    std::array<const char*, 4> channelNames = { nullptr, nullptr, nullptr, nullptr };
    std::vector<std::byte> encodedBytes;
    if (!convert_host_image_to_exr_bytes(
            image, settings, alphaChannel, outputChannels, channelNames, encodedBytes, errorMessage)) {
        return false;
    }

    try {
        OPENEXR_IMF_NAMESPACE::Header header(image.width, image.height);
        header.compression() = compression;
        if (useTiles) {
            header.setTileDescription(OPENEXR_IMF_NAMESPACE::TileDescription(
                tileWidth, tileHeight, OPENEXR_IMF_NAMESPACE::ONE_LEVEL, OPENEXR_IMF_NAMESPACE::ROUND_DOWN));
        }
        if (!apply_exr_writer_attributes(header, attributes, compression, errorMessage)) {
            return false;
        }

        OPENEXR_IMF_NAMESPACE::PixelType pixelType = OPENEXR_IMF_NAMESPACE::FLOAT;
        if (settings.componentType == ImageComponentType::F16) {
            pixelType = OPENEXR_IMF_NAMESPACE::HALF;
        } else if (settings.componentType == ImageComponentType::F32) {
            pixelType = OPENEXR_IMF_NAMESPACE::FLOAT;
        } else {
            errorMessage = "unsupported OpenEXR output component type";
            return false;
        }

        for (int channelIndex = 0; channelIndex < outputChannels; ++channelIndex) {
            header.channels().insert(channelNames[channelIndex], OPENEXR_IMF_NAMESPACE::Channel(pixelType));
        }

        const size_t bytesPerComponent = byte_size_for_image_component(settings.componentType);
        std::vector<std::string> selectedNames;
        selectedNames.reserve(static_cast<size_t>(outputChannels));
        for (int channelIndex = 0; channelIndex < outputChannels; ++channelIndex) {
            selectedNames.emplace_back(channelNames[channelIndex]);
        }
        const IMATH_NAMESPACE::Box2i dataWindow(
            IMATH_NAMESPACE::V2i(0, 0), IMATH_NAMESPACE::V2i(image.width - 1, image.height - 1));
        OPENEXR_IMF_NAMESPACE::FrameBuffer frameBuffer =
            build_exr_interleaved_frame_buffer(dataWindow, selectedNames, pixelType, bytesPerComponent, encodedBytes.data());

        if (useTiles) {
            OPENEXR_IMF_NAMESPACE::TiledOutputFile output(path.c_str(), header);
            output.setFrameBuffer(frameBuffer);
            output.writeTiles(0, output.numXTiles(0) - 1, 0, output.numYTiles(0) - 1, 0);
        } else {
            OPENEXR_IMF_NAMESPACE::OutputFile output(path.c_str(), header);
            output.setFrameBuffer(frameBuffer);
            output.writePixels(image.height);
        }
        return true;
    } catch (const std::exception& e) {
        errorMessage = e.what();
        return false;
    }
#endif
}

bool
extract_exr_reencode_attributes(const std::string& path,
                                std::map<std::string, std::string>& attributes,
                                std::string& errorMessage)
{
#if !defined(RAWGL_HAS_OPENEXR)
    (void)path;
    (void)attributes;
    errorMessage = "OpenEXR support is not available";
    return false;
#else
    try {
        OPENEXR_IMF_NAMESPACE::InputFile file(path.c_str());
        const OPENEXR_IMF_NAMESPACE::Header& header = file.header();

        std::string compressionName;
        OPENEXR_IMF_NAMESPACE::getCompressionNameFromId(header.compression(), compressionName);
        if (!compressionName.empty()) {
            attributes["openexr:compression"] = compressionName;
        }

        attributes["openexr:lineOrder"] = to_exr_line_order_name(header.lineOrder());

        if (header.hasTileDescription()) {
            const OPENEXR_IMF_NAMESPACE::TileDescription& tileDescription = header.tileDescription();
            attributes["openexr:tiled"] = "true";
            attributes["openexr:tileWidth"] = std::to_string(tileDescription.xSize);
            attributes["openexr:tileHeight"] = std::to_string(tileDescription.ySize);
        }

        if (header.compression() == OPENEXR_IMF_NAMESPACE::DWAA_COMPRESSION
            || header.compression() == OPENEXR_IMF_NAMESPACE::DWAB_COMPRESSION) {
            attributes["openexr:dwaCompressionLevel"] = std::to_string(header.dwaCompressionLevel());
        }

        return true;
    } catch (const std::exception& e) {
        errorMessage = e.what();
        return false;
    }
#endif
}

}  // namespace rawgl::io
