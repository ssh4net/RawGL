// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "rawgl/rawgl_core.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace rawgl::io {

enum class ImageCodecFamily : uint8_t {
    Unknown,
    Bmp,
    Png,
    Jpeg,
    Tga,
    Exr,
    Hdr,
    Tiff,
    Jpeg2000,
    JpegXl,
    CameraRaw,
    Webp,
};

enum class ImageComponentType : uint8_t {
    Unknown,
    U8,
    U16,
    U32,
    F16,
    F32,
    F64,
};

struct DecodedImageData {
    bool success = false;
    std::string errorMessage;
    int width = 0;
    int height = 0;
    int channels = 0;
    int alphaChannel = -1;
    ImageComponentType componentType = ImageComponentType::Unknown;
    std::vector<std::byte> bytes;
};

struct ImageEncodeSettings {
    ImageCodecFamily codec = ImageCodecFamily::Unknown;
    ImageComponentType componentType = ImageComponentType::Unknown;
    bool defaulted = false;
};

ImageCodecFamily
get_image_codec_family(const std::string& path);

size_t
byte_size_for_image_component(const ImageComponentType componentType);

DecodedImageData
decode_image_file(const std::string& path, const std::map<std::string, std::string>& attributes);

ImageEncodeSettings
resolve_image_encode_settings(const std::string& path, int bits);

bool
encode_image_file(const std::string& path,
                  const std::map<std::string, std::string>& attributes,
                  int alphaChannel,
                  const HostImageData& image,
                  const ImageEncodeSettings& settings,
                  std::string& errorMessage);

}  // namespace rawgl::io
