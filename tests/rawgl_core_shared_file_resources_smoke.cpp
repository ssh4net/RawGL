// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl.h"

#include <OpenImageIO/imageio.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

namespace {

rawgl::ShaderModuleDefinition
make_file_module(const rawgl::ShaderProgramKind programKind, const char* path)
{
    rawgl::ShaderModuleDefinition module;
    module.sourceKind = rawgl::ShaderModuleSourceKind::filePath;
    module.path       = path;
    module.role       = programKind == rawgl::ShaderProgramKind::compute ? rawgl::ShaderModuleRole::compute
                                                                         : rawgl::ShaderModuleRole::automatic;
    return module;
}

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

rawgl::Workflow
make_texture_workflow(const std::filesystem::path& outputPath)
{
    rawgl::Pass pass;
    pass.programKind = rawgl::ShaderProgramKind::vertfrag;
    rawgl::ShaderModuleDefinition vertexModule;
    vertexModule.role       = rawgl::ShaderModuleRole::vertex;
    vertexModule.sourceKind = rawgl::ShaderModuleSourceKind::filePath;
    vertexModule.path       = "tests/shaders/empty.vert";
    pass.shaderModules.push_back(std::move(vertexModule));
    rawgl::ShaderModuleDefinition fragmentModule;
    fragmentModule.role       = rawgl::ShaderModuleRole::fragment;
    fragmentModule.sourceKind = rawgl::ShaderModuleSourceKind::filePath;
    fragmentModule.path       = "tests/shaders/pass1.frag";
    pass.shaderModules.push_back(std::move(fragmentModule));
    pass.sizeX       = 8;
    pass.sizeY       = 8;
    rawgl::InputBinding input;
    input.name        = "InSample";
    input.sourceKind  = rawgl::InputSourceKind::textureFile;
    input.texturePath = "tests/inputs/EmptyPresetLUT.png";
    pass.inputs.push_back(std::move(input));

    rawgl::OutputBinding output;
    output.name         = "OutSample";
    output.path         = outputPath.string();
    output.format       = "rgba32f";
    output.channels     = 4;
    output.alphaChannel = 3;
    output.bits         = 16;
    pass.outputs.push_back(std::move(output));

    rawgl::Workflow workflow;
    workflow.verbosity = 0;
    workflow.passes.push_back(std::move(pass));
    return workflow;
}

rawgl::Workflow
make_mesh_workflow(const std::filesystem::path& outputPath)
{
    rawgl::Pass pass;
    pass.programKind = rawgl::ShaderProgramKind::vertfrag;
    rawgl::ShaderModuleDefinition vertexModule;
    vertexModule.role       = rawgl::ShaderModuleRole::vertex;
    vertexModule.sourceKind = rawgl::ShaderModuleSourceKind::filePath;
    vertexModule.path       = "tests/shaders/mesh_ao.vert";
    pass.shaderModules.push_back(std::move(vertexModule));
    rawgl::ShaderModuleDefinition fragmentModule;
    fragmentModule.role       = rawgl::ShaderModuleRole::fragment;
    fragmentModule.sourceKind = rawgl::ShaderModuleSourceKind::filePath;
    fragmentModule.path       = "tests/shaders/mesh_ao.frag";
    pass.shaderModules.push_back(std::move(fragmentModule));
    pass.sizeX       = 64;
    pass.sizeY       = 64;
    rawgl::MeshBinding mesh;
    mesh.sourceKind = rawgl::MeshSourceKind::file;
    mesh.path       = "tests/inputs/sponge.ply";
    mesh.parameters = { { "tris", "true" }, { "rend", "tr" } };
    pass.meshes.push_back(std::move(mesh));

    rawgl::OutputBinding output;
    output.name         = "OutSample";
    output.path         = outputPath.string();
    output.format       = "rgba32f";
    output.channels     = 4;
    output.alphaChannel = 3;
    output.bits         = 16;
    pass.outputs.push_back(std::move(output));

    rawgl::Workflow workflow;
    workflow.verbosity = 0;
    workflow.passes.push_back(std::move(pass));
    return workflow;
}

bool
build_and_run(rawgl::Session& session,
              const rawgl::Workflow& workflow,
              const std::filesystem::path& outputPath)
{
    rawgl::PrepareResult prepareResult = session.prepare(workflow);
    if (!prepareResult.success || !prepareResult.workflow) {
        std::cerr << "Workflow preparation failed: " << prepareResult.errorMessage << std::endl;
        return false;
    }

    const rawgl::RunResult executionResult = prepareResult.workflow->run(rawgl::RunSettings {});
    if (!executionResult.success) {
        std::cerr << "Workflow execution failed: " << executionResult.errorMessage << std::endl;
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

    rawgl::Session session;

    if (!build_and_run(session, make_texture_workflow(texOutA), texOutA)) {
        return 1;
    }
    if (!build_and_run(session, make_texture_workflow(texOutB), texOutB)) {
        return 1;
    }
    if (!compare_images(texOutA, texOutB)) {
        std::cerr << "Shared texture-resource outputs differ." << std::endl;
        return 1;
    }

    const rawgl::SessionStats textureStats = session.stats();
    if (textureStats.textures != 1) {
        std::cerr << "Expected one shared texture resource, got " << textureStats.textures << std::endl;
        return 1;
    }

    if (!build_and_run(session, make_mesh_workflow(meshOutA), meshOutA)) {
        return 1;
    }
    if (!build_and_run(session, make_mesh_workflow(meshOutB), meshOutB)) {
        return 1;
    }
    if (!compare_images(meshOutA, meshOutB)) {
        std::cerr << "Shared mesh-resource outputs differ." << std::endl;
        return 1;
    }

    const rawgl::SessionStats meshStats = session.stats();
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
