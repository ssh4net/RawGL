// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "rawgl/rawgl_cli.h"
#include "rawgl/rawgl.h"
#include "rawgl/rawgl_io.h"

namespace rawgl {

struct ShaderInterfaceInspector {
    const void* userData = nullptr;
    ShaderInterface (*inspect)(const void* userData,
                               ShaderProgramKind kind,
                               const std::vector<std::string>& paths) = nullptr;
};

struct CliWorkflow {
    Workflow workflow;
    std::vector<io::FileInputBinding> fileInputs;
    std::vector<io::FileOutputBinding> fileOutputs;
};

Workflow
BuildWorkflowFromCommandLine(const CommandLineRequest& request, const ShaderInterfaceInspector& inspectShaderInterface);

CliWorkflow
BuildCliWorkflowFromCommandLine(const CommandLineRequest& request, const ShaderInterfaceInspector& inspectShaderInterface);

}  // namespace rawgl
