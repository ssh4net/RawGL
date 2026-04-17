// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "rawgl/rawgl_cli.h"
#include "rawgl/rawgl.h"

namespace rawgl {

struct ShaderInterfaceInspector {
    const void* userData = nullptr;
    ShaderInterface (*inspect)(const void* userData,
                               ShaderProgramKind kind,
                               const std::vector<std::string>& paths) = nullptr;
};

Workflow
BuildWorkflowFromCommandLine(const CommandLineRequest& request, const ShaderInterfaceInspector& inspectShaderInterface);

}  // namespace rawgl
