// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl.h"

#include <iostream>
#include <cstring>
#include <vector>

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

rawgl::Workflow
make_persistent_counter_workflow()
{
    rawgl::Pass pass;
    pass.programKind              = rawgl::ShaderProgramKind::compute;
    pass.shaderModules.push_back(make_file_module("tests/shaders/persistent_atomic_counter.comp"));
    pass.sizeX                    = 1;
    pass.sizeY                    = 1;
    pass.workGroupSizeX           = 1;
    pass.workGroupSizeY           = 1;
    pass.hasExplicitWorkGroupSize = true;
    rawgl::CounterBinding counter;
    counter.name                  = "counter0";
    counter.initialValue          = 2u;
    counter.persistentCounterName = "graph_counter0";
    pass.counters.push_back(std::move(counter));

    pass.outputs.push_back(rawgl::CapturedOutput("o_out0", "rgba32f", 4, 3, 16));

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
    rawgl::PrepareResult prepareResult = session.prepare(make_persistent_counter_workflow());
    if (!prepareResult.success || !prepareResult.workflow) {
        std::cerr << "Workflow preparation failed: " << prepareResult.errorMessage << std::endl;
        return 1;
    }

    const rawgl::RunResult firstRun = prepareResult.workflow->run(rawgl::RunSettings {});
    if (!firstRun.success) {
        std::cerr << "First workflow execution failed: " << firstRun.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(firstRun, 3.0f)) {
        return 1;
    }

    const rawgl::RunResult secondRun = prepareResult.workflow->run(rawgl::RunSettings {});
    if (!secondRun.success) {
        std::cerr << "Second workflow execution failed: " << secondRun.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(secondRun, 4.0f)) {
        return 1;
    }

    return 0;
}
