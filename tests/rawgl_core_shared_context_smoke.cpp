// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl.h"

#include <iostream>
#include <cstring>

namespace {

rawgl::ShaderModuleDefinition
make_file_module(const char* path)
{
    rawgl::ShaderModuleDefinition module;
    module.role       = rawgl::ShaderModuleRole::compute;
    module.sourceKind = rawgl::ShaderModuleSourceKind::filePath;
    module.path       = path;
    return module;
}

bool
read_pixel_rgba32f(const rawgl::RunResult& runResult, float pixel[4])
{
    const auto captureIt = runResult.capturedOutputs.find("o_out0::0");
    if (captureIt == runResult.capturedOutputs.end()) {
        return false;
    }
    if (captureIt->second.bytes.size() < sizeof(pixel[0]) * 4u) {
        return false;
    }
    std::memcpy(pixel, captureIt->second.bytes.data(), sizeof(pixel[0]) * 4u);
    return true;
}

rawgl::Workflow
make_system_uniform_workflow()
{
    rawgl::Pass pass;
    pass.programKind              = rawgl::ShaderProgramKind::compute;
    pass.shaderModules.push_back(make_file_module("tests/shaders/system_uniforms.comp"));
    pass.sizeX                    = 1;
    pass.sizeY                    = 1;
    pass.workGroupSizeX           = 1;
    pass.workGroupSizeY           = 1;
    pass.hasExplicitWorkGroupSize = true;

    pass.outputs.push_back(rawgl::CapturedOutput("o_out0", "rgba32f", 4, 3, 16));

    rawgl::Workflow workflow;
    workflow.verbosity = 0;
    workflow.passes.push_back(std::move(pass));
    return workflow;
}

bool
verify_output(const rawgl::RunResult& runResult, float expectedTime, float expectedFrame, float expectedPassIndex)
{
    float pixel[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (!read_pixel_rgba32f(runResult, pixel)) {
        std::cerr << "Unable to read captured output image." << std::endl;
        return false;
    }

    if (pixel[0] != expectedTime || pixel[1] != expectedFrame || pixel[2] != expectedPassIndex || pixel[3] != 1.0f) {
        std::cerr << "Unexpected captured output pixel: [" << pixel[0] << ", " << pixel[1] << ", "
                  << pixel[2] << ", " << pixel[3] << "]" << std::endl;
        return false;
    }

    return true;
}

}  // namespace

int
main()
{
    rawgl::Session session;

    const rawgl::ShaderInterface inspectionA =
        session.inspectShaderInterface(rawgl::ShaderInspectionRequest {
            rawgl::ShaderProgramKind::compute,
            {},
            { make_file_module("tests/shaders/system_uniforms.comp") },
        });
    const rawgl::ShaderInterface inspectionB =
        session.inspectShaderInterface(rawgl::ShaderInspectionRequest {
            rawgl::ShaderProgramKind::compute,
            {},
            { make_file_module("tests/shaders/system_uniforms.comp") },
        });

    if (!inspectionA.success || !inspectionB.success) {
        std::cerr << "Shared-context inspection failed." << std::endl;
        return 1;
    }

    rawgl::PrepareResult workflowA = session.prepare(make_system_uniform_workflow());
    rawgl::PrepareResult workflowB = session.prepare(make_system_uniform_workflow());

    if (!workflowA.success || !workflowA.workflow) {
        std::cerr << "Shared-session workflow A preparation failed: " << workflowA.errorMessage << std::endl;
        return 1;
    }
    if (!workflowB.success || !workflowB.workflow) {
        std::cerr << "Shared-session workflow B preparation failed: " << workflowB.errorMessage << std::endl;
        return 1;
    }

    rawgl::RunSettings runSettingsA;
    runSettingsA.systemUniforms = rawgl::SystemUniformState { 2.0, 0.5, 7, 0 };
    rawgl::RunSettings runSettingsB;
    runSettingsB.systemUniforms = rawgl::SystemUniformState { 3.0, 0.25, 9, 0 };

    const rawgl::RunResult runA = workflowA.workflow->run(runSettingsA);
    const rawgl::RunResult runB = workflowB.workflow->run(runSettingsB);

    if (!runA.success) {
        std::cerr << "Shared-session workflow A execution failed: " << runA.errorMessage << std::endl;
        return 1;
    }
    if (!runB.success) {
        std::cerr << "Shared-session workflow B execution failed: " << runB.errorMessage << std::endl;
        return 1;
    }

    if (!verify_output(runA, 2.0f, 7.0f, 0.0f)) {
        return 1;
    }
    if (!verify_output(runB, 3.0f, 9.0f, 0.0f)) {
        return 1;
    }

    return 0;
}
