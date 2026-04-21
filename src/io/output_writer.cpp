// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "output_writer.h"

#include "gl_utils.h"
#include "image_io.h"
#include "log.h"
#include "timer.h"

#include <memory>

namespace rawgl::io {
namespace {

static bool
resolve_output_pixel_format(const unsigned int glType, OIIO::TypeDesc& pixelFormat)
{
    switch (glType) {
    case GL_UNSIGNED_BYTE:
        pixelFormat = OIIO::TypeDesc::UINT8;
        return true;
    case GL_UNSIGNED_SHORT:
        pixelFormat = OIIO::TypeDesc::UINT16;
        return true;
    case GL_UNSIGNED_INT:
        pixelFormat = OIIO::TypeDesc::UINT32;
        return true;
    case GL_HALF_FLOAT:
        pixelFormat = OIIO::TypeDesc::HALF;
        return true;
    case GL_FLOAT:
        pixelFormat = OIIO::TypeDesc::FLOAT;
        return true;
    default: break;
    }

    return false;
}

}  // namespace

void
save_image_output(const OutputWriteRequest& request)
{
    if (request.path.empty() || request.image == nullptr) {
        return;
    }

    Timer timer;

    LOG(info) << "Saving image: " << request.path;

    std::unique_ptr<OIIO::ImageOutput> output = OIIO::ImageOutput::create(request.path);
    if (!output) {
        LOG(error) << "Can't open file: " << OIIO::geterror();
        return;
    }

    bool formatDefaulted = false;
    const OIIO::TypeDesc outputFormat = image_utils::get_output_format(request.path, request.bits, formatDefaulted);
    if (formatDefaulted) {
        LOG(warning) << "Output bit depth " << request.bits << " is not supported for " << request.path
                     << ", using the closest supported format instead.";
    }

    OIIO::ImageSpec spec(request.image->width,
                         request.image->height,
                         request.image->channels,
                         outputFormat);

    for (const auto& attribute : request.attributes) {
        spec.attribute(attribute.first, attribute.second);
    }

    spec.alpha_channel = request.alphaChannel >= 0 ? request.alphaChannel : request.image->alphaChannel;

    if (!output->open(request.path, spec)) {
        LOG(error) << "Can't open file for writing: " << OIIO::geterror();
        return;
    }

    OIIO::TypeDesc pixelFormat;
    if (!resolve_output_pixel_format(request.image->glType, pixelFormat)) {
        LOG(error) << "Unsupported host image type for output: " << request.image->glType;
        return;
    }

    if (!output->write_image(pixelFormat,
                             request.image->bytes.data(),
                             OIIO::AutoStride,
                             OIIO::AutoStride,
                             OIIO::AutoStride,
                             (*image_utils::progress_callback))) {
        LOG(error) << "Can't write file: " << OIIO::geterror();
    } else if (!output->close()) {
        LOG(error) << "Can't close file after writing: " << OIIO::geterror();
    } else {
        LOG(debug) << "Finished in " << timer.nowText();
    }
}

}  // namespace rawgl::io
