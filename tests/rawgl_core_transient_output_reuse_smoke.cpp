// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl.h"

#include <iostream>
#include <cstring>
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
read_pixel_rgba32f(const rawgl::RunResult& runResult, float pixel[4])
{
    const auto captureIt = runResult.capturedOutputs.find("o_out0::1");
    if (captureIt == runResult.capturedOutputs.end()) {
        return false;
    }
    if (captureIt->second.bytes.size() < sizeof(pixel[0]) * 4u) {
        return false;
    }
    std::memcpy(pixel, captureIt->second.bytes.data(), sizeof(pixel[0]) * 4u);
    return true;
}

bool
verify_output(const rawgl::RunResult& runResult)
{
    float pixel[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (!read_pixel_rgba32f(runResult, pixel)) {
        std::cerr << "Unable to read captured output image." << std::endl;
        return false;
    }

    if (pixel[0] != 0.25f || pixel[1] != 0.5f || pixel[2] != 0.75f || pixel[3] != 1.0f) {
        std::cerr << "Unexpected captured output pixel: [" << pixel[0] << ", " << pixel[1] << ", "
                  << pixel[2] << ", " << pixel[3] << "]" << std::endl;
        return false;
    }

    return true;
}

rawgl::Workflow
make_transient_reuse_workflow()
{
    rawgl::Pass sourcePass;
    sourcePass.programKind              = rawgl::ShaderProgramKind::compute;
    sourcePass.shaderModules.push_back(make_file_module(rawgl::ShaderProgramKind::compute, "tests/shaders/image_chain_source.comp"));
    sourcePass.sizeX                    = 1;
    sourcePass.sizeY                    = 1;
    sourcePass.workGroupSizeX           = 1;
    sourcePass.workGroupSizeY           = 1;
    sourcePass.hasExplicitWorkGroupSize = true;
    rawgl::OutputBinding sourceOutput;
    sourceOutput.name         = "o_mid0";
    sourceOutput.format       = "rgba32f";
    sourceOutput.channels     = 4;
    sourceOutput.alphaChannel = 3;
    sourceOutput.bits         = 16;
    sourcePass.outputs.push_back(std::move(sourceOutput));

    rawgl::Pass consumePass;
    consumePass.programKind              = rawgl::ShaderProgramKind::compute;
    consumePass.shaderModules.push_back(make_file_module(rawgl::ShaderProgramKind::compute, "tests/shaders/image_chain_consume.comp"));
    consumePass.sizeX                    = 1;
    consumePass.sizeY                    = 1;
    consumePass.workGroupSizeX           = 1;
    consumePass.workGroupSizeY           = 1;
    consumePass.hasExplicitWorkGroupSize = true;
    rawgl::InputBinding midInput;
    midInput.name                 = "u_mid0";
    midInput.sourceKind           = rawgl::InputSourceKind::passOutput;
    midInput.referencedOutputName = "o_mid0";
    midInput.referencedPassIndex  = 0;
    consumePass.inputs.push_back(std::move(midInput));

    consumePass.outputs.push_back(rawgl::CapturedOutput("o_out0", "rgba32f", 4, 3, 16));

    rawgl::Workflow workflow;
    workflow.verbosity = 0;
    workflow.passes.push_back(std::move(sourcePass));
    workflow.passes.push_back(std::move(consumePass));
    return workflow;
}

}  // namespace

int
main()
{
    rawgl::Session session;
    rawgl::PrepareResult prepareResult = session.prepare(make_transient_reuse_workflow());
    if (!prepareResult.success || !prepareResult.workflow) {
        std::cerr << "Workflow preparation failed: " << prepareResult.errorMessage << std::endl;
        return 1;
    }

    const rawgl::RunResult firstRun = prepareResult.workflow->run(rawgl::RunSettings {});
    if (!firstRun.success) {
        std::cerr << "First workflow execution failed: " << firstRun.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(firstRun)) {
        return 1;
    }

    const rawgl::RunResult secondRun = prepareResult.workflow->run(rawgl::RunSettings {});
    if (!secondRun.success) {
        std::cerr << "Second workflow execution failed: " << secondRun.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(secondRun)) {
        return 1;
    }

    return 0;
}
