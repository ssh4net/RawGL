// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "rawgl/rawgl_core.h"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace rawgl::io {

struct LoadedTextureData {
    bool valid = false;
    int width = 0;
    int height = 0;
    int channels = 0;
    int alphaChannel = -1;
    unsigned int internalFormat = 0;
    unsigned int type = 0;
    std::vector<std::byte> bytes;
};

LoadedTextureData
load_texture_file_data(const std::string& path, const std::map<std::string, std::string>& attributes);

HostImageData
load_host_image_data(const std::string& path, const std::map<std::string, std::string>& attributes);

}  // namespace rawgl::io
