// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl.h"
#include "rawgl/rawgl_io.h"

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
read_first_channel_float(const std::filesystem::path& path, float& value)
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
    if (!input->read_scanline(0, 0, OIIO::TypeDesc::FLOAT, scanline.data())) {
        return false;
    }

    value = scanline[0];
    return input->close();
}

bool
verify_output(const std::filesystem::path& outputPath, float expectedValue)
{
    std::unique_ptr<OIIO::ImageInput> input = OIIO::ImageInput::open(outputPath.string());
    if (!input) {
        std::cerr << "Unable to read output image: " << outputPath << std::endl;
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

    float pixel[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    pixel[0]       = scanline[0];
    pixel[1]       = spec.nchannels > 1 ? scanline[1] : pixel[0];
    pixel[2]       = spec.nchannels > 2 ? scanline[2] : pixel[0];
    pixel[3]       = spec.nchannels > 3 ? scanline[3] : 1.0f;

    if (pixel[0] != expectedValue || pixel[1] != expectedValue || pixel[2] != expectedValue || pixel[3] != 1.0f) {
        std::cerr << "Unexpected output pixel in " << outputPath << ": [" << pixel[0] << ", " << pixel[1] << ", "
                  << pixel[2] << ", " << pixel[3] << "]" << std::endl;
        return false;
    }

    return true;
}

struct IoWorkflowCase {
    rawgl::Workflow workflow;
    std::vector<rawgl::io::FileOutputBinding> fileOutputs;
};

IoWorkflowCase
make_persistent_workflow(const std::filesystem::path& outputPath)
{
    IoWorkflowCase result;
    rawgl::Pass writePass;
    writePass.programKind              = rawgl::ShaderProgramKind::compute;
    writePass.shaderModules.push_back(make_file_module(rawgl::ShaderProgramKind::compute, "tests/shaders/persistent_write.comp"));
    writePass.sizeX                    = 1;
    writePass.sizeY                    = 1;
    writePass.workGroupSizeX           = 1;
    writePass.workGroupSizeY           = 1;
    writePass.hasExplicitWorkGroupSize = true;
    rawgl::InputBinding seed;
    seed.name        = "seed";
    seed.sourceKind  = rawgl::InputSourceKind::floatValues;
    seed.floatValues = { 0.5f };
    writePass.inputs.push_back(std::move(seed));

    rawgl::OutputBinding historyOutput;
    historyOutput.name                  = "history_out";
    historyOutput.format                = "rgba32f";
    historyOutput.channels              = 4;
    historyOutput.alphaChannel          = 3;
    historyOutput.bits                  = 16;
    historyOutput.persistentTextureName = "history";
    writePass.outputs.push_back(std::move(historyOutput));

    rawgl::Pass readPass;
    readPass.programKind              = rawgl::ShaderProgramKind::compute;
    readPass.shaderModules.push_back(make_file_module(rawgl::ShaderProgramKind::compute, "tests/shaders/persistent_read.comp"));
    readPass.sizeX                    = 1;
    readPass.sizeY                    = 1;
    readPass.workGroupSizeX           = 1;
    readPass.workGroupSizeY           = 1;
    readPass.hasExplicitWorkGroupSize = true;
    rawgl::InputBinding historyInput;
    historyInput.name             = "history_in";
    historyInput.sourceKind       = rawgl::InputSourceKind::workflowTexture;
    historyInput.workflowTextureName = "history";
    readPass.inputs.push_back(std::move(historyInput));

    result.workflow.verbosity = 0;
    result.workflow.passes.push_back(std::move(writePass));
    result.workflow.passes.push_back(std::move(readPass));
    result.fileOutputs.push_back(rawgl::io::FileOutput(1, "o_out0", outputPath.string(), "rgba32f", 4, 3, 16));
    return result;
}

}  // namespace

int
main()
{
    const std::filesystem::path outputPath = "tests/outputs/rawgl_core_persistent_texture_smoke.exr";
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    rawgl::Session session;
    rawgl::io::IoRuntime ioRuntime;

    const IoWorkflowCase workflowCase = make_persistent_workflow(outputPath);
    rawgl::io::PrepareWorkflowResult prepareResult =
        ioRuntime.prepare(session, workflowCase.workflow, {}, workflowCase.fileOutputs);
    if (!prepareResult.success || !prepareResult.workflow) {
        std::cerr << "Persistent workflow preparation failed: " << prepareResult.errorMessage << std::endl;
        return 1;
    }

    rawgl::io::RunRequest firstRun;
    firstRun.fileInputs.push_back(rawgl::io::FileTextureOverride(1, "history_in", "tests/inputs/EmptyPresetLUT.png"));

    rawgl::InputOverride seedOverride;
    seedOverride.passIndex   = 0;
    seedOverride.name        = "seed";
    seedOverride.sourceKind  = rawgl::InputSourceKind::floatValues;
    seedOverride.floatValues = { 0.5f };
    firstRun.settings.overrides.push_back(std::move(seedOverride));

    const rawgl::RunResult firstResult = prepareResult.workflow->run(firstRun);
    if (!firstResult.success) {
        std::cerr << "First persistent workflow execution failed: " << firstResult.errorMessage << std::endl;
        return 1;
    }
    float seedValue = 0.0f;
    if (!read_first_channel_float("tests/inputs/EmptyPresetLUT.png", seedValue)) {
        std::cerr << "Unable to read PNG seed texture." << std::endl;
        return 1;
    }
    if (!verify_output(outputPath, seedValue)) {
        return 1;
    }

    rawgl::io::RunRequest secondRun;
    rawgl::InputOverride secondSeedOverride;
    secondSeedOverride.passIndex   = 0;
    secondSeedOverride.name        = "seed";
    secondSeedOverride.sourceKind  = rawgl::InputSourceKind::floatValues;
    secondSeedOverride.floatValues = { 0.75f };
    secondRun.settings.overrides.push_back(std::move(secondSeedOverride));

    const rawgl::RunResult secondResult = prepareResult.workflow->run(secondRun);
    if (!secondResult.success) {
        std::cerr << "Second persistent workflow execution failed: " << secondResult.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(outputPath, 0.5f)) {
        return 1;
    }

    return 0;
}
