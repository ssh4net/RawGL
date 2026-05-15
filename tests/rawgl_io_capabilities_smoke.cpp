// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_io.h"

#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>

static const rawgl::io::ImageCodecCapabilities*
find_codec(const rawgl::io::ImageIoCapabilities& capabilities, const char* name)
{
    for (const rawgl::io::ImageCodecCapabilities& codec : capabilities.codecs) {
        if (codec.name == name) {
            return &codec;
        }
    }
    return nullptr;
}

static bool
contains_string(const std::vector<std::string>& values, const char* expected)
{
    for (const std::string& value : values) {
        if (value == expected) {
            return true;
        }
    }
    return false;
}

static bool
has_detail(const rawgl::io::ImageCodecCapabilities& codec, const char* name)
{
    for (const rawgl::io::ImageIoCapabilityDetail& detail : codec.details) {
        if (detail.name == name) {
            return true;
        }
    }
    return false;
}

static bool
expect_strings(const std::vector<std::string>& values,
               std::initializer_list<const char*> expectedValues,
               const char* context)
{
    for (const char* expectedValue : expectedValues) {
        if (!contains_string(values, expectedValue)) {
            std::cerr << context << " is missing: " << expectedValue << std::endl;
            return false;
        }
    }
    return true;
}

static bool
expect_codec(const rawgl::io::ImageIoCapabilities& capabilities, const char* name)
{
    const rawgl::io::ImageCodecCapabilities* codec = find_codec(capabilities, name);
    if (codec == nullptr) {
        std::cerr << "Missing codec capability entry: " << name << std::endl;
        return false;
    }
    if (codec->extensions.empty()) {
        std::cerr << "Codec capability entry has no extensions: " << name << std::endl;
        return false;
    }
    return true;
}

int
main()
{
    const rawgl::io::ImageIoCapabilities capabilities = rawgl::io::GetImageIoCapabilities();
    if (!capabilities.openImageIoFallback) {
        std::cerr << "OpenImageIO fallback was not reported." << std::endl;
        return 1;
    }
    if (capabilities.codecs.empty()) {
        std::cerr << "No image IO codec capabilities were reported." << std::endl;
        return 1;
    }

    const char* requiredCodecs[] = {
        "jpeg",
        "png",
        "tiff",
        "openexr",
        "jpeg2000",
        "jpegxl",
        "camera_raw",
        "openimageio",
    };
    for (const char* codecName : requiredCodecs) {
        if (!expect_codec(capabilities, codecName)) {
            return 1;
        }
    }

    const rawgl::io::ImageCodecCapabilities* jpeg = find_codec(capabilities, "jpeg");
    if (!jpeg->fallbackRead || !has_detail(*jpeg, "rawgl.high_bit_depth_jpeg")) {
        std::cerr << "JPEG capability entry is missing expected details." << std::endl;
        return 1;
    }
    if (jpeg->nativeWrite && !contains_string(jpeg->nativeWriteComponentTypes, "u8")) {
        std::cerr << "Native JPEG writer did not report u8 output." << std::endl;
        return 1;
    }
    if (jpeg->nativeRead &&
        !expect_strings(jpeg->nativeReadOptions, { "jpeg:color_transform" }, "Native JPEG reader options")) {
        std::cerr << "Native JPEG reader did not report color transform options." << std::endl;
        return 1;
    }
    if (jpeg->nativeWrite &&
        !expect_strings(jpeg->nativeWriteOptions,
                        { "jpeg:quality", "jpg:quality", "oiio:Compression",
                          "jpeg:progressive", "jpg:progressive",
                          "jpeg:optimize", "jpg:optimize",
                          "jpeg:subsampling", "jpg:subsampling" },
                        "Native JPEG writer options")) {
        return 1;
    }

    const rawgl::io::ImageCodecCapabilities* tiff = find_codec(capabilities, "tiff");
    if (tiff->nativeWrite && !contains_string(tiff->nativeWriteCompressionModes, "none")) {
        std::cerr << "Native TIFF writer did not report uncompressed output." << std::endl;
        return 1;
    }
    if (tiff->nativeWrite &&
        !expect_strings(tiff->nativeWriteOptions,
                        { "tiff:compression", "compression", "oiio:Compression",
                          "tiff:predictor",
                          "tiff:layout", "tiff:storageLayout", "tiff:storage_layout", "tiff:storage",
                          "tiff:tiled",
                          "tiff:tileWidth", "tiff:tile_width",
                          "tiff:tileLength", "tiff:tile_length",
                          "tiff:tileHeight", "tiff:tile_height",
                          "tiff:rowsPerStrip", "tiff:rows_per_strip",
                          "tiff:bigTiff", "tiff:bigtiff", "tiff:big_tiff",
                          "tiff:jpegQuality", "tiff:jpeg_quality", "jpeg:quality", "jpg:quality",
                          "tiff:zipQuality", "tiff:zip_quality",
                          "tiff:zipLevel", "tiff:zip_level",
                          "tiff:deflateLevel", "tiff:deflate_level",
                          "tiff:zstdLevel", "tiff:zstd_level",
                          "tiff:lzmaPreset", "tiff:lzma_preset",
                          "tiff:webpLevel", "tiff:webp_level",
                          "tiff:webpLossless", "tiff:webp_lossless",
                          "tiff:webpLosslessExact", "tiff:webp_lossless_exact",
                          "oiio:UnassociatedAlpha" },
                        "Native TIFF writer options")) {
        return 1;
    }
    if (tiff->nativeRead &&
        !expect_strings(tiff->nativeReadOptions,
                        { "rawgl:load_backend", "rawgl:decode_backend",
                          "tiff:directory_index", "tiff:directoryIndex", "tiff:subimage" },
                        "Native TIFF reader options")) {
        return 1;
    }

    const rawgl::io::ImageCodecCapabilities* openexr = find_codec(capabilities, "openexr");
    if (openexr->nativeWrite && openexr->nativeWriteCompressionModes.empty()) {
        std::cerr << "Native OpenEXR writer did not report compression modes." << std::endl;
        return 1;
    }
    if (openexr->nativeWrite &&
        !expect_strings(openexr->nativeWriteOptions,
                        { "openexr:compression", "compression", "oiio:Compression",
                          "openexr:layout", "openexr:storageLayout", "openexr:storage_layout",
                          "openexr:tiled",
                          "openexr:tileWidth", "openexr:tile_width",
                          "openexr:tileHeight", "openexr:tile_height",
                          "openexr:tileLength", "openexr:tile_length",
                          "openexr:lineOrder", "openexr:line_order",
                          "openexr:dwaCompressionLevel", "openexr:dwa_compression_level",
                          "openexr:attribute:string:<name>" },
                        "Native OpenEXR writer options")) {
        return 1;
    }
    if (openexr->nativeRead &&
        !expect_strings(openexr->nativeReadOptions,
                        { "rawgl:load_backend", "rawgl:decode_backend",
                          "openexr:channel_selection", "openexr:channelSelection" },
                        "Native OpenEXR reader options")) {
        return 1;
    }

    const rawgl::io::ImageCodecCapabilities* jpeg2000 = find_codec(capabilities, "jpeg2000");
    if (!contains_string(jpeg2000->extensions, "jp2") ||
        !contains_string(jpeg2000->extensions, "j2k") ||
        !contains_string(jpeg2000->extensions, "j2c") ||
        !contains_string(jpeg2000->extensions, "jpc") ||
        !has_detail(*jpeg2000, "openjpeg.enabled")) {
        std::cerr << "JPEG-2000 capability entry is missing expected details." << std::endl;
        return 1;
    }
    if (jpeg2000->nativeRead &&
        !expect_strings(jpeg2000->nativeReadOptions,
                        { "rawgl:load_backend", "rawgl:decode_backend",
                          "jpeg2000:reduce_factor", "jpeg2000:reduce",
                          "jpeg2000:layer_limit", "jpeg2000:layers" },
                        "Native JPEG-2000 reader options")) {
        return 1;
    }
    if (jpeg2000->nativeWrite &&
        (!expect_strings(jpeg2000->nativeWriteOptions,
                         { "jpeg2000:lossless",
                           "jpeg2000:compression_ratio", "jpeg2000:rate",
                           "jpeg2000:quality", "jpeg2000:psnr" },
                         "Native JPEG-2000 writer options") ||
         !expect_strings(jpeg2000->nativeWriteCompressionModes,
                         { "lossless", "rate", "quality" },
                         "Native JPEG-2000 writer compression modes"))) {
        return 1;
    }

    const rawgl::io::ImageCodecCapabilities* jpegxl = find_codec(capabilities, "jpegxl");
    if (!contains_string(jpegxl->extensions, "jxl") || !has_detail(*jpegxl, "libjxl.headers")) {
        std::cerr << "JPEG XL capability entry is missing expected details." << std::endl;
        return 1;
    }

    const rawgl::io::ImageCodecCapabilities* cameraRaw = find_codec(capabilities, "camera_raw");
    if (!contains_string(cameraRaw->extensions, "dng") || !has_detail(*cameraRaw, "libraw.headers")) {
        std::cerr << "Camera RAW capability entry is missing expected details." << std::endl;
        return 1;
    }

    const rawgl::io::ImageCodecCapabilities* openImageIo = find_codec(capabilities, "openimageio");
    if (!openImageIo->fallbackRead || !openImageIo->fallbackWrite || !has_detail(*openImageIo, "openimageio.version")) {
        std::cerr << "OpenImageIO fallback capability entry is incomplete." << std::endl;
        return 1;
    }

    return 0;
}
