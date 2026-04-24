// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "output_writer.h"

#include "image_backend.h"
#include "log.h"

namespace rawgl::io {

bool
save_image_output(const OutputWriteRequest& request, std::string& errorMessage)
{
    if (request.path.empty() || request.image == nullptr) {
        errorMessage = "invalid image output request";
        return false;
    }

    const ImageEncodeSettings settings = resolve_image_encode_settings(request.path, request.bits);
    if (settings.defaulted) {
        LOG(warning) << "Output bit depth " << request.bits << " is not supported for " << request.path
                     << ", using the closest supported format instead.";
    }

    if (!encode_image_file(request.path,
                           request.attributes,
                           request.alphaChannel,
                           *request.image,
                           settings,
                           errorMessage)) {
        LOG(error) << errorMessage;
        return false;
    }

    return true;
}

}  // namespace rawgl::io
