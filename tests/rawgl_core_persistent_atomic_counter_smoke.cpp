// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl.h"

#include <OpenImageIO/imageio.h>

#include <filesystem>
#include <iostream>
#include <memory>
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
read_pixel_rgba32f(const std::filesystem::path& path, float pixel[4])
{
    std::unique_ptr<OIIO::ImageInput> input = OIIO::ImageInput::open(path.string());
    if (!input) {
        return false;
    }

    const OIIO::ImageSpec spec = input->spec();
    if (spec.width <= 0 || spec.nchannels <= 0) {
        return false;
    }

    std::vector<float> scanline(static_cast<size_t>(spec.width) * static_cast<size_t>(spec.nchannels), 0.0f);
    if (!input->read_scanline(0, 0, OIIO::TypeDesc::FLOAT, scanline.data()) || !input->close()) {
        return false;
    }

    pixel[0] = scanline[0];
    pixel[1] = spec.nchannels > 1 ? scanline[1] : pixel[0];
    pixel[2] = spec.nchannels > 2 ? scanline[2] : pixel[0];
    pixel[3] = spec.nchannels > 3 ? scanline[3] : 1.0f;
    return true;
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

rawgl::Workflow
make_persistent_counter_workflow(const std::filesystem::path& outputPath)
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

}  // namespace

int
main()
{
    const std::filesystem::path outputPath = "tests/outputs/rawgl_core_persistent_atomic_counter_smoke.exr";
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    rawgl::Session session;
    rawgl::PrepareResult prepareResult = session.prepare(make_persistent_counter_workflow(outputPath));
    if (!prepareResult.success || !prepareResult.workflow) {
        std::cerr << "Workflow preparation failed: " << prepareResult.errorMessage << std::endl;
        return 1;
    }

    const rawgl::RunResult firstRun = prepareResult.workflow->run(rawgl::RunSettings {});
    if (!firstRun.success) {
        std::cerr << "First workflow execution failed: " << firstRun.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(outputPath, 3.0f)) {
        return 1;
    }

    const rawgl::RunResult secondRun = prepareResult.workflow->run(rawgl::RunSettings {});
    if (!secondRun.success) {
        std::cerr << "Second workflow execution failed: " << secondRun.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(outputPath, 4.0f)) {
        return 1;
    }

    return 0;
}
