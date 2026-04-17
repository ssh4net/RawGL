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
verify_pixel(const std::filesystem::path& path, const float expected[4])
{
    float pixel[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (!read_pixel_rgba32f(path, pixel)) {
        std::cerr << "Unable to read output image: " << path << std::endl;
        return false;
    }

    for (int i = 0; i < 4; ++i) {
        if (pixel[i] != expected[i]) {
            std::cerr << "Unexpected output pixel in " << path << ": [" << pixel[0] << ", " << pixel[1] << ", "
                      << pixel[2] << ", " << pixel[3] << "]" << std::endl;
            return false;
        }
    }

    return true;
}

rawgl::Workflow
make_numeric_array_workflow(const std::filesystem::path& outputPath)
{
    rawgl::Pass pass;
    pass.programKind              = rawgl::ShaderProgramKind::compute;
    pass.shaderModules.push_back(make_file_module(rawgl::ShaderProgramKind::compute, "tests/shaders/array_element_uniform.comp"));
    pass.sizeX                    = 1;
    pass.sizeY                    = 1;
    pass.workGroupSizeX           = 1;
    pass.workGroupSizeY           = 1;
    pass.hasExplicitWorkGroupSize = true;

    rawgl::InputBinding weights;
    weights.name             = "weights";
    weights.sourceKind       = rawgl::InputSourceKind::floatValues;
    weights.floatValues      = { 0.75f };
    weights.usesArrayElement = true;
    weights.arrayElement     = 3;
    pass.inputs.push_back(std::move(weights));

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

rawgl::Workflow
make_output_array_workflow(const std::filesystem::path& outputPath)
{
    rawgl::Pass sourcePass;
    sourcePass.programKind = rawgl::ShaderProgramKind::vertfrag;
    rawgl::ShaderModuleDefinition vertexModule;
    vertexModule.role       = rawgl::ShaderModuleRole::vertex;
    vertexModule.sourceKind = rawgl::ShaderModuleSourceKind::filePath;
    vertexModule.path       = "tests/shaders/empty.vert";
    sourcePass.shaderModules.push_back(std::move(vertexModule));
    rawgl::ShaderModuleDefinition fragmentModule;
    fragmentModule.role       = rawgl::ShaderModuleRole::fragment;
    fragmentModule.sourceKind = rawgl::ShaderModuleSourceKind::filePath;
    fragmentModule.path       = "tests/shaders/output_array_dual.frag";
    sourcePass.shaderModules.push_back(std::move(fragmentModule));
    sourcePass.sizeX       = 1;
    sourcePass.sizeY       = 1;

    rawgl::OutputBinding sourceOutput;
    sourceOutput.name             = "OutArray";
    sourceOutput.format           = "rgba32f";
    sourceOutput.channels         = 4;
    sourceOutput.alphaChannel     = 3;
    sourceOutput.bits             = 16;
    sourceOutput.usesArrayElement = true;
    sourceOutput.arrayElement     = 1;
    sourcePass.outputs.push_back(std::move(sourceOutput));

    rawgl::Pass consumePass;
    consumePass.programKind              = rawgl::ShaderProgramKind::compute;
    consumePass.shaderModules.push_back(make_file_module(rawgl::ShaderProgramKind::compute, "tests/shaders/image_chain_consume.comp"));
    consumePass.sizeX                    = 1;
    consumePass.sizeY                    = 1;
    consumePass.workGroupSizeX           = 1;
    consumePass.workGroupSizeY           = 1;
    consumePass.hasExplicitWorkGroupSize = true;

    rawgl::InputBinding input;
    input.name                            = "u_mid0";
    input.sourceKind                      = rawgl::InputSourceKind::passOutput;
    input.referencedOutputName            = "OutArray";
    input.referencedPassIndex             = 0;
    input.usesReferencedOutputArrayElement = true;
    input.referencedOutputArrayElement     = 1;
    consumePass.inputs.push_back(std::move(input));

    rawgl::OutputBinding output;
    output.name         = "o_out0";
    output.path         = outputPath.string();
    output.format       = "rgba32f";
    output.channels     = 4;
    output.alphaChannel = 3;
    output.bits         = 16;
    consumePass.outputs.push_back(std::move(output));

    rawgl::Workflow workflow;
    workflow.verbosity = 0;
    workflow.passes.push_back(std::move(sourcePass));
    workflow.passes.push_back(std::move(consumePass));
    return workflow;
}

rawgl::Workflow
make_atomic_array_workflow(const std::filesystem::path& outputPath)
{
    rawgl::Pass pass;
    pass.programKind              = rawgl::ShaderProgramKind::compute;
    pass.shaderModules.push_back(make_file_module(rawgl::ShaderProgramKind::compute, "tests/shaders/atomic_counter_array.comp"));
    pass.sizeX                    = 1;
    pass.sizeY                    = 1;
    pass.workGroupSizeX           = 1;
    pass.workGroupSizeY           = 1;
    pass.hasExplicitWorkGroupSize = true;

    rawgl::CounterBinding counter;
    counter.name             = "counter0";
    counter.initialValue     = 4u;
    counter.usesArrayElement = true;
    counter.arrayElement     = 1;
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

bool
execute_and_verify(rawgl::Session& session,
                   const rawgl::Workflow& workflow,
                   const std::filesystem::path& outputPath,
                   const float expected[4],
                   const char* label)
{
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    rawgl::PrepareResult prepareResult = session.prepare(workflow);
    if (!prepareResult.success || !prepareResult.workflow) {
        std::cerr << label << " preparation failed: " << prepareResult.errorMessage << std::endl;
        return false;
    }

    const rawgl::RunResult runResult = prepareResult.workflow->run(rawgl::RunSettings {});
    if (!runResult.success) {
        std::cerr << label << " execution failed: " << runResult.errorMessage << std::endl;
        return false;
    }

    return verify_pixel(outputPath, expected);
}

}  // namespace

int
main()
{
    rawgl::Session session;

    const std::filesystem::path numericOutputPath = "tests/outputs/rawgl_core_array_element_uniform_smoke.exr";
    const float numericExpected[4]               = { 0.75f, 0.75f, 0.75f, 1.0f };
    std::error_code removeError;
    std::filesystem::remove(numericOutputPath, removeError);

    rawgl::PrepareResult numericPrepare = session.prepare(make_numeric_array_workflow(numericOutputPath));
    if (!numericPrepare.success || !numericPrepare.workflow) {
        std::cerr << "numeric array element preparation failed: " << numericPrepare.errorMessage << std::endl;
        return 1;
    }

    const rawgl::RunResult numericDefaultRun = numericPrepare.workflow->run(rawgl::RunSettings {});
    if (!numericDefaultRun.success) {
        std::cerr << "numeric array element execution failed: " << numericDefaultRun.errorMessage << std::endl;
        return 1;
    }
    if (!verify_pixel(numericOutputPath, numericExpected)) {
        return 1;
    }

    rawgl::RunSettings numericOverrideRequest;
    rawgl::InputOverride numericOverride;
    numericOverride.passIndex         = 0;
    numericOverride.name              = "weights";
    numericOverride.sourceKind        = rawgl::InputSourceKind::floatValues;
    numericOverride.floatValues       = { 0.5f };
    numericOverride.usesArrayElement  = true;
    numericOverride.arrayElement      = 3;
    numericOverrideRequest.overrides.push_back(std::move(numericOverride));

    const rawgl::RunResult numericOverrideRun = numericPrepare.workflow->run(numericOverrideRequest);
    if (!numericOverrideRun.success) {
        std::cerr << "numeric array element override execution failed: " << numericOverrideRun.errorMessage
                  << std::endl;
        return 1;
    }

    const float numericOverrideExpected[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
    if (!verify_pixel(numericOutputPath, numericOverrideExpected)) {
        return 1;
    }

    const float outputExpected[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    if (!execute_and_verify(session,
                            make_output_array_workflow("tests/outputs/rawgl_core_output_array_element_smoke.exr"),
                            "tests/outputs/rawgl_core_output_array_element_smoke.exr",
                            outputExpected,
                            "output array element")) {
        return 1;
    }

    const float atomicExpected[4] = { 0.0f, 5.0f, 0.0f, 1.0f };
    if (!execute_and_verify(session,
                            make_atomic_array_workflow("tests/outputs/rawgl_core_atomic_array_element_smoke.exr"),
                            "tests/outputs/rawgl_core_atomic_array_element_smoke.exr",
                            atomicExpected,
                            "atomic counter array element")) {
        return 1;
    }

    return 0;
}
