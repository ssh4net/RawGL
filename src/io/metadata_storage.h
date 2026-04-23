// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include <memory>

namespace rawgl::io {

struct MetadataBackendStorageDeleter {
    void operator()(void* storage) const noexcept;
};

using MetadataBackendStorageHandle = std::unique_ptr<void, MetadataBackendStorageDeleter>;

struct MetadataDocumentStorage final {
    MetadataBackendStorageHandle backendStorage;
};

}  // namespace rawgl::io
