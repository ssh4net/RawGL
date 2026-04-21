// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_io.h"

#include <filesystem>
#include <iostream>

int
main()
{
    const std::filesystem::path inputPath = std::filesystem::absolute("tests/inputs/EmptyPresetLUT.png");
    const std::filesystem::path outputPath = std::filesystem::absolute("tests/outputs/rawgl_io_workflow_smoke.png");

    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    rawgl::Workflow workflow;
    workflow.verbosity = 3;

    rawgl::Pass pass;
    pass.programKind = rawgl::ShaderProgramKind::vertfrag;
    pass.sizeX = 64;
    pass.sizeY = 64;

    rawgl::ShaderModuleDefinition fragmentModule;
    fragmentModule.role = rawgl::ShaderModuleRole::fragment;
    fragmentModule.sourceKind = rawgl::ShaderModuleSourceKind::glslText;
    fragmentModule.glslText = R"(#version 450 core
layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 out_color;
uniform sampler2D u_src0;
void main()
{
    out_color = texture(u_src0, UV);
}
)";
    fragmentModule.debugLabel = "rawgl_io_workflow_smoke_fragment";
    pass.shaderModules.push_back(fragmentModule);

    rawgl::InputBinding input;
    input.name        = "u_src0";
    input.sourceKind  = rawgl::InputSourceKind::textureFile;
    input.texturePath = inputPath.string();
    pass.inputs.push_back(input);

    rawgl::OutputBinding output;
    output.name = "out_color";
    output.path = outputPath.string();
    pass.outputs.push_back(output);

    workflow.passes.push_back(pass);

    rawgl::Session session;
    rawgl::io::IoRuntime ioRuntime;

    rawgl::io::PrepareWorkflowResult prepareResult = ioRuntime.prepare(session, workflow);
    if (!prepareResult.success || !prepareResult.workflow) {
        std::cerr << "IO workflow prepare failed: " << prepareResult.errorMessage << std::endl;
        return 1;
    }

    rawgl::RunSettings settings;
    rawgl::InputOverride overrideInput;
    overrideInput.passIndex = 0;
    overrideInput.name = "u_src0";
    overrideInput.sourceKind = rawgl::InputSourceKind::textureFile;
    overrideInput.texturePath = inputPath.string();
    settings.overrides.push_back(overrideInput);

    const rawgl::RunResult preparedRunResult = prepareResult.workflow->run(settings);
    if (!preparedRunResult.success) {
        std::cerr << "Prepared IO workflow run failed: " << preparedRunResult.errorMessage << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(outputPath)) {
        std::cerr << "Prepared IO workflow did not write output: " << outputPath << std::endl;
        return 1;
    }

    if (preparedRunResult.capturedOutputs.find("out_color::0") == preparedRunResult.capturedOutputs.end()) {
        std::cerr << "Prepared IO workflow did not preserve captured output." << std::endl;
        return 1;
    }

    const rawgl::RunResult directRunResult = ioRuntime.run(session, workflow);
    if (!directRunResult.success) {
        std::cerr << "Direct IO workflow run failed: " << directRunResult.errorMessage << std::endl;
        return 1;
    }

    return 0;
}
