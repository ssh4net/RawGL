// SPDX-License-Identifier: Apache-2.0
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

    workflow.passes.push_back(pass);

    rawgl::Session session;
    rawgl::io::IoRuntime ioRuntime;
    std::vector<rawgl::io::FileInputBinding> fileInputs;
    fileInputs.push_back(rawgl::io::FileTextureInput(0, "u_src0", inputPath.string()));
    std::vector<rawgl::io::FileOutputBinding> fileOutputs;
    fileOutputs.push_back(rawgl::io::FileOutput(0, "out_color", outputPath.string()));

    rawgl::io::PrepareWorkflowResult prepareResult = ioRuntime.prepare(session, workflow, fileInputs, fileOutputs);
    if (!prepareResult.success || !prepareResult.workflow) {
        std::cerr << "IO workflow prepare failed: " << prepareResult.errorMessage << std::endl;
        return 1;
    }

    rawgl::io::RunRequest runRequest;
    runRequest.fileInputs.push_back(rawgl::io::FileTextureOverride(0, "u_src0", inputPath.string()));

    const rawgl::RunResult preparedRunResult = prepareResult.workflow->run(runRequest);
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

    const rawgl::RunResult directRunResult = ioRuntime.run(session, workflow, {}, fileInputs, fileOutputs);
    if (!directRunResult.success) {
        std::cerr << "Direct IO workflow run failed: " << directRunResult.errorMessage << std::endl;
        return 1;
    }

    return 0;
}
