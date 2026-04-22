// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "image_backend.h"

#include <map>
#include <string>

namespace rawgl::io {

DecodedImageData
decode_jpg_file(const std::string& path);

bool
encode_jpg_file(const std::string& path,
                const std::map<std::string, std::string>& attributes,
                int alphaChannel,
                const HostImageData& image,
                std::string& errorMessage);

}  // namespace rawgl::io
