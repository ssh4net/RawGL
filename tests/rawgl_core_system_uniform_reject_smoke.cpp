// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl.h"

#include <iostream>

int
main()
{
    rawgl::Pass pass;
    pass.programKind            = rawgl::ShaderProgramKind::compute;
    rawgl::ShaderModuleDefinition module;
    module.role       = rawgl::ShaderModuleRole::compute;
    module.sourceKind = rawgl::ShaderModuleSourceKind::filePath;
    module.path       = "tests/shaders/system_uniforms.comp";
    pass.shaderModules.push_back(std::move(module));
    pass.sizeX                  = 1;
    pass.sizeY                  = 1;
    pass.workGroupSizeX         = 1;
    pass.workGroupSizeY         = 1;
    pass.hasExplicitWorkGroupSize = true;
    rawgl::InputBinding input;
    input.name        = "iTime";
    input.sourceKind  = rawgl::InputSourceKind::floatValues;
    input.floatValues = { 10.0f };
    pass.inputs.push_back(std::move(input));
    rawgl::OutputBinding output;
    output.name         = "o_out0";
    output.path         = "tests/outputs/rawgl_core_system_uniform_reject_smoke.exr";
    output.format       = "rgba32f";
    output.channels     = 4;
    output.alphaChannel = 3;
    output.bits         = 16;
    pass.outputs.push_back(std::move(output));

    rawgl::Workflow workflow;
    workflow.verbosity = 0;
    workflow.passes.push_back(std::move(pass));

    rawgl::Session session;
    rawgl::PrepareResult prepareResult = session.prepare(workflow);
    if (prepareResult.success) {
        std::cerr << "Expected workflow preparation to reject user-provided system uniform input." << std::endl;
        return 1;
    }
    if (prepareResult.errorMessage.find("system uniform is engine controlled") == std::string::npos) {
        std::cerr << "Unexpected workflow preparation error: " << prepareResult.errorMessage << std::endl;
        return 1;
    }

    return 0;
}
