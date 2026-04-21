// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "rawgl/rawgl_io.h"
#include "output_writer.h"
#include "texture_loader.h"

namespace rawgl::io {

class IoRuntimeService {
public:
    explicit IoRuntimeService(const IoRuntimeOptions& options = {});

    const IoRuntimeOptions& options() const { return m_options; }

    LoadedTextureData loadTextureFileData(const std::string& path,
                                          const std::map<std::string, std::string>& attributes) const;

    HostImageData loadHostImageData(const std::string& path,
                                    const std::map<std::string, std::string>& attributes) const;

    void saveImageOutput(const OutputWriteRequest& request) const;

private:
    IoRuntimeOptions m_options;
};

}  // namespace rawgl::io
