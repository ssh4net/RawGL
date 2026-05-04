// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl.h"

#include <cstring>
#include <iostream>

int
main()
{
    rawgl::Pass pass;
    pass.programKind              = rawgl::ShaderProgramKind::compute;
    pass.shaderModules.push_back(rawgl::ShaderModuleDefinition {
        rawgl::ShaderModuleRole::compute,
        rawgl::ShaderModuleSourceKind::glslText,
        "",
        R"(#version 450 core

uniform float iTime;
uniform int iFrame;
uniform int iPassIndex;
layout(rgba32f) writeonly uniform image2D o_out0;

layout(local_size_x = 1, local_size_y = 1) in;

void main()
{
    imageStore(o_out0, ivec2(0, 0), vec4(iTime, float(iFrame), float(iPassIndex), 1.0));
}
)",
        {},
        "rawgl_core_graph_smoke_inline_system_uniforms",
    });
    pass.sizeX                    = 1;
    pass.sizeY                    = 1;
    pass.workGroupSizeX           = 1;
    pass.workGroupSizeY           = 1;
    pass.hasExplicitWorkGroupSize = true;
    pass.outputs.push_back(rawgl::CapturedOutput("o_out0", "rgba32f", 4, 3, 16));

    rawgl::Workflow workflow;
    workflow.verbosity = 0;
    workflow.passes.push_back(std::move(pass));

    rawgl::Session session;
    rawgl::PrepareResult prepareResult = session.prepare(workflow);
    if (!prepareResult.success || !prepareResult.workflow) {
        std::cerr << "Workflow preparation failed: " << prepareResult.errorMessage << std::endl;
        return 1;
    }

    rawgl::RunSettings runSettings;
    runSettings.systemUniforms = rawgl::SystemUniformState { 1.0, 1.0 / 24.0, 24, 0 };
    rawgl::RunResult executionResult = prepareResult.workflow->run(runSettings);
    if (!executionResult.success) {
        std::cerr << "Workflow execution failed: " << executionResult.errorMessage << std::endl;
        return 1;
    }

    const auto outputIt = executionResult.capturedOutputs.find("o_out0::0");
    if (outputIt == executionResult.capturedOutputs.end()) {
        std::cerr << "Graph execution did not capture output image." << std::endl;
        return 1;
    }

    const rawgl::HostImageData& output = outputIt->second;
    if (output.width != 1 || output.height != 1 || output.channels != 4
        || output.bytes.size() != sizeof(float) * 4U) {
        std::cerr << "Unexpected graph smoke output dimensions or layout." << std::endl;
        return 1;
    }

    float pixel[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    std::memcpy(pixel, output.bytes.data(), sizeof(pixel));
    if (pixel[0] != 1.0f || pixel[1] != 24.0f || pixel[2] != 0.0f || pixel[3] != 1.0f) {
        std::cerr << "Unexpected graph smoke output pixel: [" << pixel[0] << ", " << pixel[1] << ", " << pixel[2]
                  << ", " << pixel[3] << "]" << std::endl;
        return 1;
    }

    return 0;
}
