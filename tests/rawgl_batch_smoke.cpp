// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_batch.h"

#include <filesystem>
#include <iostream>

static bool
verify_core_batch_path()
{
    rawgl::Pass pass;
    pass.programKind = rawgl::ShaderProgramKind::compute;
    pass.shaderModules.push_back(rawgl::ShaderModuleDefinition {
        rawgl::ShaderModuleRole::compute,
        rawgl::ShaderModuleSourceKind::glslText,
        "",
        R"(#version 450 core
uniform float iTime;
uniform int iFrame;
layout(rgba32f) writeonly uniform image2D o_out0;
layout(local_size_x = 1, local_size_y = 1) in;
void main()
{
    imageStore(o_out0, ivec2(0, 0), vec4(iTime, float(iFrame), 0.0, 1.0));
}
)",
        {},
        "rawgl_batch_smoke_compute",
    });
    pass.sizeX = 1;
    pass.sizeY = 1;
    pass.workGroupSizeX = 1;
    pass.workGroupSizeY = 1;
    pass.hasExplicitWorkGroupSize = true;

    rawgl::OutputBinding output;
    output.name = "o_out0";
    output.format = "rgba32f";
    output.channels = 4;
    output.alphaChannel = 3;
    output.captureToHost = true;
    pass.outputs.push_back(output);

    rawgl::Workflow workflow;
    workflow.verbosity = 0;
    workflow.passes.push_back(std::move(pass));

    rawgl::Session session;
    rawgl::batch::BatchRunner runner(session);

    rawgl::batch::BatchPrepareResult prepareResult = runner.prepare(workflow);
    if (!prepareResult.success || !prepareResult.workflow) {
        std::cerr << "Batch core prepare failed: " << prepareResult.errorMessage << std::endl;
        return false;
    }

    rawgl::batch::BatchSubmitRequest firstRequest;
    firstRequest.settings.systemUniforms = rawgl::SystemUniformState { 1.0, 0.0, 24, 0 };

    rawgl::batch::BatchSubmitRequest secondRequest;
    secondRequest.settings.systemUniforms = rawgl::SystemUniformState { 2.0, 0.0, 48, 0 };

    rawgl::batch::BatchJobHandle firstHandle = runner.submit(*prepareResult.workflow, firstRequest);
    rawgl::batch::BatchJobHandle secondHandle = runner.submit(*prepareResult.workflow, secondRequest);

    rawgl::batch::BatchResult firstResult = firstHandle.wait();
    rawgl::batch::BatchResult secondResult = secondHandle.wait();

    if (firstResult.cancelled || secondResult.cancelled) {
        std::cerr << "Batch core result unexpectedly cancelled." << std::endl;
        return false;
    }
    if (firstResult.submitIndex != 0u || secondResult.submitIndex != 1u) {
        std::cerr << "Unexpected batch submit indices: " << firstResult.submitIndex << ", "
                  << secondResult.submitIndex << std::endl;
        return false;
    }
    if (!firstResult.runResult.success || !secondResult.runResult.success) {
        std::cerr << "Batch core run failed: " << firstResult.runResult.errorMessage << " / "
                  << secondResult.runResult.errorMessage << std::endl;
        return false;
    }

    const auto firstCaptured = firstResult.runResult.capturedOutputs.find("o_out0::0");
    const auto secondCaptured = secondResult.runResult.capturedOutputs.find("o_out0::0");
    if (firstCaptured == firstResult.runResult.capturedOutputs.end()
        || secondCaptured == secondResult.runResult.capturedOutputs.end()) {
        std::cerr << "Batch core run did not capture host output." << std::endl;
        return false;
    }
    if (firstCaptured->second.width != 1 || firstCaptured->second.height != 1 || firstCaptured->second.bytes.empty()
        || secondCaptured->second.width != 1 || secondCaptured->second.height != 1
        || secondCaptured->second.bytes.empty()) {
        std::cerr << "Batch core captured output is malformed." << std::endl;
        return false;
    }

    rawgl::batch::BatchProgress progress = runner.progress();
    if (progress.submittedJobs != 2u || progress.completedJobs != 2u || progress.failedJobs != 0u
        || progress.cancelledJobs != 0u || progress.inFlightJobs != 0u) {
        std::cerr << "Unexpected batch core progress snapshot." << std::endl;
        return false;
    }

    rawgl::batch::BatchCancellationToken cancellation;
    cancellation.cancel();
    rawgl::batch::BatchJobHandle cancelledHandle = runner.submit(*prepareResult.workflow, {}, &cancellation);
    rawgl::batch::BatchResult cancelledResult = cancelledHandle.wait();
    if (!cancelledResult.cancelled) {
        std::cerr << "Batch cancellation token did not cancel job." << std::endl;
        return false;
    }

    progress = runner.progress();
    if (progress.submittedJobs != 3u || progress.completedJobs != 2u || progress.failedJobs != 0u
        || progress.cancelledJobs != 1u || progress.inFlightJobs != 0u) {
        std::cerr << "Unexpected batch core progress snapshot after cancellation." << std::endl;
        return false;
    }

    return true;
}

static bool
verify_io_batch_path()
{
    const std::filesystem::path inputPath = std::filesystem::absolute("tests/inputs/EmptyPresetLUT.png");
    const std::filesystem::path outputPath = std::filesystem::absolute("tests/outputs/rawgl_batch_smoke.png");

    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    rawgl::Workflow workflow;
    workflow.verbosity = 0;

    rawgl::Pass pass;
    pass.programKind = rawgl::ShaderProgramKind::vertfrag;
    pass.sizeX = 64;
    pass.sizeY = 64;

    rawgl::ShaderModuleDefinition fragmentModule;
    fragmentModule.role = rawgl::ShaderModuleRole::fragment;
    fragmentModule.sourceKind = rawgl::ShaderModuleSourceKind::glslText;
    fragmentModule.glslText = R"(#version 450 core
layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 out_color;
uniform sampler2D u_src0;
void main()
{
    out_color = texture(u_src0, UV);
}
)";
    fragmentModule.debugLabel = "rawgl_batch_smoke_fragment";
    pass.shaderModules.push_back(fragmentModule);

    rawgl::InputBinding input;
    input.name = "u_src0";
    input.sourceKind = rawgl::InputSourceKind::textureFile;
    input.texturePath = inputPath.string();
    pass.inputs.push_back(input);

    rawgl::OutputBinding output;
    output.name = "out_color";
    output.path = outputPath.string();
    pass.outputs.push_back(output);

    workflow.passes.push_back(pass);

    rawgl::Session session;
    rawgl::io::IoRuntime ioRuntime;
    rawgl::batch::BatchRunner runner(session, ioRuntime);

    rawgl::batch::BatchPrepareResult prepareResult = runner.prepare(workflow);
    if (!prepareResult.success || !prepareResult.workflow) {
        std::cerr << "Batch IO prepare failed: " << prepareResult.errorMessage << std::endl;
        return false;
    }

    rawgl::batch::BatchJobHandle handle = runner.submit(*prepareResult.workflow);
    rawgl::batch::BatchResult result = handle.wait();

    if (result.cancelled) {
        std::cerr << "Batch IO result unexpectedly cancelled." << std::endl;
        return false;
    }
    if (result.submitIndex != 0u) {
        std::cerr << "Unexpected first IO batch submit index: " << result.submitIndex << std::endl;
        return false;
    }
    if (!result.runResult.success) {
        std::cerr << "Batch IO run failed: " << result.runResult.errorMessage << std::endl;
        return false;
    }
    if (!std::filesystem::exists(outputPath)) {
        std::cerr << "Batch IO run did not write output image." << std::endl;
        return false;
    }

    rawgl::batch::BatchProgress progress = runner.progress();
    if (progress.submittedJobs != 1u || progress.completedJobs != 1u || progress.failedJobs != 0u
        || progress.cancelledJobs != 0u || progress.inFlightJobs != 0u) {
        std::cerr << "Unexpected batch IO progress snapshot." << std::endl;
        return false;
    }

    return true;
}

int
main()
{
    if (!verify_core_batch_path()) {
        return 1;
    }
    if (!verify_io_batch_path()) {
        return 1;
    }
    return 0;
}
