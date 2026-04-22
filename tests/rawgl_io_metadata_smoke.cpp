// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_io.h"

#include <filesystem>
#include <iostream>

int
main()
{
    const std::filesystem::path inputPath = "tests/inputs/EmptyPresetLUT.png";
    const std::filesystem::path outputPath = "tests/outputs/rawgl_io_metadata_smoke.tif";
    const std::filesystem::path copiedPath = "tests/outputs/rawgl_io_metadata_copy_smoke.tif";
    const std::filesystem::path explicitPath = "tests/outputs/rawgl_io_metadata_explicit_smoke.tif";

    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);
    std::filesystem::remove(copiedPath, removeError);
    std::filesystem::remove(explicitPath, removeError);

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
    documentRequest.path = outputPath.string();
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
    metadataRequest.path = outputPath.string();
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

    rawgl::io::ImageSaveRequest copiedSaveRequest;
    copiedSaveRequest.path = copiedPath.string();
    copiedSaveRequest.bits = 16;
    copiedSaveRequest.image = loadResult.image;
    copiedSaveRequest.metadataMode = rawgl::io::MetadataTransferMode::CopySource;
    copiedSaveRequest.sourceMetadata = std::make_shared<rawgl::io::MetadataDocument>(documentResult.document);

    const rawgl::io::ImageSaveResult copiedSaveResult = rawgl::io::SaveImageFile(copiedSaveRequest);
    if (!copiedSaveResult.success) {
        std::cerr << "Copy-source metadata save failed: " << copiedSaveResult.errorMessage << std::endl;
        return 1;
    }

    rawgl::io::MetadataReadRequest copiedMetadataRequest;
    copiedMetadataRequest.path = copiedPath.string();
    copiedMetadataRequest.nameStyle = rawgl::io::MetadataNameStyle::Oiio;
    copiedMetadataRequest.namePolicy = rawgl::io::MetadataNamePolicy::ExifToolAlias;
    const rawgl::io::MetadataReadResult copiedMetadataResult = rawgl::io::ReadMetadataFile(copiedMetadataRequest);
    if (!copiedMetadataResult.success) {
        std::cerr << "Copied metadata read failed: " << copiedMetadataResult.errorMessage << std::endl;
        return 1;
    }

    bool copiedDateTime = false;
    for (const rawgl::io::MetadataEntry& entry : copiedMetadataResult.entries) {
        if ((entry.name == "DateTime" || entry.name == "ModifyDate") && !entry.valueText.empty()) {
            copiedDateTime = true;
        }
    }

    if (!copiedDateTime) {
        std::cerr << "Copy-source save did not preserve a date/time metadata entry." << std::endl;
        return 1;
    }

    auto explicitDocument = std::make_shared<rawgl::io::MetadataDocument>();
    rawgl::io::MetadataField softwareField;
    softwareField.keyKind = rawgl::io::MetadataKeyKind::ExifTag;
    softwareField.name = "Software";
    softwareField.value.kind = rawgl::io::MetadataValueKind::Text;
    softwareField.value.textEncoding = rawgl::io::MetadataTextEncoding::Utf8;
    const std::string softwareValue = "RawGL metadata smoke";
    softwareField.value.count = static_cast<uint32_t>(softwareValue.size());
    softwareField.value.bytes.assign(reinterpret_cast<const std::byte*>(softwareValue.data()),
                                     reinterpret_cast<const std::byte*>(softwareValue.data() + softwareValue.size()));
    explicitDocument->fields.push_back(std::move(softwareField));

    rawgl::io::ImageSaveRequest explicitSaveRequest;
    explicitSaveRequest.path = explicitPath.string();
    explicitSaveRequest.bits = 16;
    explicitSaveRequest.image = loadResult.image;
    explicitSaveRequest.metadataMode = rawgl::io::MetadataTransferMode::ExplicitOnly;
    explicitSaveRequest.explicitMetadata = explicitDocument;

    const rawgl::io::ImageSaveResult explicitSaveResult = rawgl::io::SaveImageFile(explicitSaveRequest);
    if (!explicitSaveResult.success) {
        std::cerr << "Explicit metadata save failed: " << explicitSaveResult.errorMessage << std::endl;
        return 1;
    }

    rawgl::io::MetadataReadRequest explicitMetadataRequest;
    explicitMetadataRequest.path = explicitPath.string();
    explicitMetadataRequest.nameStyle = rawgl::io::MetadataNameStyle::Oiio;
    explicitMetadataRequest.namePolicy = rawgl::io::MetadataNamePolicy::ExifToolAlias;
    const rawgl::io::MetadataReadResult explicitMetadataResult = rawgl::io::ReadMetadataFile(explicitMetadataRequest);
    if (!explicitMetadataResult.success) {
        std::cerr << "Explicit metadata read failed: " << explicitMetadataResult.errorMessage << std::endl;
        return 1;
    }

    bool foundSoftware = false;
    for (const rawgl::io::MetadataEntry& entry : explicitMetadataResult.entries) {
        if (entry.name == "Software" && entry.valueText == softwareValue) {
            foundSoftware = true;
        }
    }

    if (!foundSoftware) {
        std::cerr << "Explicit metadata save did not export Software correctly." << std::endl;
        return 1;
    }

    return 0;
}
