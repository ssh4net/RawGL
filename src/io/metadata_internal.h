// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include <string>

#include <rawgl/rawgl_io.h>

namespace rawgl::io {

MetadataReadResult
read_metadata_file_impl(const MetadataReadRequest& request);

MetadataDocumentReadResult
read_metadata_document_file_impl(const MetadataDocumentReadRequest& request);

}  // namespace rawgl::io
