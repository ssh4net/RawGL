// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "../src/io/metadata_internal.h"
#include "rawgl/rawgl_io.h"

#include <filesystem>
#include <iostream>

namespace {

static bool
find_entry_value(const std::vector<rawgl::io::MetadataEntry>& entries, const std::string& name, std::string* value)
{
    for (const rawgl::io::MetadataEntry& entry : entries) {
        if (entry.name != name || entry.valueText.empty()) {
            continue;
        }

        *value = entry.valueText;
        return true;
    }

    return false;
}

}  // namespace

int
main()
{
    const std::filesystem::path inputPath = "tests/inputs/sky.jpg";
    const std::filesystem::path tiffOutputPath = "tests/outputs/rawgl_io_metadata_smoke.tif";
    const std::filesystem::path jpegOutputPath = "tests/outputs/rawgl_io_metadata_smoke.jpg";
    const std::filesystem::path pngOutputPath = "tests/outputs/rawgl_io_metadata_smoke.png";
    const std::filesystem::path exrOutputPath = "tests/outputs/rawgl_io_metadata_smoke.exr";

    std::error_code removeError;
    std::filesystem::remove(tiffOutputPath, removeError);
    std::filesystem::remove(jpegOutputPath, removeError);
    std::filesystem::remove(pngOutputPath, removeError);
    std::filesystem::remove(exrOutputPath, removeError);

    rawgl::io::ImageLoadRequest loadRequest;
    loadRequest.path = inputPath.string();

    const rawgl::io::ImageLoadResult loadResult = rawgl::io::LoadImageFile(loadRequest);
    if (!loadResult.success) {
        std::cerr << "Image load failed: " << loadResult.errorMessage << std::endl;
        return 1;
    }

    rawgl::io::MetadataDocumentReadRequest documentRequest;
    documentRequest.path = inputPath.string();
    documentRequest.nameStyle = rawgl::io::MetadataNameStyle::Oiio;
    documentRequest.namePolicy = rawgl::io::MetadataNamePolicy::ExifToolAlias;

    const rawgl::io::MetadataDocumentReadResult documentResult = rawgl::io::ReadMetadataDocumentFile(documentRequest);
    if (!documentResult.success) {
        std::cerr << "Typed metadata read failed: " << documentResult.errorMessage << std::endl;
        return 1;
    }

    if (documentResult.document.fields.empty()) {
        std::cerr << "Typed metadata read returned no fields." << std::endl;
        return 1;
    }

    rawgl::io::MetadataReadRequest metadataRequest;
    metadataRequest.path = inputPath.string();
    metadataRequest.nameStyle = rawgl::io::MetadataNameStyle::Oiio;
    metadataRequest.namePolicy = rawgl::io::MetadataNamePolicy::ExifToolAlias;

    const rawgl::io::MetadataReadResult metadataResult = rawgl::io::ReadMetadataFile(metadataRequest);
    if (!metadataResult.success) {
        std::cerr << "Metadata read failed: " << metadataResult.errorMessage << std::endl;
        return 1;
    }

    if (metadataResult.entries.empty()) {
        std::cerr << "Metadata read returned no entries." << std::endl;
        return 1;
    }

    std::string sourceMake;
    std::string sourceModel;
    std::string sourceDateTimeOriginal;
    if (!find_entry_value(metadataResult.entries, "Make", &sourceMake)) {
        std::cerr << "Source Make metadata entry was not found." << std::endl;
        return 1;
    }
    if (!find_entry_value(metadataResult.entries, "Model", &sourceModel)) {
        std::cerr << "Source Model metadata entry was not found." << std::endl;
        return 1;
    }
    if (!find_entry_value(metadataResult.entries, "DateTimeOriginal", &sourceDateTimeOriginal)
        && !find_entry_value(metadataResult.entries, "Exif:DateTimeOriginal", &sourceDateTimeOriginal)) {
        std::cerr << "Source DateTimeOriginal metadata entry was not found." << std::endl;
        return 1;
    }

    rawgl::io::ImageSaveRequest tiffSaveRequest;
    tiffSaveRequest.path  = tiffOutputPath.string();
    tiffSaveRequest.bits  = 16;
    tiffSaveRequest.image = loadResult.image;

    const rawgl::io::ImageSaveResult tiffSaveResult = rawgl::io::SaveImageFile(tiffSaveRequest);
    if (!tiffSaveResult.success) {
        std::cerr << "TIFF metadata smoke save failed: " << tiffSaveResult.errorMessage << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(tiffOutputPath)) {
        std::cerr << "TIFF metadata smoke output was not created: " << tiffOutputPath << std::endl;
        return 1;
    }

    const rawgl::io::ImageMetadataApplyResult tiffPatchResult =
        rawgl::io::apply_source_metadata_to_tiff_file_impl(documentResult.document, tiffOutputPath.string());
    if (!tiffPatchResult.success) {
        std::cerr << "TIFF metadata patch failed: " << tiffPatchResult.errorMessage << std::endl;
        return 1;
    }

    metadataRequest.path = tiffOutputPath.string();
    const rawgl::io::MetadataReadResult tiffOutputMetadataResult = rawgl::io::ReadMetadataFile(metadataRequest);
    if (!tiffOutputMetadataResult.success) {
        std::cerr << "Patched TIFF metadata read failed: " << tiffOutputMetadataResult.errorMessage << std::endl;
        return 1;
    }

    bool foundWidth = false;
    bool foundHeight = false;
    bool foundMake = false;
    bool foundModel = false;
    bool foundDateTimeOriginal = false;

    for (const rawgl::io::MetadataEntry& entry : tiffOutputMetadataResult.entries) {
        if (entry.name == "ImageWidth" && entry.valueText == std::to_string(loadResult.image.width)) {
            foundWidth = true;
        } else if ((entry.name == "ImageLength" || entry.name == "ImageHeight")
                   && entry.valueText == std::to_string(loadResult.image.height)) {
            foundHeight = true;
        } else if ((entry.name == "Make" || entry.name == "Exif:Make") && entry.valueText == sourceMake) {
            foundMake = true;
        } else if ((entry.name == "Model" || entry.name == "Exif:Model") && entry.valueText == sourceModel) {
            foundModel = true;
        } else if ((entry.name == "DateTimeOriginal" || entry.name == "Exif:DateTimeOriginal")
                   && entry.valueText == sourceDateTimeOriginal) {
            foundDateTimeOriginal = true;
        }
    }

    if (!foundWidth) {
        std::cerr << "ImageWidth metadata entry was not found or had the wrong value." << std::endl;
        return 1;
    }

    if (!foundHeight) {
        std::cerr << "Image height metadata entry was not found or had the wrong value." << std::endl;
        return 1;
    }

    if (!foundMake) {
        std::cerr << "Make metadata entry was not preserved into TIFF." << std::endl;
        return 1;
    }

    if (!foundModel) {
        std::cerr << "Model metadata entry was not preserved into TIFF." << std::endl;
        return 1;
    }

    if (!foundDateTimeOriginal) {
        std::cerr << "DateTimeOriginal metadata entry was not preserved into TIFF." << std::endl;
        return 1;
    }

    rawgl::io::ImageSaveRequest jpegSaveRequest;
    jpegSaveRequest.path  = jpegOutputPath.string();
    jpegSaveRequest.bits  = 8;
    jpegSaveRequest.image = loadResult.image;

    const rawgl::io::ImageSaveResult jpegSaveResult = rawgl::io::SaveImageFile(jpegSaveRequest);
    if (!jpegSaveResult.success) {
        std::cerr << "JPEG metadata smoke save failed: " << jpegSaveResult.errorMessage << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(jpegOutputPath)) {
        std::cerr << "JPEG metadata smoke output was not created: " << jpegOutputPath << std::endl;
        return 1;
    }

    const rawgl::io::ImageMetadataApplyResult jpegPatchResult =
        rawgl::io::apply_source_metadata_to_jpeg_file_impl(documentResult.document, jpegOutputPath.string());
    if (!jpegPatchResult.success) {
        std::cerr << "JPEG metadata patch failed: " << jpegPatchResult.errorMessage << std::endl;
        return 1;
    }

    metadataRequest.path = jpegOutputPath.string();
    const rawgl::io::MetadataReadResult jpegOutputMetadataResult = rawgl::io::ReadMetadataFile(metadataRequest);
    if (!jpegOutputMetadataResult.success) {
        std::cerr << "Patched JPEG metadata read failed: " << jpegOutputMetadataResult.errorMessage << std::endl;
        return 1;
    }

    bool foundJpegMake = false;
    bool foundJpegModel = false;
    bool foundJpegDateTimeOriginal = false;

    for (const rawgl::io::MetadataEntry& entry : jpegOutputMetadataResult.entries) {
        if ((entry.name == "Make" || entry.name == "Exif:Make") && entry.valueText == sourceMake) {
            foundJpegMake = true;
        } else if ((entry.name == "Model" || entry.name == "Exif:Model") && entry.valueText == sourceModel) {
            foundJpegModel = true;
        } else if ((entry.name == "DateTimeOriginal" || entry.name == "Exif:DateTimeOriginal")
                   && entry.valueText == sourceDateTimeOriginal) {
            foundJpegDateTimeOriginal = true;
        }
    }

    if (!foundJpegMake) {
        std::cerr << "Make metadata entry was not preserved into JPEG." << std::endl;
        return 1;
    }

    if (!foundJpegModel) {
        std::cerr << "Model metadata entry was not preserved into JPEG." << std::endl;
        return 1;
    }

    if (!foundJpegDateTimeOriginal) {
        std::cerr << "DateTimeOriginal metadata entry was not preserved into JPEG." << std::endl;
        return 1;
    }

    rawgl::io::ImageSaveRequest pngSaveRequest;
    pngSaveRequest.path  = pngOutputPath.string();
    pngSaveRequest.bits  = 8;
    pngSaveRequest.image = loadResult.image;

    const rawgl::io::ImageSaveResult pngSaveResult = rawgl::io::SaveImageFile(pngSaveRequest);
    if (!pngSaveResult.success) {
        std::cerr << "PNG metadata smoke save failed: " << pngSaveResult.errorMessage << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(pngOutputPath)) {
        std::cerr << "PNG metadata smoke output was not created: " << pngOutputPath << std::endl;
        return 1;
    }

    const rawgl::io::ImageMetadataApplyResult pngPatchResult =
        rawgl::io::apply_source_metadata_to_png_file_impl(documentResult.document, pngOutputPath.string());
    if (!pngPatchResult.success) {
        std::cerr << "PNG metadata patch failed: " << pngPatchResult.errorMessage << std::endl;
        return 1;
    }

    metadataRequest.path = pngOutputPath.string();
    const rawgl::io::MetadataReadResult pngOutputMetadataResult = rawgl::io::ReadMetadataFile(metadataRequest);
    if (!pngOutputMetadataResult.success) {
        std::cerr << "Patched PNG metadata read failed: " << pngOutputMetadataResult.errorMessage << std::endl;
        return 1;
    }

    bool foundPngMake = false;
    bool foundPngModel = false;
    bool foundPngDateTimeOriginal = false;

    for (const rawgl::io::MetadataEntry& entry : pngOutputMetadataResult.entries) {
        if ((entry.name == "Make" || entry.name == "Exif:Make") && entry.valueText == sourceMake) {
            foundPngMake = true;
        } else if ((entry.name == "Model" || entry.name == "Exif:Model") && entry.valueText == sourceModel) {
            foundPngModel = true;
        } else if ((entry.name == "DateTimeOriginal" || entry.name == "Exif:DateTimeOriginal")
                   && entry.valueText == sourceDateTimeOriginal) {
            foundPngDateTimeOriginal = true;
        }
    }

    if (!foundPngMake) {
        std::cerr << "Make metadata entry was not preserved into PNG." << std::endl;
        return 1;
    }

    if (!foundPngModel) {
        std::cerr << "Model metadata entry was not preserved into PNG." << std::endl;
        return 1;
    }

    if (!foundPngDateTimeOriginal) {
        std::cerr << "DateTimeOriginal metadata entry was not preserved into PNG." << std::endl;
        return 1;
    }

    rawgl::io::ImageSaveRequest exrSaveRequest;
    exrSaveRequest.path  = exrOutputPath.string();
    exrSaveRequest.bits  = 16;
    exrSaveRequest.image = loadResult.image;

    const rawgl::io::ImageSaveResult exrSaveResult = rawgl::io::SaveImageFile(exrSaveRequest);
    if (!exrSaveResult.success) {
        std::cerr << "EXR metadata smoke save failed: " << exrSaveResult.errorMessage << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(exrOutputPath)) {
        std::cerr << "EXR metadata smoke output was not created: " << exrOutputPath << std::endl;
        return 1;
    }

    const rawgl::io::ImageMetadataApplyResult exrPatchResult =
        rawgl::io::apply_source_metadata_to_exr_file_impl(documentResult.document, exrOutputPath.string());
    if (!exrPatchResult.success) {
        std::cerr << "EXR metadata patch failed: " << exrPatchResult.errorMessage << std::endl;
        return 1;
    }

    metadataRequest.path = exrOutputPath.string();
    const rawgl::io::MetadataReadResult exrOutputMetadataResult = rawgl::io::ReadMetadataFile(metadataRequest);
    if (!exrOutputMetadataResult.success) {
        std::cerr << "Patched EXR metadata read failed: " << exrOutputMetadataResult.errorMessage << std::endl;
        return 1;
    }

    bool foundExrMake = false;
    bool foundExrModel = false;
    bool foundExrDateTimeOriginal = false;

    for (const rawgl::io::MetadataEntry& entry : exrOutputMetadataResult.entries) {
        if ((entry.name == "Make" || entry.name == "openexr:Make") && entry.valueText == sourceMake) {
            foundExrMake = true;
        } else if ((entry.name == "Model" || entry.name == "openexr:Model") && entry.valueText == sourceModel) {
            foundExrModel = true;
        } else if ((entry.name == "DateTimeOriginal"
                    || entry.name == "Exif:DateTimeOriginal"
                    || entry.name == "openexr:DateTimeOriginal"
                    || entry.name == "openexr:Exif:DateTimeOriginal")
                   && entry.valueText == sourceDateTimeOriginal) {
            foundExrDateTimeOriginal = true;
        }
    }

    if (!foundExrMake) {
        std::cerr << "Make metadata entry was not preserved into EXR." << std::endl;
        return 1;
    }

    if (!foundExrModel) {
        std::cerr << "Model metadata entry was not preserved into EXR." << std::endl;
        return 1;
    }

    if (!foundExrDateTimeOriginal) {
        std::cerr << "DateTimeOriginal metadata entry was not preserved into EXR." << std::endl;
        return 1;
    }

    return 0;
}
