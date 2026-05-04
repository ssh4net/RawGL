// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "tiff_backend.h"

#include "gl_utils.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
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

enum class TiffStorageLayout
{
    Auto,
    Strips,
    Tiles,
};

struct TiffSaveOptions {
    uint16_t compression = COMPRESSION_NONE;
    uint16_t predictor = PREDICTOR_NONE;
    TiffStorageLayout storageLayout = TiffStorageLayout::Auto;
    bool forceBigTiff = false;
    bool unassociatedAlpha = false;
    bool hasRowsPerStrip = false;
    uint32_t requestedRowsPerStrip = 0u;
    bool hasTileWidth = false;
    uint32_t requestedTileWidth = 0u;
    bool hasTileLength = false;
    uint32_t requestedTileLength = 0u;
    bool hasJpegQuality = false;
    uint32_t jpegQuality = 0u;
    bool hasZipQuality = false;
    uint32_t zipQuality = 0u;
    bool hasZstdLevel = false;
    uint32_t zstdLevel = 0u;
    bool hasLzmaPreset = false;
    uint32_t lzmaPreset = 0u;
    bool hasWebpLevel = false;
    uint32_t webpLevel = 0u;
    bool hasWebpLossless = false;
    bool webpLossless = false;
    bool hasWebpLosslessExact = false;
    bool webpLosslessExact = false;
};

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
    const std::string* value = find_attribute_value(attributes, { "tiff:compression", "compression", "oiio:Compression" });
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
    } else if (normalized == "jpeg" || normalized == "jpg") {
#if defined(COMPRESSION_JPEG)
        compression = COMPRESSION_JPEG;
#else
        errorMessage = "TIFF JPEG compression is not available in this libtiff header";
        return false;
#endif
    } else if (normalized == "lzma") {
#if defined(COMPRESSION_LZMA)
        compression = COMPRESSION_LZMA;
#else
        errorMessage = "TIFF LZMA compression is not available in this libtiff header";
        return false;
#endif
    } else if (normalized == "zstd") {
#if defined(COMPRESSION_ZSTD)
        compression = COMPRESSION_ZSTD;
#else
        errorMessage = "TIFF ZSTD compression is not available in this libtiff header";
        return false;
#endif
    } else if (normalized == "webp") {
#if defined(COMPRESSION_WEBP)
        compression = COMPRESSION_WEBP;
#else
        errorMessage = "TIFF WebP compression is not available in this libtiff header";
        return false;
#endif
    } else if (normalized == "jxl" || normalized == "jpegxl" || normalized == "jpeg_xl") {
#if defined(COMPRESSION_JXL)
        compression = COMPRESSION_JXL;
#else
        errorMessage = "TIFF JPEG XL compression is not available in this libtiff header";
        return false;
#endif
    } else if (normalized == "jxl_dng" || normalized == "jpegxl_dng" || normalized == "dng_jxl") {
#if defined(COMPRESSION_JXL_DNG_1_7)
        compression = COMPRESSION_JXL_DNG_1_7;
#else
        errorMessage = "TIFF DNG JPEG XL compression is not available in this libtiff header";
        return false;
#endif
    } else if (normalized == "lerc") {
#if defined(COMPRESSION_LERC)
        compression = COMPRESSION_LERC;
#else
        errorMessage = "TIFF LERC compression is not available in this libtiff header";
        return false;
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

static bool
parse_bool_attribute(const std::map<std::string, std::string>& attributes, const char* key, bool& value)
{
    const std::string* attribute = find_attribute_value(attributes, key);
    if (!attribute) {
        return false;
    }

    const std::string normalized = to_lower_copy(*attribute);
    value = normalized == "1" || normalized == "true" || normalized == "on" || normalized == "yes";
    return true;
}

static bool
parse_bool_attribute(const std::map<std::string, std::string>& attributes,
                     std::initializer_list<const char*> keys,
                     bool& value)
{
    const std::string* attribute = find_attribute_value(attributes, keys);
    if (!attribute) {
        return false;
    }

    const std::string normalized = to_lower_copy(*attribute);
    value = normalized == "1" || normalized == "true" || normalized == "on" || normalized == "yes";
    return true;
}

static bool
parse_u32_attribute(const std::map<std::string, std::string>& attributes,
                    std::initializer_list<const char*> keys,
                    const uint32_t minimumValue,
                    const uint32_t maximumValue,
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
    const unsigned long long parsedValue = std::strtoull(attribute->c_str(), &end, 10);
    if (errno != 0 || end == nullptr || *end != '\0' || parsedValue < minimumValue || parsedValue > maximumValue) {
        errorMessage = invalidValueMessage;
        return false;
    }

    present = true;
    value = static_cast<uint32_t>(parsedValue);
    return true;
}

static bool
parse_tiff_storage_layout(const std::map<std::string, std::string>& attributes,
                          TiffStorageLayout& layout,
                          std::string& errorMessage)
{
    layout = TiffStorageLayout::Auto;

    const std::string* value =
        find_attribute_value(attributes, { "tiff:layout", "tiff:storageLayout", "tiff:storage_layout", "tiff:storage" });
    if (value) {
        const std::string normalized = to_lower_copy(*value);
        if (normalized == "auto") {
            layout = TiffStorageLayout::Auto;
        } else if (normalized == "strip" || normalized == "strips" || normalized == "scanline"
                   || normalized == "scanlines") {
            layout = TiffStorageLayout::Strips;
        } else if (normalized == "tile" || normalized == "tiles" || normalized == "tiled") {
            layout = TiffStorageLayout::Tiles;
        } else {
            errorMessage = "unsupported TIFF storage layout";
            return false;
        }
    }

    bool tiledValue = false;
    const bool hasTiledAttribute = parse_bool_attribute(attributes, "tiff:tiled", tiledValue);
    if (!hasTiledAttribute) {
        return true;
    }

    if (tiledValue) {
        if (layout == TiffStorageLayout::Strips) {
            errorMessage = "conflicting TIFF layout options";
            return false;
        }
        layout = TiffStorageLayout::Tiles;
        return true;
    }

    if (layout == TiffStorageLayout::Tiles) {
        errorMessage = "conflicting TIFF layout options";
        return false;
    }

    layout = TiffStorageLayout::Strips;
    return true;
}

static bool
parse_tiff_predictor(const std::map<std::string, std::string>& attributes,
                     const ImageComponentType componentType,
                     const uint16_t compression,
                     uint16_t& predictor,
                     std::string& errorMessage)
{
    const std::string* value = find_attribute_value(attributes, "tiff:predictor");
    if (!value) {
        predictor = PREDICTOR_NONE;
        return true;
    }

    const std::string normalized = to_lower_copy(*value);
    if (normalized == "1" || normalized == "none") {
        predictor = PREDICTOR_NONE;
    } else if (normalized == "2" || normalized == "horizontal" || normalized == "horizontal_differencing") {
        predictor = PREDICTOR_HORIZONTAL;
    } else if (
        normalized == "3" || normalized == "float" || normalized == "floatingpoint"
        || normalized == "floating_point") {
        predictor = PREDICTOR_FLOATINGPOINT;
    } else {
        errorMessage = "unsupported TIFF predictor mode";
        return false;
    }

    if (predictor == PREDICTOR_NONE) {
        return true;
    }

    if (compression != COMPRESSION_LZW
#if defined(COMPRESSION_ADOBE_DEFLATE)
        && compression != COMPRESSION_ADOBE_DEFLATE
#endif
        && compression != COMPRESSION_DEFLATE) {
        errorMessage = "TIFF predictor requires LZW or Deflate compression";
        return false;
    }

    if (predictor == PREDICTOR_FLOATINGPOINT && componentType != ImageComponentType::F32) {
        errorMessage = "floating-point TIFF predictor requires 32-bit float output";
        return false;
    }

    if (predictor == PREDICTOR_HORIZONTAL && componentType == ImageComponentType::F32) {
        errorMessage = "horizontal TIFF predictor is not supported for 32-bit float output";
        return false;
    }

    return true;
}

static bool
resolve_tiff_strip_layout(const TiffSaveOptions& options,
                          TIFF* tif,
                          const bool useTiles,
                          uint32_t& rowsPerStrip,
                          std::string& errorMessage)
{
    if (useTiles) {
        if (options.hasRowsPerStrip) {
            errorMessage = "tiff:rowsPerStrip is not valid for tiled TIFF output";
            return false;
        }
        rowsPerStrip = 0u;
        return true;
    }

    rowsPerStrip = TIFFDefaultStripSize(tif, options.hasRowsPerStrip ? options.requestedRowsPerStrip : 0u);
    if (rowsPerStrip == 0u) {
        errorMessage = "invalid TIFF rows-per-strip geometry";
        return false;
    }

    return true;
}

static bool
parse_tiff_layout_options(const std::map<std::string, std::string>& attributes,
                          TiffSaveOptions& options,
                          std::string& errorMessage)
{
    if (!parse_tiff_storage_layout(attributes, options.storageLayout, errorMessage)) {
        return false;
    }

    if (!parse_u32_attribute(attributes,
                             { "tiff:rowsPerStrip", "tiff:rows_per_strip" },
                             1u,
                             std::numeric_limits<uint32_t>::max(),
                             options.hasRowsPerStrip,
                             options.requestedRowsPerStrip,
                             "invalid positive TIFF rows-per-strip value",
                             errorMessage)) {
        return false;
    }

    if (!parse_u32_attribute(attributes,
                             { "tiff:tileWidth", "tiff:tile_width" },
                             1u,
                             std::numeric_limits<uint32_t>::max(),
                             options.hasTileWidth,
                             options.requestedTileWidth,
                             "invalid positive TIFF tile width",
                             errorMessage)) {
        return false;
    }

    if (!parse_u32_attribute(attributes,
                             { "tiff:tileLength", "tiff:tile_length" },
                             1u,
                             std::numeric_limits<uint32_t>::max(),
                             options.hasTileLength,
                             options.requestedTileLength,
                             "invalid positive TIFF tile length",
                             errorMessage)) {
        return false;
    }

    if (!options.hasTileLength) {
        if (!parse_u32_attribute(attributes,
                                 { "tiff:tileHeight", "tiff:tile_height" },
                                 1u,
                                 std::numeric_limits<uint32_t>::max(),
                                 options.hasTileLength,
                                 options.requestedTileLength,
                                 "invalid positive TIFF tile height",
                                 errorMessage)) {
            return false;
        }
    }

    if (options.storageLayout == TiffStorageLayout::Strips && (options.hasTileWidth || options.hasTileLength)) {
        errorMessage = "TIFF tile dimensions are not valid for stripped TIFF output";
        return false;
    }

    const bool useTiles = options.storageLayout == TiffStorageLayout::Tiles || options.hasTileWidth || options.hasTileLength;
    if (useTiles && options.hasRowsPerStrip) {
        errorMessage = "tiff:rowsPerStrip is not valid for tiled TIFF output";
        return false;
    }

    return true;
}

static bool
parse_tiff_compression_u32_option(const std::map<std::string, std::string>& attributes,
                                  const uint16_t compression,
                                  const uint16_t expectedCompression,
                                  std::initializer_list<const char*> keys,
                                  const uint32_t minimumValue,
                                  const uint32_t maximumValue,
                                  bool& present,
                                  uint32_t& value,
                                  const char* invalidValueMessage,
                                  const char* wrongCompressionMessage,
                                  std::string& errorMessage)
{
    if (!parse_u32_attribute(
            attributes, keys, minimumValue, maximumValue, present, value, invalidValueMessage, errorMessage)) {
        return false;
    }
    if (!present) {
        return true;
    }
    if (compression != expectedCompression) {
        errorMessage = wrongCompressionMessage;
        return false;
    }
    return true;
}

static bool
parse_tiff_compression_options(const std::map<std::string, std::string>& attributes,
                               TiffSaveOptions& options,
                               std::string& errorMessage)
{
#if defined(TIFFTAG_JPEGQUALITY) && defined(COMPRESSION_JPEG)
    if (!parse_tiff_compression_u32_option(attributes,
                                          options.compression,
                                          COMPRESSION_JPEG,
                                          { "tiff:jpegQuality", "tiff:jpeg_quality", "jpeg:quality", "jpg:quality" },
                                          1u,
                                          100u,
                                          options.hasJpegQuality,
                                          options.jpegQuality,
                                          "invalid TIFF JPEG quality value",
                                          "TIFF JPEG quality requires JPEG compression",
                                          errorMessage)) {
        return false;
    }
#else
    if (find_attribute_value(attributes, { "tiff:jpegQuality", "tiff:jpeg_quality", "jpeg:quality", "jpg:quality" })) {
        errorMessage = "TIFF JPEG quality is not available in this libtiff build";
        return false;
    }
#endif

    const std::string* zipQuality = find_attribute_value(
        attributes, { "tiff:zipQuality", "tiff:zip_quality", "tiff:zipLevel", "tiff:zip_level",
                      "tiff:deflateLevel", "tiff:deflate_level" });
    if (zipQuality) {
#if defined(TIFFTAG_ZIPQUALITY)
        if (!parse_u32_attribute(attributes,
                                 { "tiff:zipQuality", "tiff:zip_quality", "tiff:zipLevel", "tiff:zip_level",
                                   "tiff:deflateLevel", "tiff:deflate_level" },
                                 1u,
                                 9u,
                                 options.hasZipQuality,
                                 options.zipQuality,
                                 "invalid TIFF Deflate level value",
                                 errorMessage)) {
            return false;
        }
        if (options.compression != COMPRESSION_DEFLATE
#if defined(COMPRESSION_ADOBE_DEFLATE)
            && options.compression != COMPRESSION_ADOBE_DEFLATE
#endif
        ) {
            errorMessage = "TIFF Deflate level requires Deflate compression";
            return false;
        }
#else
        (void)zipQuality;
        errorMessage = "TIFF Deflate level is not available in this libtiff build";
        return false;
#endif
    }

#if defined(TIFFTAG_ZSTD_LEVEL) && defined(COMPRESSION_ZSTD)
    if (!parse_tiff_compression_u32_option(attributes,
                                          options.compression,
                                          COMPRESSION_ZSTD,
                                          { "tiff:zstdLevel", "tiff:zstd_level" },
                                          1u,
                                          22u,
                                          options.hasZstdLevel,
                                          options.zstdLevel,
                                          "invalid TIFF ZSTD level value",
                                          "TIFF ZSTD level requires ZSTD compression",
                                          errorMessage)) {
        return false;
    }
#else
    if (find_attribute_value(attributes, { "tiff:zstdLevel", "tiff:zstd_level" })) {
        errorMessage = "TIFF ZSTD level is not available in this libtiff build";
        return false;
    }
#endif

#if defined(TIFFTAG_LZMAPRESET) && defined(COMPRESSION_LZMA)
    if (!parse_tiff_compression_u32_option(attributes,
                                          options.compression,
                                          COMPRESSION_LZMA,
                                          { "tiff:lzmaPreset", "tiff:lzma_preset" },
                                          0u,
                                          9u,
                                          options.hasLzmaPreset,
                                          options.lzmaPreset,
                                          "invalid TIFF LZMA preset value",
                                          "TIFF LZMA preset requires LZMA compression",
                                          errorMessage)) {
        return false;
    }
#else
    if (find_attribute_value(attributes, { "tiff:lzmaPreset", "tiff:lzma_preset" })) {
        errorMessage = "TIFF LZMA preset is not available in this libtiff build";
        return false;
    }
#endif

#if defined(TIFFTAG_WEBP_LEVEL) && defined(COMPRESSION_WEBP)
    if (!parse_tiff_compression_u32_option(attributes,
                                          options.compression,
                                          COMPRESSION_WEBP,
                                          { "tiff:webpLevel", "tiff:webp_level" },
                                          0u,
                                          100u,
                                          options.hasWebpLevel,
                                          options.webpLevel,
                                          "invalid TIFF WebP level value",
                                          "TIFF WebP level requires WebP compression",
                                          errorMessage)) {
        return false;
    }
#else
    if (find_attribute_value(attributes, { "tiff:webpLevel", "tiff:webp_level" })) {
        errorMessage = "TIFF WebP level is not available in this libtiff build";
        return false;
    }
#endif

#if defined(TIFFTAG_WEBP_LOSSLESS) && defined(COMPRESSION_WEBP)
    if (parse_bool_attribute(attributes, { "tiff:webpLossless", "tiff:webp_lossless" }, options.webpLossless)) {
        options.hasWebpLossless = true;
        if (options.compression != COMPRESSION_WEBP) {
            errorMessage = "TIFF WebP lossless mode requires WebP compression";
            return false;
        }
    }
#else
    if (find_attribute_value(attributes, { "tiff:webpLossless", "tiff:webp_lossless" })) {
        errorMessage = "TIFF WebP lossless mode is not available in this libtiff build";
        return false;
    }
#endif

#if defined(TIFFTAG_WEBP_LOSSLESS_EXACT) && defined(COMPRESSION_WEBP)
    if (parse_bool_attribute(attributes, { "tiff:webpLosslessExact", "tiff:webp_lossless_exact" }, options.webpLosslessExact)) {
        options.hasWebpLosslessExact = true;
        if (options.compression != COMPRESSION_WEBP) {
            errorMessage = "TIFF WebP exact lossless mode requires WebP compression";
            return false;
        }
    }
#else
    if (find_attribute_value(attributes, { "tiff:webpLosslessExact", "tiff:webp_lossless_exact" })) {
        errorMessage = "TIFF WebP exact lossless mode is not available in this libtiff build";
        return false;
    }
#endif

    return true;
}

static bool
set_tiff_compression_options(TIFF* tif, const TiffSaveOptions& options, std::string& errorMessage)
{
#if defined(TIFFTAG_JPEGQUALITY) && defined(COMPRESSION_JPEG)
    if (options.hasJpegQuality
        && TIFFSetField(tif, TIFFTAG_JPEGQUALITY, static_cast<int>(options.jpegQuality)) != 1) {
        errorMessage = "failed to set TIFF JPEG quality";
        return false;
    }
#endif

#if defined(TIFFTAG_ZIPQUALITY)
    if (options.hasZipQuality && TIFFSetField(tif, TIFFTAG_ZIPQUALITY, static_cast<int>(options.zipQuality)) != 1) {
        errorMessage = "failed to set TIFF Deflate level";
        return false;
    }
#endif

#if defined(TIFFTAG_ZSTD_LEVEL) && defined(COMPRESSION_ZSTD)
    if (options.hasZstdLevel && TIFFSetField(tif, TIFFTAG_ZSTD_LEVEL, static_cast<int>(options.zstdLevel)) != 1) {
        errorMessage = "failed to set TIFF ZSTD level";
        return false;
    }
#endif

#if defined(TIFFTAG_LZMAPRESET) && defined(COMPRESSION_LZMA)
    if (options.hasLzmaPreset && TIFFSetField(tif, TIFFTAG_LZMAPRESET, static_cast<int>(options.lzmaPreset)) != 1) {
        errorMessage = "failed to set TIFF LZMA preset";
        return false;
    }
#endif

#if defined(TIFFTAG_WEBP_LEVEL) && defined(COMPRESSION_WEBP)
    if (options.hasWebpLevel && TIFFSetField(tif, TIFFTAG_WEBP_LEVEL, static_cast<int>(options.webpLevel)) != 1) {
        errorMessage = "failed to set TIFF WebP level";
        return false;
    }
#endif

#if defined(TIFFTAG_WEBP_LOSSLESS) && defined(COMPRESSION_WEBP)
    if (options.hasWebpLossless && TIFFSetField(tif, TIFFTAG_WEBP_LOSSLESS, options.webpLossless ? 1 : 0) != 1) {
        errorMessage = "failed to set TIFF WebP lossless mode";
        return false;
    }
#endif

#if defined(TIFFTAG_WEBP_LOSSLESS_EXACT) && defined(COMPRESSION_WEBP)
    if (options.hasWebpLosslessExact
        && TIFFSetField(tif, TIFFTAG_WEBP_LOSSLESS_EXACT, options.webpLosslessExact ? 1 : 0) != 1) {
        errorMessage = "failed to set TIFF WebP exact lossless mode";
        return false;
    }
#endif

    return true;
}

static bool
parse_tiff_save_options(const std::map<std::string, std::string>& attributes,
                        const ImageComponentType componentType,
                        TiffSaveOptions& options,
                        std::string& errorMessage)
{
    options = TiffSaveOptions();

    if (!parse_tiff_compression(attributes, options.compression, errorMessage)) {
        return false;
    }
    if (!parse_tiff_predictor(attributes, componentType, options.compression, options.predictor, errorMessage)) {
        return false;
    }
    if (!parse_tiff_layout_options(attributes, options, errorMessage)) {
        return false;
    }
    if (!parse_tiff_compression_options(attributes, options, errorMessage)) {
        return false;
    }

    bool forceBigTiff = false;
    if (parse_bool_attribute(attributes, { "tiff:bigTiff", "tiff:bigtiff", "tiff:big_tiff" }, forceBigTiff)) {
        options.forceBigTiff = forceBigTiff;
    }
    options.unassociatedAlpha = has_unassociated_alpha_hint(attributes);
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
resolve_tiff_tile_layout(const TiffSaveOptions& options,
                         TIFF* tif,
                         bool& useTiles,
                         uint32_t& tileWidth,
                         uint32_t& tileLength,
                         std::string& errorMessage)
{
    useTiles = options.storageLayout == TiffStorageLayout::Tiles || options.hasTileWidth || options.hasTileLength;
    if (!useTiles) {
        tileWidth = 0u;
        tileLength = 0u;
        return true;
    }

    tileWidth = options.requestedTileWidth;
    tileLength = options.requestedTileLength;
    TIFFDefaultTileSize(tif, &tileWidth, &tileLength);

    if (tileWidth == 0u || tileLength == 0u) {
        errorMessage = "invalid TIFF tile dimensions";
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
copy_tiff_interleaved_samples(const uint8_t* sourceBytes,
                              const size_t sourcePixelWidth,
                              const size_t copyPixelWidth,
                              const size_t copyPixelHeight,
                              const size_t destinationPixelWidth,
                              const size_t destinationOriginX,
                              const size_t destinationOriginY,
                              const uint16_t samplesPerPixel,
                              const uint16_t bitsPerSample,
                              const uint16_t photometric,
                              const ImageComponentType componentType,
                              std::byte* destinationBytes)
{
    if (componentType == ImageComponentType::U8) {
        const uint8_t* source = reinterpret_cast<const uint8_t*>(sourceBytes);
        uint8_t* destination = reinterpret_cast<uint8_t*>(destinationBytes);
        for (size_t localY = 0; localY < copyPixelHeight; ++localY) {
            for (size_t localX = 0; localX < copyPixelWidth; ++localX) {
                const size_t sourcePixelIndex = (localY * sourcePixelWidth + localX) * static_cast<size_t>(samplesPerPixel);
                const size_t destinationPixelIndex =
                    ((destinationOriginY + localY) * destinationPixelWidth + destinationOriginX + localX)
                    * static_cast<size_t>(samplesPerPixel);
                for (size_t sampleOffset = 0; sampleOffset < static_cast<size_t>(samplesPerPixel); ++sampleOffset) {
                    uint8_t value = source[sourcePixelIndex + sampleOffset];
                    if (photometric == PHOTOMETRIC_MINISWHITE) {
                        value = static_cast<uint8_t>(255u - value);
                    }
                    destination[destinationPixelIndex + sampleOffset] = value;
                }
            }
        }
        return;
    }

    if (componentType == ImageComponentType::U16) {
        const uint16_t* source = reinterpret_cast<const uint16_t*>(sourceBytes);
        uint16_t* destination = reinterpret_cast<uint16_t*>(destinationBytes);
        for (size_t localY = 0; localY < copyPixelHeight; ++localY) {
            for (size_t localX = 0; localX < copyPixelWidth; ++localX) {
                const size_t sourcePixelIndex = (localY * sourcePixelWidth + localX) * static_cast<size_t>(samplesPerPixel);
                const size_t destinationPixelIndex =
                    ((destinationOriginY + localY) * destinationPixelWidth + destinationOriginX + localX)
                    * static_cast<size_t>(samplesPerPixel);
                for (size_t sampleOffset = 0; sampleOffset < static_cast<size_t>(samplesPerPixel); ++sampleOffset) {
                    uint16_t value = source[sourcePixelIndex + sampleOffset];
                    if (photometric == PHOTOMETRIC_MINISWHITE) {
                        value = static_cast<uint16_t>(65535u - value);
                    }
                    destination[destinationPixelIndex + sampleOffset] = value;
                }
            }
        }
        return;
    }

    if (componentType == ImageComponentType::U32) {
        const uint32_t* source = reinterpret_cast<const uint32_t*>(sourceBytes);
        uint32_t* destination = reinterpret_cast<uint32_t*>(destinationBytes);
        for (size_t localY = 0; localY < copyPixelHeight; ++localY) {
            for (size_t localX = 0; localX < copyPixelWidth; ++localX) {
                const size_t sourcePixelIndex = (localY * sourcePixelWidth + localX) * static_cast<size_t>(samplesPerPixel);
                const size_t destinationPixelIndex =
                    ((destinationOriginY + localY) * destinationPixelWidth + destinationOriginX + localX)
                    * static_cast<size_t>(samplesPerPixel);
                for (size_t sampleOffset = 0; sampleOffset < static_cast<size_t>(samplesPerPixel); ++sampleOffset) {
                    uint32_t value = source[sourcePixelIndex + sampleOffset];
                    if (photometric == PHOTOMETRIC_MINISWHITE) {
                        value = 0xffffffffu - value;
                    }
                    destination[destinationPixelIndex + sampleOffset] = value;
                }
            }
        }
        return;
    }

    if (componentType == ImageComponentType::F32 && bitsPerSample == 32u) {
        const float* source = reinterpret_cast<const float*>(sourceBytes);
        float* destination = reinterpret_cast<float*>(destinationBytes);
        for (size_t localY = 0; localY < copyPixelHeight; ++localY) {
            for (size_t localX = 0; localX < copyPixelWidth; ++localX) {
                const size_t sourcePixelIndex = (localY * sourcePixelWidth + localX) * static_cast<size_t>(samplesPerPixel);
                const size_t destinationPixelIndex =
                    ((destinationOriginY + localY) * destinationPixelWidth + destinationOriginX + localX)
                    * static_cast<size_t>(samplesPerPixel);
                for (size_t sampleOffset = 0; sampleOffset < static_cast<size_t>(samplesPerPixel); ++sampleOffset) {
                    float value = source[sourcePixelIndex + sampleOffset];
                    if (photometric == PHOTOMETRIC_MINISWHITE) {
                        value = 1.0f - value;
                    }
                    destination[destinationPixelIndex + sampleOffset] = value;
                }
            }
        }
    }
}
#endif

}  // namespace

DecodedImageData
decode_tiff_file(const std::string& path, const std::map<std::string, std::string>& attributes)
{
    DecodedImageData result;

#if !defined(RAWGL_HAS_LIBTIFF)
    (void)path;
    (void)attributes;
    result.errorMessage = "libtiff support is not available";
    return result;
#else
    TIFF* tif = TIFFOpen(path.c_str(), "r");
    if (tif == nullptr) {
        result.errorMessage = "can't open TIFF file";
        return result;
    }

    uint32_t directoryIndex = 0u;
    bool hasDirectoryIndex = false;
    std::string errorMessage;
    if (!parse_u32_attribute(attributes,
                             { "tiff:directoryIndex", "tiff:directory_index", "tiff:subimage" },
                             0u,
                             65535u,
                             hasDirectoryIndex,
                             directoryIndex,
                             "invalid TIFF directory index",
                             errorMessage)) {
        TIFFClose(tif);
        result.errorMessage = errorMessage;
        return result;
    }
    if (hasDirectoryIndex && TIFFSetDirectory(tif, static_cast<tdir_t>(directoryIndex)) != 1) {
        TIFFClose(tif);
        result.errorMessage = "requested TIFF directory was not found";
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
    if (!resolve_tiff_decode_layout(samplesPerPixel, photometric, planarConfig, alphaChannel, errorMessage)) {
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
    if (TIFFIsTiled(tif) != 0) {
        uint32_t tileWidth = 0u;
        uint32_t tileLength = 0u;
        TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tileWidth);
        TIFFGetField(tif, TIFFTAG_TILELENGTH, &tileLength);
        if (tileWidth == 0u || tileLength == 0u) {
            TIFFClose(tif);
            result.errorMessage = "invalid TIFF tile geometry";
            return result;
        }

        const tsize_t tileSize = TIFFTileSize(tif);
        const tsize_t tileRowSize = TIFFTileRowSize(tif);
        if (tileSize <= 0) {
            TIFFClose(tif);
            result.errorMessage = "invalid TIFF tile size";
            return result;
        }
        if (tileRowSize <= 0) {
            TIFFClose(tif);
            result.errorMessage = "invalid TIFF tile row size";
            return result;
        }

        std::vector<uint8_t> tileBuffer(static_cast<size_t>(tileSize));
        for (uint32_t tileY = 0u; tileY < height; tileY += tileLength) {
            for (uint32_t tileX = 0u; tileX < width; tileX += tileWidth) {
                const ttile_t tileIndex = TIFFComputeTile(tif, tileX, tileY, 0u, 0u);
                if (TIFFReadEncodedTile(tif, tileIndex, tileBuffer.data(), tileSize) < 0) {
                    TIFFClose(tif);
                    result = DecodedImageData();
                    result.errorMessage = "can't read TIFF tile";
                    return result;
                }

                const size_t copyWidth = std::min(static_cast<size_t>(tileWidth), static_cast<size_t>(width - tileX));
                const size_t copyHeight =
                    std::min(static_cast<size_t>(tileLength), static_cast<size_t>(height - tileY));
                const size_t tilePixelsPerRow = static_cast<size_t>(tileRowSize)
                                                / (static_cast<size_t>(samplesPerPixel) * bytesPerComponent);
                copy_tiff_interleaved_samples(tileBuffer.data(),
                                              tilePixelsPerRow,
                                              copyWidth,
                                              copyHeight,
                                              static_cast<size_t>(width),
                                              static_cast<size_t>(tileX),
                                              static_cast<size_t>(tileY),
                                              samplesPerPixel,
                                              bitsPerSample,
                                              photometric,
                                              componentType,
                                              result.bytes.data());
            }
        }
    } else {
        for (uint32_t row = 0u; row < height; ++row) {
            if (TIFFReadScanline(tif, rowBuffer.data(), row, 0) != 1) {
                TIFFClose(tif);
                result = DecodedImageData();
                result.errorMessage = "can't read TIFF scanline";
                return result;
            }

            copy_tiff_interleaved_samples(rowBuffer.data(),
                                          static_cast<size_t>(width),
                                          static_cast<size_t>(width),
                                          1u,
                                          static_cast<size_t>(width),
                                          0u,
                                          static_cast<size_t>(row),
                                          samplesPerPixel,
                                          bitsPerSample,
                                          photometric,
                                          componentType,
                                          result.bytes.data());
        }
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
    TiffSaveOptions options;
    if (!parse_tiff_save_options(attributes, settings.componentType, options, errorMessage)) {
        return false;
    }

    bool useTiles = false;
    uint32_t tileWidth = 0u;
    uint32_t tileLength = 0u;
    uint32_t rowsPerStrip = 0u;
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

    const char* openMode = options.forceBigTiff ? "w8" : "w";
    TIFF* tif = TIFFOpen(path.c_str(), openMode);
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
    TIFFSetField(tif, TIFFTAG_COMPRESSION, options.compression);

    if (!set_tiff_compression_options(tif, options, errorMessage)) {
        TIFFClose(tif);
        return false;
    }

    if (!resolve_tiff_tile_layout(options, tif, useTiles, tileWidth, tileLength, errorMessage)) {
        TIFFClose(tif);
        return false;
    }
    if (!resolve_tiff_strip_layout(options, tif, useTiles, rowsPerStrip, errorMessage)) {
        TIFFClose(tif);
        return false;
    }

    if (useTiles) {
        TIFFSetField(tif, TIFFTAG_TILEWIDTH, tileWidth);
        TIFFSetField(tif, TIFFTAG_TILELENGTH, tileLength);
    } else {
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, rowsPerStrip);
    }

    if (options.predictor != PREDICTOR_NONE) {
        TIFFSetField(tif, TIFFTAG_PREDICTOR, options.predictor);
    }

    if (resolvedAlphaChannel >= 0) {
        const uint16_t extraSample = options.unassociatedAlpha ? EXTRASAMPLE_UNASSALPHA : EXTRASAMPLE_ASSOCALPHA;
        TIFFSetField(tif, TIFFTAG_EXTRASAMPLES, 1, &extraSample);
    }

    const size_t rowBytes = static_cast<size_t>(image.width) * static_cast<size_t>(outputChannels)
                            * byte_size_for_image_component(settings.componentType);

    if (useTiles) {
        const tsize_t tileByteCountSigned = TIFFTileSize(tif);
        const tsize_t tileRowBytesSigned = TIFFTileRowSize(tif);
        if (tileByteCountSigned <= 0 || tileRowBytesSigned <= 0) {
            TIFFClose(tif);
            errorMessage = "invalid TIFF tile buffer geometry";
            return false;
        }

        const size_t tileByteCount = static_cast<size_t>(tileByteCountSigned);
        const size_t tileRowBytes = static_cast<size_t>(tileRowBytesSigned);
        std::vector<std::byte> tileBytes(tileByteCount);

        for (uint32_t tileY = 0u; tileY < static_cast<uint32_t>(image.height); tileY += tileLength) {
            for (uint32_t tileX = 0u; tileX < static_cast<uint32_t>(image.width); tileX += tileWidth) {
                std::fill(tileBytes.begin(), tileBytes.end(), std::byte { 0 });

                const size_t copyWidth =
                    std::min(static_cast<size_t>(tileWidth), static_cast<size_t>(image.width) - static_cast<size_t>(tileX));
                const size_t copyHeight = std::min(
                    static_cast<size_t>(tileLength), static_cast<size_t>(image.height) - static_cast<size_t>(tileY));

                for (size_t localY = 0; localY < copyHeight; ++localY) {
                    const std::byte* sourceRow =
                        encodedBytes.data() + (static_cast<size_t>(tileY) + localY) * rowBytes
                        + static_cast<size_t>(tileX) * static_cast<size_t>(outputChannels)
                              * byte_size_for_image_component(settings.componentType);
                    std::byte* destinationRow = tileBytes.data() + localY * tileRowBytes;
                    std::memcpy(destinationRow,
                                sourceRow,
                                copyWidth * static_cast<size_t>(outputChannels)
                                    * byte_size_for_image_component(settings.componentType));
                }

                const ttile_t tileIndex = TIFFComputeTile(tif, tileX, tileY, 0u, 0u);
                if (TIFFWriteEncodedTile(tif, tileIndex, tileBytes.data(), tileByteCountSigned) < 0) {
                    TIFFClose(tif);
                    errorMessage = "can't write TIFF tile";
                    return false;
                }
            }
        }
    } else {
        for (int row = 0; row < image.height; ++row) {
            std::byte* rowData = encodedBytes.data() + static_cast<size_t>(row) * rowBytes;
            if (TIFFWriteScanline(tif, rowData, static_cast<uint32_t>(row), 0) != 1) {
                TIFFClose(tif);
                errorMessage = "can't write TIFF scanline";
                return false;
            }
        }
    }

    TIFFClose(tif);
    return true;
#endif
}

}  // namespace rawgl::io
