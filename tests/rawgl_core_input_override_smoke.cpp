// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl.h"

#include <iostream>
#include <cstring>

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
read_pixel_rgba32f(const rawgl::RunResult& runResult, float pixel[4])
{
    const auto captureIt = runResult.capturedOutputs.find("o_out0::0");
    if (captureIt == runResult.capturedOutputs.end()) {
        return false;
    }
    if (captureIt->second.bytes.size() < sizeof(pixel[0]) * 4u) {
        return false;
    }
    std::memcpy(pixel, captureIt->second.bytes.data(), sizeof(pixel[0]) * 4u);
    return true;
}

rawgl::Workflow
make_input_override_workflow()
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

    rawgl::InputBinding bias;
    bias.name        = "bias";
    bias.sourceKind  = rawgl::InputSourceKind::floatValues;
    bias.floatValues = { 0.0f, 0.0f, 0.0f };
    pass.inputs.push_back(std::move(bias));

    rawgl::InputBinding transform;
    transform.name        = "transform";
    transform.sourceKind  = rawgl::InputSourceKind::floatValues;
    transform.floatValues = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f,
    };
    pass.inputs.push_back(std::move(transform));

    pass.outputs.push_back(rawgl::CapturedOutput("o_out0", "rgba32f", 4, 3, 16));

    rawgl::Workflow workflow;
    workflow.verbosity = 0;
    workflow.passes.push_back(std::move(pass));
    return workflow;
}

bool
verify_output(const rawgl::RunResult& runResult, float expectedValue)
{
    float pixel[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (!read_pixel_rgba32f(runResult, pixel)) {
        std::cerr << "Unable to read captured output image." << std::endl;
        return false;
    }

    if (pixel[0] != expectedValue || pixel[1] != expectedValue || pixel[2] != expectedValue || pixel[3] != 1.0f) {
        std::cerr << "Unexpected captured output pixel: [" << pixel[0] << ", " << pixel[1] << ", "
                  << pixel[2] << ", " << pixel[3] << "]" << std::endl;
        return false;
    }

    return true;
}

}  // namespace

int
main()
{
    rawgl::Session session;
    rawgl::PrepareResult prepareResult = session.prepare(make_input_override_workflow());
    if (!prepareResult.success || !prepareResult.workflow) {
        std::cerr << "Workflow preparation failed: " << prepareResult.errorMessage << std::endl;
        return 1;
    }

    const rawgl::RunResult defaultRun = prepareResult.workflow->run(rawgl::RunSettings {});
    if (!defaultRun.success) {
        std::cerr << "Default workflow execution failed: " << defaultRun.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(defaultRun, 0.25f)) {
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
    if (!verify_output(overriddenRun, 0.75f)) {
        return 1;
    }

    rawgl::RunSettings vectorOverrideRun;
    rawgl::InputOverride overrideBias;
    overrideBias.passIndex   = 0;
    overrideBias.name        = "bias";
    overrideBias.sourceKind  = rawgl::InputSourceKind::floatValues;
    overrideBias.floatValues = { 0.0f, 0.125f, 0.25f };
    vectorOverrideRun.overrides.push_back(std::move(overrideBias));

    const rawgl::RunResult vectorOverriddenRun = prepareResult.workflow->run(vectorOverrideRun);
    if (!vectorOverriddenRun.success) {
        std::cerr << "Vector-overridden workflow execution failed: " << vectorOverriddenRun.errorMessage << std::endl;
        return 1;
    }

    float vectorPixel[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (!read_pixel_rgba32f(vectorOverriddenRun, vectorPixel)) {
        std::cerr << "Unable to read vector-overridden captured output image." << std::endl;
        return 1;
    }
    if (vectorPixel[0] != 0.25f || vectorPixel[1] != 0.375f || vectorPixel[2] != 0.5f
        || vectorPixel[3] != 1.0f) {
        std::cerr << "Unexpected vector-overridden captured output pixel: [" << vectorPixel[0] << ", "
                  << vectorPixel[1] << ", " << vectorPixel[2] << ", " << vectorPixel[3] << "]" << std::endl;
        return 1;
    }

    rawgl::RunSettings matrixOverrideRun;
    rawgl::InputOverride overrideTransform;
    overrideTransform.passIndex   = 0;
    overrideTransform.name        = "transform";
    overrideTransform.sourceKind  = rawgl::InputSourceKind::floatValues;
    overrideTransform.floatValues = {
        1.0f, 0.0f, 0.0f,
        0.0f, 2.0f, 0.0f,
        0.0f, 0.0f, 3.0f,
    };
    matrixOverrideRun.overrides.push_back(std::move(overrideTransform));

    const rawgl::RunResult matrixOverriddenRun = prepareResult.workflow->run(matrixOverrideRun);
    if (!matrixOverriddenRun.success) {
        std::cerr << "Matrix-overridden workflow execution failed: " << matrixOverriddenRun.errorMessage << std::endl;
        return 1;
    }

    float matrixPixel[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (!read_pixel_rgba32f(matrixOverriddenRun, matrixPixel)) {
        std::cerr << "Unable to read matrix-overridden captured output image." << std::endl;
        return 1;
    }
    if (matrixPixel[0] != 0.25f || matrixPixel[1] != 0.5f || matrixPixel[2] != 0.75f
        || matrixPixel[3] != 1.0f) {
        std::cerr << "Unexpected matrix-overridden captured output pixel: [" << matrixPixel[0] << ", "
                  << matrixPixel[1] << ", " << matrixPixel[2] << ", " << matrixPixel[3] << "]" << std::endl;
        return 1;
    }

    return 0;
}
