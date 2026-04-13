// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl_core.h"

#include <OpenImageIO/imageio.h>

#include <filesystem>
#include <iostream>

int
main()
{
    const std::filesystem::path outputPath = "tests/outputs/rawgl_core_graph_smoke.exr";
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    rawgl::GraphPassDefinition pass;
    pass.programKind            = rawgl::ShaderProgramKind::compute;
    pass.shaderPaths            = { "tests/shaders/system_uniforms.comp" };
    pass.sizeX                  = 1;
    pass.sizeY                  = 1;
    pass.workGroupSizeX         = 1;
    pass.workGroupSizeY         = 1;
    pass.hasExplicitWorkGroupSize = true;
    pass.outputs.push_back(rawgl::GraphOutputDefinition {
        "o_out0",
        outputPath.string(),
        "rgba32f",
        4,
        3,
        16,
        "",
        {},
    });

    rawgl::GraphBuildRequest graphRequest;
    graphRequest.definition.verbosity = 0;
    graphRequest.definition.passes.push_back(std::move(pass));

    rawgl::RawGLContext context;
    rawgl::GraphBuildResult buildResult = context.buildGraph(graphRequest);
    if (!buildResult.success || !buildResult.graph) {
        std::cerr << "Graph build failed: " << buildResult.errorMessage << std::endl;
        return 1;
    }

    rawgl::GraphExecutionResult executionResult = buildResult.graph->execute(rawgl::GraphExecutionRequest {
        rawgl::SystemUniformState { 1.0, 1.0 / 24.0, 24, 0 },
    });
    if (!executionResult.success) {
        std::cerr << "Graph execution failed: " << executionResult.errorMessage << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(outputPath)) {
        std::cerr << "Graph execution did not produce output image: " << outputPath << std::endl;
        return 1;
    }

    std::unique_ptr<OIIO::ImageInput> input = OIIO::ImageInput::open(outputPath.string());
    if (!input) {
        std::cerr << "Unable to read graph smoke output image: " << outputPath << std::endl;
        return 1;
    }

    const OIIO::ImageSpec spec = input->spec();
    float pixel[4]             = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (!input->read_image(0, 0, 0, 4, OIIO::TypeDesc::FLOAT, pixel)) {
        std::cerr << "Unable to read graph smoke output pixel." << std::endl;
        return 1;
    }

    if (!input->close()) {
        std::cerr << "Unable to close graph smoke image input." << std::endl;
        return 1;
    }

    if (spec.width != 1 || spec.height != 1) {
        std::cerr << "Unexpected graph smoke output dimensions." << std::endl;
        return 1;
    }
    if (pixel[0] != 1.0f || pixel[1] != 24.0f || pixel[2] != 0.0f || pixel[3] != 1.0f) {
        std::cerr << "Unexpected graph smoke output pixel: [" << pixel[0] << ", " << pixel[1] << ", " << pixel[2]
                  << ", " << pixel[3] << "]" << std::endl;
        return 1;
    }

    return 0;
}
