/*
 * This file is part of the RawGL distribution (https://github.com/ssh4net/RawGL).
 * Copyright (c) 2022-2026 Erium Vladlen.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "rawgl_core.h"

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
verify_output(const std::filesystem::path& outputPath, float expectedValue)
{
    float pixel[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (!read_pixel_rgba32f(outputPath, pixel)) {
        std::cerr << "Unable to read output image: " << outputPath << std::endl;
        return false;
    }

    if (pixel[0] != expectedValue || pixel[1] != expectedValue || pixel[2] != expectedValue || pixel[3] != 1.0f) {
        std::cerr << "Unexpected output pixel in " << outputPath << ": [" << pixel[0] << ", " << pixel[1] << ", "
                  << pixel[2] << ", " << pixel[3] << "]" << std::endl;
        return false;
    }

    return true;
}

rawgl::GraphBuildRequest
make_persistent_counter_request(const std::filesystem::path& outputPath)
{
    rawgl::GraphPassDefinition pass;
    pass.programKind              = rawgl::ShaderProgramKind::compute;
    pass.shaderPaths              = { "tests/shaders/persistent_atomic_counter.comp" };
    pass.sizeX                    = 1;
    pass.sizeY                    = 1;
    pass.workGroupSizeX           = 1;
    pass.workGroupSizeY           = 1;
    pass.hasExplicitWorkGroupSize = true;
    pass.atomicCounters.push_back(rawgl::GraphAtomicCounterDefinition {
        "counter0",
        2u,
        "graph_counter0",
    });
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

    rawgl::GraphBuildRequest request;
    request.definition.verbosity = 0;
    request.definition.passes.push_back(std::move(pass));
    return request;
}

}  // namespace

int
main()
{
    const std::filesystem::path outputPath = "tests/outputs/rawgl_core_persistent_atomic_counter_smoke.exr";
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    rawgl::RawGLContext context;
    rawgl::GraphBuildResult buildResult = context.buildGraph(make_persistent_counter_request(outputPath));
    if (!buildResult.success || !buildResult.graph) {
        std::cerr << "Graph build failed: " << buildResult.errorMessage << std::endl;
        return 1;
    }

    const rawgl::GraphExecutionResult firstRun = buildResult.graph->execute(rawgl::GraphExecutionRequest {});
    if (!firstRun.success) {
        std::cerr << "First graph execution failed: " << firstRun.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(outputPath, 3.0f)) {
        return 1;
    }

    const rawgl::GraphExecutionResult secondRun = buildResult.graph->execute(rawgl::GraphExecutionRequest {});
    if (!secondRun.success) {
        std::cerr << "Second graph execution failed: " << secondRun.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(outputPath, 4.0f)) {
        return 1;
    }

    return 0;
}
