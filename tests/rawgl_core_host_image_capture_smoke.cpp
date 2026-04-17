// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl.h"

#include <GL/glew.h>

#include <cstring>
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

std::shared_ptr<rawgl::HostImageData>
make_rgba32f_host_image(const float red, const float green, const float blue, const float alpha)
{
    auto hostImage                 = std::make_shared<rawgl::HostImageData>();
    hostImage->width               = 1;
    hostImage->height              = 1;
    hostImage->channels            = 4;
    hostImage->alphaChannel        = 3;
    hostImage->glInternalFormat    = GL_RGBA32F;
    hostImage->glType              = GL_FLOAT;
    const float pixel[4]           = { red, green, blue, alpha };
    hostImage->bytes.resize(sizeof(pixel));
    std::memcpy(hostImage->bytes.data(), pixel, sizeof(pixel));
    return hostImage;
}

bool
read_rgba32f_pixel(const rawgl::HostImageData& hostImage, float pixel[4])
{
    if (hostImage.width != 1 || hostImage.height != 1 || hostImage.channels != 4 || hostImage.alphaChannel != 3
        || hostImage.glInternalFormat != GL_RGBA32F || hostImage.glType != GL_FLOAT
        || hostImage.bytes.size() != sizeof(float) * 4u) {
        return false;
    }

    std::memcpy(pixel, hostImage.bytes.data(), sizeof(float) * 4u);
    return true;
}

bool
verify_captured_output(const rawgl::RunResult& executionResult,
                       const char* outputKey,
                       const float expected[4])
{
    const auto outputIt = executionResult.capturedOutputs.find(outputKey);
    if (outputIt == executionResult.capturedOutputs.end()) {
        std::cerr << "Missing captured output: " << outputKey << std::endl;
        return false;
    }

    float pixel[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (!read_rgba32f_pixel(outputIt->second, pixel)) {
        std::cerr << "Captured output metadata is invalid for " << outputKey << std::endl;
        return false;
    }

    for (int i = 0; i < 4; ++i) {
        if (pixel[i] != expected[i]) {
            std::cerr << "Unexpected captured output pixel in " << outputKey << ": [" << pixel[0] << ", " << pixel[1]
                      << ", " << pixel[2] << ", " << pixel[3] << "]" << std::endl;
            return false;
        }
    }

    return true;
}

bool
verify_captured_counter(const rawgl::RunResult& executionResult,
                        const char* counterKey,
                        const uint32_t expected)
{
    const auto counterIt = executionResult.capturedCounters.find(counterKey);
    if (counterIt == executionResult.capturedCounters.end()) {
        std::cerr << "Missing captured atomic counter: " << counterKey << std::endl;
        return false;
    }
    if (counterIt->second.size() != 1u || counterIt->second[0] != expected) {
        std::cerr << "Unexpected captured atomic counter value in " << counterKey << std::endl;
        return false;
    }

    return true;
}

rawgl::Workflow
make_host_capture_workflow()
{
    rawgl::Pass pass;
    pass.programKind              = rawgl::ShaderProgramKind::compute;
    pass.shaderModules.push_back(make_file_module("tests/shaders/host_image_roundtrip.comp"));
    pass.sizeX                    = 1;
    pass.sizeY                    = 1;
    pass.workGroupSizeX           = 1;
    pass.workGroupSizeY           = 1;
    pass.hasExplicitWorkGroupSize = true;

    rawgl::InputBinding input;
    input.name        = "u_src0";
    input.sourceKind  = rawgl::InputSourceKind::hostTexture;
    input.hostTexture = make_rgba32f_host_image(0.25f, 0.5f, 0.75f, 1.0f);
    pass.inputs.push_back(std::move(input));

    rawgl::CounterBinding counter;
    counter.name         = "counter0";
    counter.initialValue = 7u;
    pass.counters.push_back(std::move(counter));

    rawgl::OutputBinding output;
    output.name          = "o_out0";
    output.format        = "rgba32f";
    output.channels      = 4;
    output.alphaChannel  = 3;
    output.bits          = 16;
    output.captureToHost = true;
    pass.outputs.push_back(std::move(output));

    rawgl::Workflow workflow;
    workflow.verbosity = 0;
    workflow.passes.push_back(std::move(pass));
    return workflow;
}

}  // namespace

int
main()
{
    rawgl::Session session;
    rawgl::PrepareResult prepareResult = session.prepare(make_host_capture_workflow());
    if (!prepareResult.success || !prepareResult.workflow) {
        std::cerr << "Workflow preparation failed: " << prepareResult.errorMessage << std::endl;
        return 1;
    }

    if (session.stats().textures != 0u) {
        std::cerr << "Host-memory textures should not populate the shared file texture cache" << std::endl;
        return 1;
    }

    const rawgl::RunResult defaultRun = prepareResult.workflow->run(rawgl::RunSettings {});
    if (!defaultRun.success) {
        std::cerr << "Default workflow execution failed: " << defaultRun.errorMessage << std::endl;
        return 1;
    }

    const float defaultExpected[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
    if (!verify_captured_output(defaultRun, "o_out0::0", defaultExpected)
        || !verify_captured_counter(defaultRun, "counter0::0", 8u)) {
        return 1;
    }

    rawgl::RunSettings overrideRun;
    rawgl::InputOverride overrideInput;
    overrideInput.passIndex   = 0;
    overrideInput.name        = "u_src0";
    overrideInput.sourceKind  = rawgl::InputSourceKind::hostTexture;
    overrideInput.hostTexture = make_rgba32f_host_image(1.0f, 0.125f, 0.0f, 0.5f);
    overrideRun.overrides.push_back(std::move(overrideInput));

    const rawgl::RunResult overriddenRun = prepareResult.workflow->run(overrideRun);
    if (!overriddenRun.success) {
        std::cerr << "Override workflow execution failed: " << overriddenRun.errorMessage << std::endl;
        return 1;
    }

    const float overrideExpected[4] = { 1.0f, 0.125f, 0.0f, 0.5f };
    if (!verify_captured_output(overriddenRun, "o_out0::0", overrideExpected)
        || !verify_captured_counter(overriddenRun, "counter0::0", 8u)) {
        return 1;
    }

    return 0;
}
