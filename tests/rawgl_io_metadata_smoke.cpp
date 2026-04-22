// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_io.h"

#include <filesystem>
#include <iostream>

int
main()
{
    const std::filesystem::path inputPath = "tests/inputs/sky.jpg";
    const std::filesystem::path outputPath = "tests/outputs/rawgl_io_metadata_smoke.tif";

    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    rawgl::io::ImageLoadRequest loadRequest;
    loadRequest.path = inputPath.string();

    const rawgl::io::ImageLoadResult loadResult = rawgl::io::LoadImageFile(loadRequest);
    if (!loadResult.success) {
        std::cerr << "Image load failed: " << loadResult.errorMessage << std::endl;
        return 1;
    }

    rawgl::io::ImageSaveRequest saveRequest;
    saveRequest.path  = outputPath.string();
    saveRequest.bits  = 16;
    saveRequest.image = loadResult.image;

    const rawgl::io::ImageSaveResult saveResult = rawgl::io::SaveImageFile(saveRequest);
    if (!saveResult.success) {
        std::cerr << "Metadata smoke save failed: " << saveResult.errorMessage << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(outputPath)) {
        std::cerr << "Metadata smoke output was not created: " << outputPath << std::endl;
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

    bool foundWidth = false;
    bool foundHeight = false;
    bool foundDateTime = false;

    for (const rawgl::io::MetadataEntry& entry : metadataResult.entries) {
        if (entry.name == "ImageWidth" && entry.valueText == std::to_string(loadResult.image.width)) {
            foundWidth = true;
        } else if ((entry.name == "ImageLength" || entry.name == "ImageHeight")
                   && entry.valueText == std::to_string(loadResult.image.height)) {
            foundHeight = true;
        } else if ((entry.name == "DateTime" || entry.name == "ModifyDate") && !entry.valueText.empty()) {
            foundDateTime = true;
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

    if (!foundDateTime) {
        std::cerr << "Date/time metadata entry was not exported." << std::endl;
        return 1;
    }

    return 0;
}
