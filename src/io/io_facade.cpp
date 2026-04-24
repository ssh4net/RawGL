// SPDX-License-Identifier: Apache-2.0
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
build_addressed_output_name(const FileOutputBinding& output)
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
    stream << build_addressed_output_name(outputSave.output) << "::" << outputSave.output.passIndex;
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
        if (!save_image_output(writeRequest, result.errorMessage)) {
            return result;
        }
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
IoRuntime::materializeWorkflow(const Workflow& workflow,
                               const std::vector<FileInputBinding>& fileInputs,
                               const std::vector<FileOutputBinding>& fileOutputs) const
{
    WorkflowMaterializationResult result;
    result.workflow = workflow;

    try {
        for (const FileInputBinding& fileInput : fileInputs) {
            if (fileInput.passIndex >= result.workflow.passes.size()) {
                result.errorMessage = "file input references an out-of-range pass index";
                return result;
            }

            const ImageLoadResult loadResult = loadImageFile(ImageLoadRequest { fileInput.path, fileInput.attributes });
            if (!loadResult.success) {
                result.errorMessage = loadResult.errorMessage.empty() ? "workflow input materialization failed"
                                                                     : loadResult.errorMessage;
                return result;
            }

            Pass& pass = result.workflow.passes[fileInput.passIndex];
            for (const InputBinding& existingInput : pass.inputs) {
                if (existingInput.name != fileInput.name || existingInput.usesArrayElement != fileInput.usesArrayElement
                    || existingInput.arrayElement != fileInput.arrayElement) {
                    continue;
                }

                result.errorMessage = "file input duplicates an in-memory workflow input";
                return result;
            }

            InputBinding input;
            input.name = fileInput.name;
            input.sourceKind = InputSourceKind::hostTexture;
            input.attributes = fileInput.attributes;
            input.usesArrayElement = fileInput.usesArrayElement;
            input.arrayElement = fileInput.arrayElement;
            input.hostTexture = std::make_shared<HostImageData>(loadResult.image);
            pass.inputs.push_back(std::move(input));
        }

        for (const FileOutputBinding& fileOutput : fileOutputs) {
            if (fileOutput.passIndex >= result.workflow.passes.size()) {
                result.errorMessage = "file output references an out-of-range pass index";
                return result;
            }

            Pass& pass = result.workflow.passes[fileOutput.passIndex];
            bool matchedOutput = false;
            for (OutputBinding& output : pass.outputs) {
                if (output.name != fileOutput.name || output.usesArrayElement != fileOutput.usesArrayElement
                    || output.arrayElement != fileOutput.arrayElement) {
                    continue;
                }

                output.captureToHost = true;
                matchedOutput = true;
                break;
            }

            if (!matchedOutput) {
                OutputBinding output;
                output.name = fileOutput.name;
                output.format = fileOutput.format;
                output.channels = fileOutput.channels;
                output.alphaChannel = fileOutput.alphaChannel;
                output.bits = fileOutput.bits;
                output.attributes = fileOutput.attributes;
                output.captureToHost = true;
                output.usesArrayElement = fileOutput.usesArrayElement;
                output.arrayElement = fileOutput.arrayElement;
                pass.outputs.push_back(std::move(output));
            }

            result.outputSaves.push_back(OutputSaveBinding { fileOutput });
        }
    } catch (const std::exception& exception) {
        result.errorMessage = exception.what();
        return result;
    }

    result.success = true;
    return result;
}

RunSettingsMaterializationResult
IoRuntime::materializeRunSettings(const RunRequest& request) const
{
    RunSettingsMaterializationResult result;
    result.settings = request.settings;

    try {
        for (const FileInputOverride& fileInput : request.fileInputs) {
            const ImageLoadResult loadResult = loadImageFile(ImageLoadRequest { fileInput.path, fileInput.attributes });
            if (!loadResult.success) {
                result.errorMessage = loadResult.errorMessage.empty() ? "run settings materialization failed"
                                                                     : loadResult.errorMessage;
                return result;
            }

            for (const InputOverride& existingOverride : result.settings.overrides) {
                if (existingOverride.passIndex != fileInput.passIndex || existingOverride.name != fileInput.name
                    || existingOverride.usesArrayElement != fileInput.usesArrayElement
                    || existingOverride.arrayElement != fileInput.arrayElement) {
                    continue;
                }

                result.errorMessage = "file input override duplicates an in-memory run override";
                return result;
            }

            InputOverride inputOverride;
            inputOverride.passIndex = fileInput.passIndex;
            inputOverride.name = fileInput.name;
            inputOverride.sourceKind = InputSourceKind::hostTexture;
            inputOverride.attributes = fileInput.attributes;
            inputOverride.usesArrayElement = fileInput.usesArrayElement;
            inputOverride.arrayElement = fileInput.arrayElement;
            inputOverride.hostTexture = std::make_shared<HostImageData>(loadResult.image);
            result.settings.overrides.push_back(std::move(inputOverride));
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
