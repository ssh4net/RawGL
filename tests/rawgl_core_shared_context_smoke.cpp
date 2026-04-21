// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl.h"

#include <OpenImageIO/imageio.h>

#include <filesystem>
#include <iostream>
#include <memory>

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

rawgl::Workflow
make_system_uniform_workflow(const std::filesystem::path& outputPath)
{
    rawgl::Pass pass;
    pass.programKind              = rawgl::ShaderProgramKind::compute;
    pass.shaderModules.push_back(make_file_module("tests/shaders/system_uniforms.comp"));
    pass.sizeX                    = 1;
    pass.sizeY                    = 1;
    pass.workGroupSizeX           = 1;
    pass.workGroupSizeY           = 1;
    pass.hasExplicitWorkGroupSize = true;

    rawgl::OutputBinding output;
    output.name         = "o_out0";
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

    rawgl::PrepareResult workflowA = session.prepare(make_system_uniform_workflow(outputA));
    rawgl::PrepareResult workflowB = session.prepare(make_system_uniform_workflow(outputB));

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
