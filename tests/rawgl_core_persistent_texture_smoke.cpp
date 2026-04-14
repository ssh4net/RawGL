// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_core.h"

#include <OpenImageIO/imageio.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <vector>

namespace {

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

rawgl::GraphBuildRequest
make_persistent_graph_request(const std::filesystem::path& outputPath)
{
    rawgl::GraphPassDefinition writePass;
    writePass.programKind              = rawgl::ShaderProgramKind::compute;
    writePass.shaderPaths              = { "tests/shaders/persistent_write.comp" };
    writePass.sizeX                    = 1;
    writePass.sizeY                    = 1;
    writePass.workGroupSizeX           = 1;
    writePass.workGroupSizeY           = 1;
    writePass.hasExplicitWorkGroupSize = true;
    writePass.inputs.push_back(rawgl::GraphInputDefinition {
        "seed",
        rawgl::GraphInputSourceKind::floatValues,
        {},
        {},
        { 0.5f },
        {},
        "",
        "",
        0,
        "",
        {},
    });
    writePass.outputs.push_back(rawgl::GraphOutputDefinition {
        "history_out",
        "",
        "rgba32f",
        4,
        3,
        16,
        "history",
        {},
    });

    rawgl::GraphPassDefinition readPass;
    readPass.programKind              = rawgl::ShaderProgramKind::compute;
    readPass.shaderPaths              = { "tests/shaders/persistent_read.comp" };
    readPass.sizeX                    = 1;
    readPass.sizeY                    = 1;
    readPass.workGroupSizeX           = 1;
    readPass.workGroupSizeY           = 1;
    readPass.hasExplicitWorkGroupSize = true;
    readPass.inputs.push_back(rawgl::GraphInputDefinition {
        "history_in",
        rawgl::GraphInputSourceKind::graphTexture,
        {},
        {},
        {},
        {},
        "",
        "",
        0,
        "history",
        {},
    });
    readPass.outputs.push_back(rawgl::GraphOutputDefinition {
        "o_out0",
        outputPath.string(),
        "rgba32f",
        4,
        3,
        16,
        "",
        {},
    });

    rawgl::GraphBuildRequest request;
    request.definition.verbosity = 0;
    request.definition.passes.push_back(std::move(writePass));
    request.definition.passes.push_back(std::move(readPass));
    return request;
}

}  // namespace

int
main()
{
    const std::filesystem::path outputPath = "tests/outputs/rawgl_core_persistent_texture_smoke.exr";
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    rawgl::RawGLContext context;

    rawgl::GraphBuildResult persistentBuild = context.buildGraph(make_persistent_graph_request(outputPath));
    if (!persistentBuild.success || !persistentBuild.graph) {
        std::cerr << "Persistent graph build failed: " << persistentBuild.errorMessage << std::endl;
        return 1;
    }

    rawgl::GraphExecutionRequest firstRun;
    firstRun.inputOverrides.push_back(rawgl::GraphInputOverride {
        1,
        "history_in",
        rawgl::GraphInputSourceKind::textureFile,
        {},
        {},
        {},
        {},
        "tests/inputs/EmptyPresetLUT.png",
        {},
    });
    firstRun.inputOverrides.push_back(rawgl::GraphInputOverride {
        0,
        "seed",
        rawgl::GraphInputSourceKind::floatValues,
        {},
        {},
        { 0.5f },
        {},
        "",
        {},
    });

    const rawgl::GraphExecutionResult firstResult = persistentBuild.graph->execute(firstRun);
    if (!firstResult.success) {
        std::cerr << "First persistent graph execution failed: " << firstResult.errorMessage << std::endl;
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

    rawgl::GraphExecutionRequest secondRun;
    secondRun.inputOverrides.push_back(rawgl::GraphInputOverride {
        0,
        "seed",
        rawgl::GraphInputSourceKind::floatValues,
        {},
        {},
        { 0.75f },
        {},
        "",
        {},
    });

    const rawgl::GraphExecutionResult secondResult = persistentBuild.graph->execute(secondRun);
    if (!secondResult.success) {
        std::cerr << "Second persistent graph execution failed: " << secondResult.errorMessage << std::endl;
        return 1;
    }
    if (!verify_output(outputPath, 0.5f)) {
        return 1;
    }

    return 0;
}
