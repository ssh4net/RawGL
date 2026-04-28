// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include <string>

#include <rawgl/rawgl_io.h>

#include "metadata_storage.h"

namespace rawgl::io {

MetadataReadResult
read_metadata_file_impl(const MetadataReadRequest& request);

MetadataDocumentReadResult
read_metadata_document_file_impl(const MetadataDocumentReadRequest& request);

struct ImageMetadataApplyResult {
    bool success = false;
    std::string errorMessage;
};

ImageMetadataApplyResult
apply_source_metadata_to_tiff_file_impl(const MetadataDocument& document,
                                        const std::string& path);

ImageMetadataApplyResult
apply_source_metadata_to_tiff_file_impl(const MetadataDocument& document,
                                        const std::string& path,
                                        const HostImageData& targetImage);

ImageMetadataApplyResult
apply_source_metadata_to_jpeg_file_impl(const MetadataDocument& document,
                                        const std::string& path);

ImageMetadataApplyResult
apply_source_metadata_to_jpeg_file_impl(const MetadataDocument& document,
                                        const std::string& path,
                                        const HostImageData& targetImage);

ImageMetadataApplyResult
apply_source_metadata_to_png_file_impl(const MetadataDocument& document,
                                       const std::string& path);

ImageMetadataApplyResult
apply_source_metadata_to_png_file_impl(const MetadataDocument& document,
                                       const std::string& path,
                                       const HostImageData& targetImage);

ImageMetadataApplyResult
apply_source_metadata_to_exr_file_impl(const MetadataDocument& document,
                                       const std::string& path);

ImageMetadataApplyResult
apply_source_metadata_to_exr_file_impl(const MetadataDocument& document,
                                       const std::string& path,
                                       const HostImageData& targetImage);

ImageMetadataTransferResult
transfer_image_metadata_file_impl(const ImageMetadataTransferRequest& request);

}  // namespace rawgl::io
