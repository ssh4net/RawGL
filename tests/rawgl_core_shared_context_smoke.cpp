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
make_system_uniform_graph_request(const std::filesystem::path& outputPath)
{
    rawgl::GraphPassDefinition pass;
    pass.programKind              = rawgl::ShaderProgramKind::compute;
    pass.shaderPaths              = { "tests/shaders/system_uniforms.comp" };
    pass.sizeX                    = 1;
    pass.sizeY                    = 1;
    pass.workGroupSizeX           = 1;
    pass.workGroupSizeY           = 1;
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

    rawgl::GraphBuildRequest request;
    request.definition.verbosity = 0;
    request.definition.passes.push_back(std::move(pass));
    return request;
}

bool
verify_output(const std::filesystem::path& outputPath, float expectedTime, float expectedFrame, float expectedPassIndex)
{
    float pixel[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (!read_pixel_rgba32f(outputPath, pixel)) {
        std::cerr << "Unable to read output image: " << outputPath << std::endl;
        return false;
    }

    if (pixel[0] != expectedTime || pixel[1] != expectedFrame || pixel[2] != expectedPassIndex || pixel[3] != 1.0f) {
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
    const std::filesystem::path outputA = "tests/outputs/rawgl_core_shared_context_a.exr";
    const std::filesystem::path outputB = "tests/outputs/rawgl_core_shared_context_b.exr";
    std::error_code removeError;
    std::filesystem::remove(outputA, removeError);
    std::filesystem::remove(outputB, removeError);

    rawgl::RawGLContext context;

    const rawgl::ShaderInterface inspectionA =
        context.inspectShaderInterface(rawgl::ShaderInspectionRequest {
            rawgl::ShaderProgramKind::compute,
            { "tests/shaders/system_uniforms.comp" },
        });
    const rawgl::ShaderInterface inspectionB =
        context.inspectShaderInterface(rawgl::ShaderInspectionRequest {
            rawgl::ShaderProgramKind::compute,
            { "tests/shaders/system_uniforms.comp" },
        });

    if (!inspectionA.success || !inspectionB.success) {
        std::cerr << "Shared-context inspection failed." << std::endl;
        return 1;
    }

    rawgl::GraphBuildResult graphA = context.buildGraph(make_system_uniform_graph_request(outputA));
    rawgl::GraphBuildResult graphB = context.buildGraph(make_system_uniform_graph_request(outputB));

    if (!graphA.success || !graphA.graph) {
        std::cerr << "Shared-context graph A build failed: " << graphA.errorMessage << std::endl;
        return 1;
    }
    if (!graphB.success || !graphB.graph) {
        std::cerr << "Shared-context graph B build failed: " << graphB.errorMessage << std::endl;
        return 1;
    }

    const rawgl::GraphExecutionResult runA = graphA.graph->execute(rawgl::GraphExecutionRequest {
        rawgl::SystemUniformState { 2.0, 0.5, 7, 0 },
    });
    const rawgl::GraphExecutionResult runB = graphB.graph->execute(rawgl::GraphExecutionRequest {
        rawgl::SystemUniformState { 3.0, 0.25, 9, 0 },
    });

    if (!runA.success) {
        std::cerr << "Shared-context graph A execution failed: " << runA.errorMessage << std::endl;
        return 1;
    }
    if (!runB.success) {
        std::cerr << "Shared-context graph B execution failed: " << runB.errorMessage << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(outputA) || !std::filesystem::exists(outputB)) {
        std::cerr << "Shared-context execution did not produce expected output images." << std::endl;
        return 1;
    }

    if (!verify_output(outputA, 2.0f, 7.0f, 0.0f)) {
        return 1;
    }
    if (!verify_output(outputB, 3.0f, 9.0f, 0.0f)) {
        return 1;
    }

    return 0;
}
