// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "rawgl/rawgl_core.h"

#include <map>
#include <string>

namespace rawgl::io {

struct OutputWriteRequest {
    std::string path;
    std::map<std::string, std::string> attributes;
    int alphaChannel = -1;
    int bits = 16;
    const HostImageData* image = nullptr;
};

void
save_image_output(const OutputWriteRequest& request);

}  // namespace rawgl::io
