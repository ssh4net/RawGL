// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_io.h"

#include "output_writer.h"
#include "texture_loader.h"

#include <exception>
#include <map>
#include <sstream>
#include <utility>

namespace rawgl::io {
namespace {

static std::map<std::string, std::string>
to_attribute_map(const std::vector<Attribute>& attributes)
{
    std::map<std::string, std::string> result;
    for (const Attribute& attribute : attributes) {
        result.insert({ attribute.name, attribute.value });
    }
    return result;
}

static std::string
build_addressed_output_name(const OutputBinding& output)
{
    if (!output.usesArrayElement) {
        return output.name;
    }

    std::ostringstream stream;
    stream << output.name << "[" << output.arrayElement << "]";
    return stream.str();
}

static std::string
build_output_capture_key(const OutputSaveBinding& outputSave)
{
    std::ostringstream stream;
    stream << build_addressed_output_name(outputSave.output) << "::" << outputSave.passIndex;
    return stream.str();
}

static ImageSaveResult
save_image_file_impl(const ImageSaveRequest& request)
{
    ImageSaveResult result;

    try {
        OutputWriteRequest writeRequest;
        writeRequest.path         = request.path;
        writeRequest.attributes   = to_attribute_map(request.attributes);
        writeRequest.alphaChannel = request.alphaChannel;
        writeRequest.bits         = request.bits;
        writeRequest.image        = &request.image;
        save_image_output(writeRequest);
        result.success = true;
    } catch (const std::exception& exception) {
        result.errorMessage = exception.what();
    }

    return result;
}

static ImageLoadResult
load_image_file_impl(const ImageLoadRequest& request)
{
    ImageLoadResult result;

    try {
        result.image   = load_host_image_data(request.path, to_attribute_map(request.attributes));
        result.success = true;
    } catch (const std::exception& exception) {
        result.errorMessage = exception.what();
    }

    return result;
}

}  // namespace

IoRuntime::IoRuntime(const IoRuntimeOptions& options)
    : m_options(options)
{
}

ImageLoadResult
IoRuntime::loadImageFile(const ImageLoadRequest& request) const
{
    return load_image_file_impl(request);
}

ImageSaveResult
IoRuntime::saveImageFile(const ImageSaveRequest& request) const
{
    return save_image_file_impl(request);
}

WorkflowMaterializationResult
IoRuntime::materializeWorkflow(const Workflow& workflow) const
{
    WorkflowMaterializationResult result;
    result.workflow = workflow;

    try {
        for (size_t passIndex = 0; passIndex < result.workflow.passes.size(); ++passIndex) {
            Pass& pass = result.workflow.passes[passIndex];

            for (InputBinding& input : pass.inputs) {
                if (input.sourceKind != InputSourceKind::textureFile) {
                    continue;
                }

                const ImageLoadResult loadResult = loadImageFile(ImageLoadRequest { input.texturePath, input.attributes });
                if (!loadResult.success) {
                    result.errorMessage = loadResult.errorMessage.empty() ? "workflow input materialization failed"
                                                                         : loadResult.errorMessage;
                    return result;
                }

                input.sourceKind  = InputSourceKind::hostTexture;
                input.hostTexture = std::make_shared<HostImageData>(loadResult.image);
                input.texturePath.clear();
            }

            for (OutputBinding& output : pass.outputs) {
                if (output.path.empty()) {
                    continue;
                }

                result.outputSaves.push_back(OutputSaveBinding { passIndex, output });
                output.path.clear();
                output.captureToHost = true;
            }
        }
    } catch (const std::exception& exception) {
        result.errorMessage = exception.what();
        return result;
    }

    result.success = true;
    return result;
}

RunSettingsMaterializationResult
IoRuntime::materializeRunSettings(const RunSettings& settings) const
{
    RunSettingsMaterializationResult result;
    result.settings = settings;

    try {
        for (InputOverride& inputOverride : result.settings.overrides) {
            if (inputOverride.sourceKind != InputSourceKind::textureFile) {
                continue;
            }

            const ImageLoadResult loadResult =
                loadImageFile(ImageLoadRequest { inputOverride.texturePath, inputOverride.attributes });
            if (!loadResult.success) {
                result.errorMessage = loadResult.errorMessage.empty() ? "run settings materialization failed"
                                                                     : loadResult.errorMessage;
                return result;
            }

            inputOverride.sourceKind  = InputSourceKind::hostTexture;
            inputOverride.hostTexture = std::make_shared<HostImageData>(loadResult.image);
            inputOverride.texturePath.clear();
        }
    } catch (const std::exception& exception) {
        result.errorMessage = exception.what();
        return result;
    }

    result.success = true;
    return result;
}

ImageSaveResult
IoRuntime::saveCapturedOutput(const OutputSaveBinding& outputSave, const RunResult& result) const
{
    ImageSaveResult saveResult;

    const auto captureIt = result.capturedOutputs.find(build_output_capture_key(outputSave));
    if (captureIt == result.capturedOutputs.end()) {
        saveResult.errorMessage = "captured output not found for save request";
        return saveResult;
    }

    ImageSaveRequest request;
    request.path         = outputSave.output.path;
    request.alphaChannel = outputSave.output.alphaChannel;
    request.bits         = outputSave.output.bits;
    request.image        = captureIt->second;
    request.attributes.reserve(outputSave.output.attributes.size());
    for (const Attribute& attribute : outputSave.output.attributes) {
        request.attributes.push_back(attribute);
    }

    return saveImageFile(request);
}

SaveOutputsResult
IoRuntime::saveCapturedOutputs(const std::vector<OutputSaveBinding>& outputSaves, const RunResult& result) const
{
    SaveOutputsResult saveResult;

    for (const OutputSaveBinding& outputSave : outputSaves) {
        const ImageSaveResult singleSaveResult = saveCapturedOutput(outputSave, result);
        if (!singleSaveResult.success) {
            saveResult.errorMessage = singleSaveResult.errorMessage;
            return saveResult;
        }

        ++saveResult.savedCount;
    }

    saveResult.success = true;
    return saveResult;
}

ImageLoadResult
LoadImageFile(const ImageLoadRequest& request)
{
    return IoRuntime().loadImageFile(request);
}

ImageSaveResult
SaveImageFile(const ImageSaveRequest& request)
{
    return IoRuntime().saveImageFile(request);
}

}  // namespace rawgl::io
