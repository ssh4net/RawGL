// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_core.h"

#include <OpenImageIO/imageio.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

namespace {

bool
read_pixel_rgba32f(const std::filesystem::path& path, float pixel[4])
{
    std::unique_ptr<OIIO::ImageInput> input = OIIO::ImageInput::open(path.string());
    if (!input) {
        return false;
    }

    const OIIO::ImageSpec spec = input->spec();
    if (spec.width <= 0 || spec.nchannels <= 0) {
        return false;
    }

    std::vector<float> scanline(static_cast<size_t>(spec.width) * static_cast<size_t>(spec.nchannels), 0.0f);
    if (!input->read_scanline(0, 0, OIIO::TypeDesc::FLOAT, scanline.data()) || !input->close()) {
        return false;
    }

    pixel[0] = scanline[0];
    pixel[1] = spec.nchannels > 1 ? scanline[1] : pixel[0];
    pixel[2] = spec.nchannels > 2 ? scanline[2] : pixel[0];
    pixel[3] = spec.nchannels > 3 ? scanline[3] : 1.0f;
    return true;
}

bool
verify_output(const std::filesystem::path& outputPath)
{
    float pixel[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (!read_pixel_rgba32f(outputPath, pixel)) {
        std::cerr << "Unable to read output image: " << outputPath << std::endl;
        return false;
    }

    if (pixel[0] != 0.25f || pixel[1] != 0.5f || pixel[2] != 0.75f || pixel[3] != 1.0f) {
        std::cerr << "Unexpected output pixel in " << outputPath << ": [" << pixel[0] << ", " << pixel[1] << ", "
                  << pixel[2] << ", " << pixel[3] << "]" << std::endl;
        return false;
    }

    return true;
}

rawgl::GraphBuildRequest
make_transient_reuse_request(const std::filesystem::path& outputPath)
{
    rawgl::GraphPassDefinition sourcePass;
    sourcePass.programKind              = rawgl::ShaderProgramKind::compute;
    sourcePass.shaderPaths              = { "tests/shaders/image_chain_source.comp" };
    sourcePass.sizeX                    = 1;
    sourcePass.sizeY                    = 1;
    sourcePass.workGroupSizeX           = 1;
    sourcePass.workGroupSizeY           = 1;
    sourcePass.hasExplicitWorkGroupSize = true;
    sourcePass.outputs.push_back(rawgl::GraphOutputDefinition {
        "o_mid0",
        "",
        "rgba32f",
        4,
        3,
        16,
        "",
        {},
    });

    rawgl::GraphPassDefinition consumePass;
    consumePass.programKind              = rawgl::ShaderProgramKind::compute;
    consumePass.shaderPaths              = { "tests/shaders/image_chain_consume.comp" };
    consumePass.sizeX                    = 1;
    consumePass.sizeY                    = 1;
    consumePass.workGroupSizeX           = 1;
    consumePass.workGroupSizeY           = 1;
    consumePass.hasExplicitWorkGroupSize = true;
    consumePass.inputs.push_back(rawgl::GraphInputDefinition {
        "u_mid0",
        rawgl::GraphInputSourceKind::passOutput,
        {},
        {},
        {},
        {},
        "",
        "o_mid0",
        0,
        "",
        {},
    });
    consumePass.outputs.push_back(rawgl::GraphOutputDefinition {
        "o_out0",
        outputPath.string(),
        "rgba32f",
        4,
        3,
        16,
        "",
        {},
    });

    rawgl::GraphBuildRequest request;
    request.definition.verbosity = 0;
    request.definition.passes.push_back(std::move(sourcePass));
    request.definition.passes.push_back(std::move(consumePass));
    return request;
}

}  // namespace

int
main()
{
    const std::filesystem::path outputPath = "tests/outputs/rawgl_core_transient_output_reuse_smoke.exr";
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    rawgl::RawGLContext context;
    rawgl::GraphBuildResult buildResult = context.buildGraph(make_transient_reuse_request(outputPath));
    if (!buildResult.success || !buildResult.graph) {
        std::cerr << "Graph build failed: " << buildResult.errorMessage << std::endl;
        return 1;
    }

    const rawgl::GraphExecutionResult firstRun = buildResult.graph->execute(rawgl::GraphExecutionRequest {});
    if (!firstRun.success) {
        std::cerr << "First graph execution failed: " << firstRun.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(outputPath)) {
        return 1;
    }

    const rawgl::GraphExecutionResult secondRun = buildResult.graph->execute(rawgl::GraphExecutionRequest {});
    if (!secondRun.success) {
        std::cerr << "Second graph execution failed: " << secondRun.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(outputPath)) {
        return 1;
    }

    return 0;
}
