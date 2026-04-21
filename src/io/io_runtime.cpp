// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "io_runtime.h"

namespace rawgl::io {

IoRuntimeService::IoRuntimeService(const IoRuntimeOptions& options)
    : m_options(options)
{
}

LoadedTextureData
IoRuntimeService::loadTextureFileData(const std::string& path,
                                      const std::map<std::string, std::string>& attributes) const
{
    return rawgl::io::load_texture_file_data(path, attributes);
}

HostImageData
IoRuntimeService::loadHostImageData(const std::string& path,
                                    const std::map<std::string, std::string>& attributes) const
{
    return rawgl::io::load_host_image_data(path, attributes);
}

void
IoRuntimeService::saveImageOutput(const OutputWriteRequest& request) const
{
    rawgl::io::save_image_output(request);
}

}  // namespace rawgl::io
