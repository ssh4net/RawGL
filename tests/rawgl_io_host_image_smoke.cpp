// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_io.h"

#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>

#if __has_include(<tiffio.h>)
#include <tiffio.h>
#define RAWGL_TEST_HAS_TIFFIO 1
#endif

#if __has_include(<OpenEXR/ImfTiledInputFile.h>)
#include <OpenEXR/ImfTiledInputFile.h>
#define RAWGL_TEST_HAS_OPENEXR 1
#endif

int
main()
{
    const std::filesystem::path inputPath = "tests/inputs/EmptyPresetLUT.png";
    const std::filesystem::path jpegInputPath = "tests/inputs/sky.jpg";
    const std::filesystem::path outputPath = "tests/outputs/rawgl_io_host_image_smoke.png";
    const std::filesystem::path jpegOutputPath = "tests/outputs/rawgl_io_host_image_smoke.jpg";
    const std::filesystem::path tiffOutputPath = "tests/outputs/rawgl_io_host_image_smoke.tif";
    const std::filesystem::path tiffBigOutputPath = "tests/outputs/rawgl_io_host_image_smoke_big.tif";
    const std::filesystem::path exrOutputPath = "tests/outputs/rawgl_io_host_image_smoke.exr";
    const std::filesystem::path materializedOutputPath = "tests/outputs/rawgl_io_materialized_output_smoke.png";

    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);
    std::filesystem::remove(jpegOutputPath, removeError);
    std::filesystem::remove(tiffOutputPath, removeError);
    std::filesystem::remove(tiffBigOutputPath, removeError);
    std::filesystem::remove(exrOutputPath, removeError);
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
    saveRequest.attributes.push_back({ "png:compressionLevel", "0" });
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

    rawgl::io::ImageLoadRequest jpegLoadRequest;
    jpegLoadRequest.path = jpegInputPath.string();

    const rawgl::io::ImageLoadResult jpegLoadResult = rawgl::io::LoadImageFile(jpegLoadRequest);
    if (!jpegLoadResult.success) {
        std::cerr << "JPEG load failed: " << jpegLoadResult.errorMessage << std::endl;
        return 1;
    }

    rawgl::io::ImageSaveRequest jpegSaveRequest;
    jpegSaveRequest.path = jpegOutputPath.string();
    jpegSaveRequest.bits = 8;
    jpegSaveRequest.attributes.push_back({ "jpeg:quality", "100" });
    jpegSaveRequest.attributes.push_back({ "jpeg:progressive", "true" });
    jpegSaveRequest.image = jpegLoadResult.image;

    const rawgl::io::ImageSaveResult jpegSaveResult = rawgl::io::SaveImageFile(jpegSaveRequest);
    if (!jpegSaveResult.success) {
        std::cerr << "JPEG save failed: " << jpegSaveResult.errorMessage << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(jpegOutputPath)) {
        std::cerr << "Expected JPEG output was not created: " << jpegOutputPath << std::endl;
        return 1;
    }

    rawgl::io::ImageLoadRequest jpegReloadRequest;
    jpegReloadRequest.path = jpegOutputPath.string();

    const rawgl::io::ImageLoadResult jpegReloadResult = rawgl::io::LoadImageFile(jpegReloadRequest);
    if (!jpegReloadResult.success) {
        std::cerr << "JPEG reload failed: " << jpegReloadResult.errorMessage << std::endl;
        return 1;
    }

    if (jpegReloadResult.image.width != jpegLoadResult.image.width
        || jpegReloadResult.image.height != jpegLoadResult.image.height) {
        std::cerr << "Reloaded JPEG dimensions differ from source." << std::endl;
        return 1;
    }

    if (jpegReloadResult.image.channels != 3 || jpegReloadResult.image.bytes.empty()) {
        std::cerr << "Reloaded JPEG shape is unexpected." << std::endl;
        return 1;
    }

    rawgl::io::ImageSaveRequest tiffSaveRequest;
    tiffSaveRequest.path = tiffOutputPath.string();
    tiffSaveRequest.bits = 16;
    tiffSaveRequest.attributes.push_back({ "tiff:compression", "ZIP" });
    tiffSaveRequest.attributes.push_back({ "tiff:tiled", "true" });
    tiffSaveRequest.attributes.push_back({ "tiff:tileWidth", "256" });
    tiffSaveRequest.attributes.push_back({ "tiff:tileLength", "128" });
    tiffSaveRequest.image = jpegLoadResult.image;

    const rawgl::io::ImageSaveResult tiffSaveResult = rawgl::io::SaveImageFile(tiffSaveRequest);
    if (!tiffSaveResult.success) {
        std::cerr << "TIFF save failed: " << tiffSaveResult.errorMessage << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(tiffOutputPath)) {
        std::cerr << "Expected TIFF output was not created: " << tiffOutputPath << std::endl;
        return 1;
    }

    rawgl::io::ImageLoadRequest tiffReloadRequest;
    tiffReloadRequest.path = tiffOutputPath.string();

    const rawgl::io::ImageLoadResult tiffReloadResult = rawgl::io::LoadImageFile(tiffReloadRequest);
    if (!tiffReloadResult.success) {
        std::cerr << "TIFF reload failed: " << tiffReloadResult.errorMessage << std::endl;
        return 1;
    }

    if (tiffReloadResult.image.width != jpegLoadResult.image.width
        || tiffReloadResult.image.height != jpegLoadResult.image.height
        || tiffReloadResult.image.channels != 3) {
        std::cerr << "Reloaded TIFF dimensions differ from source." << std::endl;
        return 1;
    }

    const size_t expectedTiffByteCount = static_cast<size_t>(tiffReloadResult.image.width)
                                         * static_cast<size_t>(tiffReloadResult.image.height) * 3u * sizeof(uint16_t);
    if (tiffReloadResult.image.bytes.size() != expectedTiffByteCount) {
        std::cerr << "Reloaded TIFF byte size is unexpected." << std::endl;
        return 1;
    }

#if defined(RAWGL_TEST_HAS_TIFFIO)
    TIFF* tif = TIFFOpen(tiffOutputPath.string().c_str(), "r");
    if (tif == nullptr) {
        std::cerr << "Failed to reopen TIFF for layout verification." << std::endl;
        return 1;
    }

    if (TIFFIsTiled(tif) == 0) {
        TIFFClose(tif);
        std::cerr << "TIFF output is not tiled." << std::endl;
        return 1;
    }

    uint32_t tileWidth = 0u;
    uint32_t tileLength = 0u;
    TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tileWidth);
    TIFFGetField(tif, TIFFTAG_TILELENGTH, &tileLength);
    TIFFClose(tif);

    if (tileWidth != 256u || tileLength != 128u) {
        std::cerr << "TIFF tile geometry is unexpected." << std::endl;
        return 1;
    }
#endif

    rawgl::io::ImageSaveRequest tiffBigSaveRequest;
    tiffBigSaveRequest.path = tiffBigOutputPath.string();
    tiffBigSaveRequest.bits = 32;
    tiffBigSaveRequest.attributes.push_back({ "tiff:compression", "ZIP" });
    tiffBigSaveRequest.attributes.push_back({ "tiff:predictor", "float" });
    tiffBigSaveRequest.attributes.push_back({ "tiff:rowsPerStrip", "17" });
    tiffBigSaveRequest.attributes.push_back({ "tiff:bigTiff", "true" });
    tiffBigSaveRequest.image = jpegLoadResult.image;

    const rawgl::io::ImageSaveResult tiffBigSaveResult = rawgl::io::SaveImageFile(tiffBigSaveRequest);
    if (!tiffBigSaveResult.success) {
        std::cerr << "BigTIFF save failed: " << tiffBigSaveResult.errorMessage << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(tiffBigOutputPath)) {
        std::cerr << "Expected BigTIFF output was not created: " << tiffBigOutputPath << std::endl;
        return 1;
    }

    rawgl::io::ImageLoadRequest tiffBigReloadRequest;
    tiffBigReloadRequest.path = tiffBigOutputPath.string();

    const rawgl::io::ImageLoadResult tiffBigReloadResult = rawgl::io::LoadImageFile(tiffBigReloadRequest);
    if (!tiffBigReloadResult.success) {
        std::cerr << "BigTIFF reload failed: " << tiffBigReloadResult.errorMessage << std::endl;
        return 1;
    }

    if (tiffBigReloadResult.image.width != jpegLoadResult.image.width
        || tiffBigReloadResult.image.height != jpegLoadResult.image.height
        || tiffBigReloadResult.image.channels != 3) {
        std::cerr << "Reloaded BigTIFF dimensions differ from source." << std::endl;
        return 1;
    }

    const size_t expectedBigTiffByteCount = static_cast<size_t>(tiffBigReloadResult.image.width)
                                            * static_cast<size_t>(tiffBigReloadResult.image.height) * 3u
                                            * sizeof(float);
    if (tiffBigReloadResult.image.bytes.size() != expectedBigTiffByteCount) {
        std::cerr << "Reloaded BigTIFF byte size is unexpected." << std::endl;
        return 1;
    }

#if defined(RAWGL_TEST_HAS_TIFFIO)
    TIFF* bigTif = TIFFOpen(tiffBigOutputPath.string().c_str(), "r");
    if (bigTif == nullptr) {
        std::cerr << "Failed to reopen BigTIFF for layout verification." << std::endl;
        return 1;
    }

    if (TIFFIsTiled(bigTif) != 0) {
        TIFFClose(bigTif);
        std::cerr << "BigTIFF output unexpectedly used tiles." << std::endl;
        return 1;
    }

    if (TIFFIsBigTIFF(bigTif) == 0) {
        TIFFClose(bigTif);
        std::cerr << "TIFF output was not written as BigTIFF." << std::endl;
        return 1;
    }

    uint32_t rowsPerStrip = 0u;
    uint16_t predictor = 0u;
    TIFFGetField(bigTif, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip);
    TIFFGetFieldDefaulted(bigTif, TIFFTAG_PREDICTOR, &predictor);
    TIFFClose(bigTif);

    if (rowsPerStrip != 17u) {
        std::cerr << "BigTIFF rows-per-strip is unexpected." << std::endl;
        return 1;
    }

    if (predictor != PREDICTOR_FLOATINGPOINT) {
        std::cerr << "BigTIFF predictor is unexpected." << std::endl;
        return 1;
    }
#endif

    rawgl::io::ImageSaveRequest exrSaveRequest;
    exrSaveRequest.path = exrOutputPath.string();
    exrSaveRequest.bits = 16;
    exrSaveRequest.attributes.push_back({ "openexr:compression", "zip" });
    exrSaveRequest.attributes.push_back({ "openexr:tiled", "true" });
    exrSaveRequest.attributes.push_back({ "openexr:tileWidth", "64" });
    exrSaveRequest.attributes.push_back({ "openexr:tileHeight", "32" });
    exrSaveRequest.image = jpegLoadResult.image;

    const rawgl::io::ImageSaveResult exrSaveResult = rawgl::io::SaveImageFile(exrSaveRequest);
    if (!exrSaveResult.success) {
        std::cerr << "OpenEXR save failed: " << exrSaveResult.errorMessage << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(exrOutputPath)) {
        std::cerr << "Expected OpenEXR output was not created: " << exrOutputPath << std::endl;
        return 1;
    }

    rawgl::io::ImageLoadRequest exrReloadRequest;
    exrReloadRequest.path = exrOutputPath.string();

    const rawgl::io::ImageLoadResult exrReloadResult = rawgl::io::LoadImageFile(exrReloadRequest);
    if (!exrReloadResult.success) {
        std::cerr << "OpenEXR reload failed: " << exrReloadResult.errorMessage << std::endl;
        return 1;
    }

    if (exrReloadResult.image.width != jpegLoadResult.image.width
        || exrReloadResult.image.height != jpegLoadResult.image.height
        || exrReloadResult.image.channels != 3) {
        std::cerr << "Reloaded OpenEXR dimensions differ from source." << std::endl;
        return 1;
    }

    const size_t expectedExrByteCount = static_cast<size_t>(exrReloadResult.image.width)
                                        * static_cast<size_t>(exrReloadResult.image.height) * 3u * sizeof(uint16_t);
    if (exrReloadResult.image.bytes.size() != expectedExrByteCount) {
        std::cerr << "Reloaded OpenEXR byte size is unexpected." << std::endl;
        return 1;
    }

#if defined(RAWGL_TEST_HAS_OPENEXR)
    try {
        OPENEXR_IMF_NAMESPACE::TiledInputFile exrInput(exrOutputPath.string().c_str());
        if (exrInput.levelMode() != OPENEXR_IMF_NAMESPACE::ONE_LEVEL) {
            std::cerr << "OpenEXR tile level mode is unexpected." << std::endl;
            return 1;
        }
        if (exrInput.tileXSize() != 64u || exrInput.tileYSize() != 32u) {
            std::cerr << "OpenEXR tile geometry is unexpected." << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to reopen OpenEXR for tile verification: " << e.what() << std::endl;
        return 1;
    }
#endif

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
