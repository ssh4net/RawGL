// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include <map>
#include <string>

#include <rawgl/rawgl_io.h>

namespace rawgl::io {

MetadataReadResult
read_metadata_file_impl(const MetadataReadRequest& request);

MetadataDocumentReadResult
read_metadata_document_file_impl(const MetadataDocumentReadRequest& request);

bool
flatten_metadata_document_to_oiio_attributes(const MetadataDocument& document,
                                             std::map<std::string, std::string>* out,
                                             std::string* errorMessage);

}  // namespace rawgl::io
