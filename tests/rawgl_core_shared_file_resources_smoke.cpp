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
read_image_rgba32f(const std::filesystem::path& path, std::vector<float>& pixels, int& width, int& height)
{
    std::unique_ptr<OIIO::ImageInput> input = OIIO::ImageInput::open(path.string());
    if (!input) {
        return false;
    }

    const OIIO::ImageSpec spec = input->spec();
    width                      = spec.width;
    height                     = spec.height;
    pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);

    if (!input->read_image(0, 0, 0, 4, OIIO::TypeDesc::FLOAT, pixels.data())) {
        return false;
    }

    return input->close();
}

bool
compare_images(const std::filesystem::path& pathA, const std::filesystem::path& pathB)
{
    std::vector<float> pixelsA;
    std::vector<float> pixelsB;
    int widthA  = 0;
    int heightA = 0;
    int widthB  = 0;
    int heightB = 0;

    if (!read_image_rgba32f(pathA, pixelsA, widthA, heightA)) {
        std::cerr << "Unable to read image: " << pathA << std::endl;
        return false;
    }
    if (!read_image_rgba32f(pathB, pixelsB, widthB, heightB)) {
        std::cerr << "Unable to read image: " << pathB << std::endl;
        return false;
    }

    if (widthA != widthB || heightA != heightB || pixelsA.size() != pixelsB.size()) {
        std::cerr << "Image dimensions differ between shared-resource outputs." << std::endl;
        return false;
    }

    for (size_t i = 0; i < pixelsA.size(); ++i) {
        if (pixelsA[i] != pixelsB[i]) {
            std::cerr << "Image data differs at element " << i << std::endl;
            return false;
        }
    }

    return true;
}

rawgl::GraphBuildRequest
make_texture_graph_request(const std::filesystem::path& outputPath)
{
    rawgl::GraphPassDefinition pass;
    pass.programKind = rawgl::ShaderProgramKind::vertfrag;
    pass.shaderPaths = { "tests/shaders/empty.vert", "tests/shaders/pass1.frag" };
    pass.sizeX       = 8;
    pass.sizeY       = 8;
    pass.inputs.push_back(rawgl::GraphInputDefinition {
        "InSample",
        rawgl::GraphInputSourceKind::textureFile,
        {},
        {},
        {},
        {},
        "tests/inputs/EmptyPresetLUT.png",
        "",
        0,
        {},
    });
    pass.outputs.push_back(rawgl::GraphOutputDefinition {
        "OutSample",
        outputPath.string(),
        "rgba32f",
        4,
        3,
        16,
        {},
    });

    rawgl::GraphBuildRequest request;
    request.definition.verbosity = 0;
    request.definition.passes.push_back(std::move(pass));
    return request;
}

rawgl::GraphBuildRequest
make_mesh_graph_request(const std::filesystem::path& outputPath)
{
    rawgl::GraphPassDefinition pass;
    pass.programKind = rawgl::ShaderProgramKind::vertfrag;
    pass.shaderPaths = { "tests/shaders/mesh_ao.vert", "tests/shaders/mesh_ao.frag" };
    pass.sizeX       = 64;
    pass.sizeY       = 64;
    pass.meshes.push_back(rawgl::GraphMeshDefinition {
        rawgl::GraphMeshSourceKind::file,
        "tests/inputs/sponge.ply",
        { { "tris", "true" }, { "rend", "tr" } },
    });
    pass.outputs.push_back(rawgl::GraphOutputDefinition {
        "OutSample",
        outputPath.string(),
        "rgba32f",
        4,
        3,
        16,
        {},
    });

    rawgl::GraphBuildRequest request;
    request.definition.verbosity = 0;
    request.definition.passes.push_back(std::move(pass));
    return request;
}

bool
build_and_run(rawgl::RawGLContext& context,
              const rawgl::GraphBuildRequest& request,
              const std::filesystem::path& outputPath)
{
    rawgl::GraphBuildResult buildResult = context.buildGraph(request);
    if (!buildResult.success || !buildResult.graph) {
        std::cerr << "Graph build failed: " << buildResult.errorMessage << std::endl;
        return false;
    }

    const rawgl::GraphExecutionResult executionResult = buildResult.graph->execute(rawgl::GraphExecutionRequest {});
    if (!executionResult.success) {
        std::cerr << "Graph execution failed: " << executionResult.errorMessage << std::endl;
        return false;
    }

    if (!std::filesystem::exists(outputPath)) {
        std::cerr << "Expected output was not created: " << outputPath << std::endl;
        return false;
    }

    return true;
}

}  // namespace

int
main()
{
    const std::filesystem::path texOutA  = "tests/outputs/rawgl_core_shared_tex_a.exr";
    const std::filesystem::path texOutB  = "tests/outputs/rawgl_core_shared_tex_b.exr";
    const std::filesystem::path meshOutA = "tests/outputs/rawgl_core_shared_mesh_a.exr";
    const std::filesystem::path meshOutB = "tests/outputs/rawgl_core_shared_mesh_b.exr";

    std::error_code removeError;
    std::filesystem::remove(texOutA, removeError);
    std::filesystem::remove(texOutB, removeError);
    std::filesystem::remove(meshOutA, removeError);
    std::filesystem::remove(meshOutB, removeError);

    rawgl::RawGLContext context;

    if (!build_and_run(context, make_texture_graph_request(texOutA), texOutA)) {
        return 1;
    }
    if (!build_and_run(context, make_texture_graph_request(texOutB), texOutB)) {
        return 1;
    }
    if (!compare_images(texOutA, texOutB)) {
        std::cerr << "Shared texture-resource outputs differ." << std::endl;
        return 1;
    }

    const rawgl::ContextCacheStats textureStats = context.cacheStats();
    if (textureStats.textures != 1) {
        std::cerr << "Expected one shared texture resource, got " << textureStats.textures << std::endl;
        return 1;
    }

    if (!build_and_run(context, make_mesh_graph_request(meshOutA), meshOutA)) {
        return 1;
    }
    if (!build_and_run(context, make_mesh_graph_request(meshOutB), meshOutB)) {
        return 1;
    }
    if (!compare_images(meshOutA, meshOutB)) {
        std::cerr << "Shared mesh-resource outputs differ." << std::endl;
        return 1;
    }

    const rawgl::ContextCacheStats meshStats = context.cacheStats();
    if (meshStats.meshesHost != 1) {
        std::cerr << "Expected one shared host mesh resource, got " << meshStats.meshesHost << std::endl;
        return 1;
    }
    if (meshStats.meshesGpu != 1) {
        std::cerr << "Expected one shared GPU mesh resource, got " << meshStats.meshesGpu << std::endl;
        return 1;
    }

    return 0;
}
