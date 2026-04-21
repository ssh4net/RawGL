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
make_input_override_workflow(const std::filesystem::path& outputPath)
{
    rawgl::Pass pass;
    pass.programKind              = rawgl::ShaderProgramKind::compute;
    pass.shaderModules.push_back(make_file_module("tests/shaders/input_override.comp"));
    pass.sizeX                    = 1;
    pass.sizeY                    = 1;
    pass.workGroupSizeX           = 1;
    pass.workGroupSizeY           = 1;
    pass.hasExplicitWorkGroupSize = true;

    rawgl::InputBinding input;
    input.name       = "gain";
    input.sourceKind = rawgl::InputSourceKind::floatValues;
    input.floatValues.push_back(0.25f);
    pass.inputs.push_back(std::move(input));

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

    rawgl::Session session;
    rawgl::PrepareResult prepareResult = session.prepare(make_input_override_workflow(outputPath));
    if (!prepareResult.success || !prepareResult.workflow) {
        std::cerr << "Workflow preparation failed: " << prepareResult.errorMessage << std::endl;
        return 1;
    }

    const rawgl::RunResult defaultRun = prepareResult.workflow->run(rawgl::RunSettings {});
    if (!defaultRun.success) {
        std::cerr << "Default workflow execution failed: " << defaultRun.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(outputPath, 0.25f)) {
        return 1;
    }

    rawgl::RunSettings overrideRun;
    rawgl::InputOverride overrideInput;
    overrideInput.passIndex   = 0;
    overrideInput.name        = "gain";
    overrideInput.sourceKind  = rawgl::InputSourceKind::floatValues;
    overrideInput.floatValues = { 0.75f };
    overrideRun.overrides.push_back(std::move(overrideInput));

    const rawgl::RunResult overriddenRun = prepareResult.workflow->run(overrideRun);
    if (!overriddenRun.success) {
        std::cerr << "Overridden workflow execution failed: " << overriddenRun.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(outputPath, 0.75f)) {
        return 1;
    }

    return 0;
}
