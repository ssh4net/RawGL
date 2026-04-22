// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_io.h"

#include <filesystem>
#include <iostream>

int
main()
{
    const std::filesystem::path inputPath = "tests/inputs/EmptyPresetLUT.png";
    const std::filesystem::path outputPath = "tests/outputs/rawgl_io_host_image_smoke.png";
    const std::filesystem::path materializedOutputPath = "tests/outputs/rawgl_io_materialized_output_smoke.png";

    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);
    std::filesystem::remove(materializedOutputPath, removeError);

    rawgl::io::ImageLoadRequest loadRequest;
    loadRequest.path = inputPath.string();

    const rawgl::io::ImageLoadResult loadResult = rawgl::io::LoadImageFile(loadRequest);
    if (!loadResult.success) {
        std::cerr << "Image load failed: " << loadResult.errorMessage << std::endl;
        return 1;
    }

    rawgl::io::ImageSaveRequest saveRequest;
    saveRequest.path  = outputPath.string();
    saveRequest.bits  = 8;
    saveRequest.image = loadResult.image;

    const rawgl::io::ImageSaveResult saveResult = rawgl::io::SaveImageFile(saveRequest);
    if (!saveResult.success) {
        std::cerr << "Image save failed: " << saveResult.errorMessage << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(outputPath)) {
        std::cerr << "Expected output was not created: " << outputPath << std::endl;
        return 1;
    }

    rawgl::io::ImageLoadRequest reloadRequest;
    reloadRequest.path = outputPath.string();

    const rawgl::io::ImageLoadResult reloadResult = rawgl::io::LoadImageFile(reloadRequest);
    if (!reloadResult.success) {
        std::cerr << "Reload failed: " << reloadResult.errorMessage << std::endl;
        return 1;
    }

    if (reloadResult.image.width != loadResult.image.width || reloadResult.image.height != loadResult.image.height
        || reloadResult.image.channels != loadResult.image.channels) {
        std::cerr << "Reloaded image dimensions differ from source." << std::endl;
        return 1;
    }

    if (reloadResult.image.bytes.empty() || loadResult.image.bytes.empty()) {
        std::cerr << "Loaded image bytes are empty." << std::endl;
        return 1;
    }

    rawgl::io::IoRuntime ioRuntime;

    rawgl::Workflow workflow;
    rawgl::Pass pass;
    workflow.passes.push_back(pass);

    std::vector<rawgl::io::FileInputBinding> fileInputs;
    fileInputs.push_back(rawgl::io::FileTextureInput(0, "u_src0", inputPath.string()));
    std::vector<rawgl::io::FileOutputBinding> fileOutputs;
    fileOutputs.push_back(rawgl::io::FileOutput(0, "out_color", materializedOutputPath.string()));

    const rawgl::io::WorkflowMaterializationResult workflowMaterialization =
        ioRuntime.materializeWorkflow(workflow, fileInputs, fileOutputs);
    if (!workflowMaterialization.success) {
        std::cerr << "Workflow materialization failed: " << workflowMaterialization.errorMessage << std::endl;
        return 1;
    }

    if (workflowMaterialization.workflow.passes.empty()
        || workflowMaterialization.workflow.passes[0].inputs.empty()
        || workflowMaterialization.workflow.passes[0].outputs.empty()) {
        std::cerr << "Materialized workflow is incomplete." << std::endl;
        return 1;
    }

    const rawgl::InputBinding& materializedInput = workflowMaterialization.workflow.passes[0].inputs[0];
    if (materializedInput.sourceKind != rawgl::InputSourceKind::hostTexture || !materializedInput.hostTexture) {
        std::cerr << "Workflow input was not materialized to host texture." << std::endl;
        return 1;
    }

    const rawgl::OutputBinding& materializedOutput = workflowMaterialization.workflow.passes[0].outputs[0];
    if (!materializedOutput.captureToHost) {
        std::cerr << "Workflow output was not rewritten into captured host output." << std::endl;
        return 1;
    }

    if (workflowMaterialization.outputSaves.size() != 1u
        || workflowMaterialization.outputSaves[0].output.path != materializedOutputPath.string()) {
        std::cerr << "Workflow file output save binding was not preserved." << std::endl;
        return 1;
    }

    rawgl::RunResult runResult;
    runResult.success = true;
    runResult.capturedOutputs.insert({ "out_color::0", loadResult.image });

    const rawgl::io::SaveOutputsResult saveOutputsResult =
        ioRuntime.saveCapturedOutputs(workflowMaterialization.outputSaves, runResult);
    if (!saveOutputsResult.success || saveOutputsResult.savedCount != 1u) {
        std::cerr << "Saving captured workflow outputs failed: " << saveOutputsResult.errorMessage << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(materializedOutputPath)) {
        std::cerr << "Materialized workflow output was not saved: " << materializedOutputPath << std::endl;
        return 1;
    }

    rawgl::io::RunRequest runRequest;
    runRequest.fileInputs.push_back(rawgl::io::FileTextureOverride(0, "u_src0", inputPath.string()));

    const rawgl::io::RunSettingsMaterializationResult settingsMaterialization =
        ioRuntime.materializeRunSettings(runRequest);
    if (!settingsMaterialization.success) {
        std::cerr << "Run settings materialization failed: " << settingsMaterialization.errorMessage << std::endl;
        return 1;
    }

    if (settingsMaterialization.settings.overrides.empty()) {
        std::cerr << "Materialized run settings lost overrides." << std::endl;
        return 1;
    }

    const rawgl::InputOverride& materializedOverride = settingsMaterialization.settings.overrides[0];
    if (materializedOverride.sourceKind != rawgl::InputSourceKind::hostTexture || !materializedOverride.hostTexture) {
        std::cerr << "Run settings override was not materialized to host texture." << std::endl;
        return 1;
    }

    return 0;
}
