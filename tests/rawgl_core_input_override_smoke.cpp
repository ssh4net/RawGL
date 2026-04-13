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

namespace {

bool
read_pixel_rgba32f(const std::filesystem::path& path, float pixel[4])
{
    std::unique_ptr<OIIO::ImageInput> input = OIIO::ImageInput::open(path.string());
    if (!input) {
        return false;
    }

    if (!input->read_image(0, 0, 0, 4, OIIO::TypeDesc::FLOAT, pixel)) {
        return false;
    }

    return input->close();
}

rawgl::GraphBuildRequest
make_input_override_request(const std::filesystem::path& outputPath)
{
    rawgl::GraphPassDefinition pass;
    pass.programKind              = rawgl::ShaderProgramKind::compute;
    pass.shaderPaths              = { "tests/shaders/input_override.comp" };
    pass.sizeX                    = 1;
    pass.sizeY                    = 1;
    pass.workGroupSizeX           = 1;
    pass.workGroupSizeY           = 1;
    pass.hasExplicitWorkGroupSize = true;
    pass.inputs.push_back(rawgl::GraphInputDefinition {
        "gain",
        rawgl::GraphInputSourceKind::floatValues,
        {},
        {},
        { 0.25f },
        {},
        "",
        "",
        0,
        "",
        {},
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

}  // namespace

int
main()
{
    const std::filesystem::path outputPath = "tests/outputs/rawgl_core_input_override_smoke.exr";
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    rawgl::RawGLContext context;
    rawgl::GraphBuildResult buildResult = context.buildGraph(make_input_override_request(outputPath));
    if (!buildResult.success || !buildResult.graph) {
        std::cerr << "Graph build failed: " << buildResult.errorMessage << std::endl;
        return 1;
    }

    const rawgl::GraphExecutionResult defaultRun = buildResult.graph->execute(rawgl::GraphExecutionRequest {});
    if (!defaultRun.success) {
        std::cerr << "Default graph execution failed: " << defaultRun.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(outputPath, 0.25f)) {
        return 1;
    }

    rawgl::GraphExecutionRequest overrideRun;
    overrideRun.inputOverrides.push_back(rawgl::GraphInputOverride {
        0,
        "gain",
        rawgl::GraphInputSourceKind::floatValues,
        {},
        {},
        { 0.75f },
        {},
        "",
        {},
    });

    const rawgl::GraphExecutionResult overriddenRun = buildResult.graph->execute(overrideRun);
    if (!overriddenRun.success) {
        std::cerr << "Overridden graph execution failed: " << overriddenRun.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(outputPath, 0.75f)) {
        return 1;
    }

    return 0;
}
