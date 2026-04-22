// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl.h"

#include <iostream>

int
main()
{
    rawgl::ShaderModuleDefinition fragment;
    fragment.role = rawgl::ShaderModuleRole::fragment;
    fragment.sourceKind = rawgl::ShaderModuleSourceKind::glslText;
    fragment.glslText = R"(#version 450 core
layout(location = 0) out vec4 OutColor;
void main()
{
    OutColor = vec4(0.25, 0.5, 0.75, 1.0);
}
)";
    fragment.debugLabel = "rawgl_core_default_vertex_smoke_fragment";

    rawgl::Pass pass;
    pass.programKind = rawgl::ShaderProgramKind::vertfrag;
    pass.shaderModules = { fragment };
    pass.sizeX = 2;
    pass.sizeY = 2;
    pass.outputs = { rawgl::CapturedOutput("OutColor", "rgba32f", 4, 3, 16) };

    rawgl::Workflow workflow;
    workflow.verbosity = 0;
    workflow.passes = { pass };

    rawgl::Session session;
    rawgl::PrepareResult prepareResult = session.prepare(workflow);
    if (!prepareResult.success || !prepareResult.workflow) {
        std::cerr << "Workflow preparation failed: " << prepareResult.errorMessage << std::endl;
        return 1;
    }

    rawgl::RunResult executionResult = prepareResult.workflow->run(rawgl::RunSettings {});
    if (!executionResult.success) {
        std::cerr << "Workflow execution failed: " << executionResult.errorMessage << std::endl;
        return 1;
    }

    const auto imageIt = executionResult.capturedOutputs.find("OutColor::0");
    if (imageIt == executionResult.capturedOutputs.end()) {
        std::cerr << "Default vertex workflow did not capture OutColor::0" << std::endl;
        return 1;
    }

    const rawgl::HostImageData& image = imageIt->second;
    if (image.width != 2 || image.height != 2 || image.channels != 4) {
        std::cerr << "Unexpected captured image dimensions/channels" << std::endl;
        return 1;
    }
    if (image.bytes.size() != 2u * 2u * 4u * sizeof(float)) {
        std::cerr << "Unexpected captured image payload size" << std::endl;
        return 1;
    }

    const float* values = reinterpret_cast<const float*>(image.bytes.data());
    for (int pixelIndex = 0; pixelIndex < 4; ++pixelIndex) {
        const size_t base = static_cast<size_t>(pixelIndex) * 4u;
        if (values[base + 0u] != 0.25f || values[base + 1u] != 0.5f || values[base + 2u] != 0.75f
            || values[base + 3u] != 1.0f) {
            std::cerr << "Unexpected captured pixel values at pixel " << pixelIndex << std::endl;
            return 1;
        }
    }

    return 0;
}
