// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_io.h"

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

    const char* requiredCodecs[] = { "jpeg", "png", "tiff", "openexr", "openimageio" };
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
    if (jpeg->nativeRead && !contains_string(jpeg->nativeReadOptions, "jpeg:color_transform")) {
        std::cerr << "Native JPEG reader did not report color transform options." << std::endl;
        return 1;
    }

    const rawgl::io::ImageCodecCapabilities* tiff = find_codec(capabilities, "tiff");
    if (tiff->nativeWrite && !contains_string(tiff->nativeWriteCompressionModes, "none")) {
        std::cerr << "Native TIFF writer did not report uncompressed output." << std::endl;
        return 1;
    }
    if (tiff->nativeWrite && !contains_string(tiff->nativeWriteOptions, "tiff:compression")) {
        std::cerr << "Native TIFF writer did not report compression options." << std::endl;
        return 1;
    }
    if (tiff->nativeRead && !contains_string(tiff->nativeReadOptions, "tiff:directory_index")) {
        std::cerr << "Native TIFF reader did not report directory options." << std::endl;
        return 1;
    }

    const rawgl::io::ImageCodecCapabilities* openexr = find_codec(capabilities, "openexr");
    if (openexr->nativeWrite && openexr->nativeWriteCompressionModes.empty()) {
        std::cerr << "Native OpenEXR writer did not report compression modes." << std::endl;
        return 1;
    }
    if (openexr->nativeWrite && !contains_string(openexr->nativeWriteOptions, "openexr:compression")) {
        std::cerr << "Native OpenEXR writer did not report compression options." << std::endl;
        return 1;
    }
    if (openexr->nativeRead && !contains_string(openexr->nativeReadOptions, "openexr:channel_selection")) {
        std::cerr << "Native OpenEXR reader did not report channel selection options." << std::endl;
        return 1;
    }

    const rawgl::io::ImageCodecCapabilities* openImageIo = find_codec(capabilities, "openimageio");
    if (!openImageIo->fallbackRead || !openImageIo->fallbackWrite || !has_detail(*openImageIo, "openimageio.version")) {
        std::cerr << "OpenImageIO fallback capability entry is incomplete." << std::endl;
        return 1;
    }

    return 0;
}
