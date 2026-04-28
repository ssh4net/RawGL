// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "metadata_internal.h"

namespace rawgl::io {

MetadataReadResult
IoRuntime::readMetadataFile(const MetadataReadRequest& request) const
{
    return read_metadata_file_impl(request);
}

MetadataReadResult
ReadMetadataFile(const MetadataReadRequest& request)
{
    return IoRuntime().readMetadataFile(request);
}

MetadataDocumentReadResult
IoRuntime::readMetadataDocumentFile(const MetadataDocumentReadRequest& request) const
{
    return read_metadata_document_file_impl(request);
}

MetadataDocumentReadResult
ReadMetadataDocumentFile(const MetadataDocumentReadRequest& request)
{
    return IoRuntime().readMetadataDocumentFile(request);
}

ImageMetadataTransferResult
IoRuntime::transferImageMetadataFile(const ImageMetadataTransferRequest& request) const
{
    return transfer_image_metadata_file_impl(request);
}

ImageMetadataTransferResult
TransferImageMetadataFile(const ImageMetadataTransferRequest& request)
{
    return IoRuntime().transferImageMetadataFile(request);
}

}  // namespace rawgl::io
