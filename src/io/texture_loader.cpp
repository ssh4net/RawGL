// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "texture_loader.h"

#include "gl_utils.h"
#include "image_io.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace rawgl::io {
namespace {

static HostImageData
to_host_image_data(const LoadedTextureData& texture)
{
    if (!texture.valid) {
        throw std::runtime_error("Invalid loaded texture data");
    }

    HostImageData hostImage;
    hostImage.width            = texture.width;
    hostImage.height           = texture.height;
    hostImage.channels         = texture.channels;
    hostImage.alphaChannel     = texture.alphaChannel;
    hostImage.glInternalFormat = texture.internalFormat;
    hostImage.glType           = texture.type;
    hostImage.bytes            = texture.bytes;
    return hostImage;
}

static void
resolve_texture_storage(LoadedTextureData& texture, const OIIO::TypeDesc format)
{
    const GLenum formats[5][4] = {
        { GL_R8, GL_RG8, GL_RGB8, GL_RGBA8 },
        { GL_R16, GL_RG16, GL_RGB16, GL_RGBA16 },
        { GL_R32UI, GL_RG32UI, GL_RGB32UI, GL_RGBA32UI },
        { GL_R16F, GL_RG16F, GL_RGB16F, GL_RGBA16F },
        { GL_R32F, GL_RG32F, GL_RGB32F, GL_RGBA32F },
    };

    if (texture.channels < 1 || texture.channels > 4) {
        throw std::runtime_error("Unsupported image channel count");
    }

    switch (format.basetype) {
    case OIIO::TypeDesc::UINT8:
        texture.internalFormat = formats[0][texture.channels - 1];
        texture.type           = GL_UNSIGNED_BYTE;
        break;
    case OIIO::TypeDesc::UINT16:
        texture.internalFormat = formats[1][texture.channels - 1];
        texture.type           = GL_UNSIGNED_SHORT;
        break;
    case OIIO::TypeDesc::UINT32:
        texture.internalFormat = formats[2][texture.channels - 1];
        texture.type           = GL_UNSIGNED_INT;
        break;
    case OIIO::TypeDesc::HALF:
        texture.internalFormat = formats[3][texture.channels - 1];
        texture.type           = GL_HALF_FLOAT;
        break;
    case OIIO::TypeDesc::FLOAT:
        texture.internalFormat = formats[4][texture.channels - 1];
        texture.type           = GL_FLOAT;
        break;
    default: throw std::runtime_error("Unsupported image type");
    }
}

static size_t
byte_size_for_oiio_format(const OIIO::TypeDesc format)
{
    switch (format.basetype) {
    case OIIO::TypeDesc::UINT8: return 1u;
    case OIIO::TypeDesc::UINT16:
    case OIIO::TypeDesc::HALF: return 2u;
    case OIIO::TypeDesc::UINT32:
    case OIIO::TypeDesc::FLOAT: return 4u;
    case OIIO::TypeDesc::UINT64:
    case OIIO::TypeDesc::DOUBLE: return 8u;
    default: break;
    }

    return 0u;
}

}  // namespace

LoadedTextureData
load_texture_file_data(const std::string& path, const std::map<std::string, std::string>& attributes)
{
    LoadedTextureData texture;
    void* pixelData = nullptr;
    OIIO::TypeDesc format;

    if (!image_utils::load_image(path,
                                 attributes,
                                 texture.width,
                                 texture.height,
                                 pixelData,
                                 texture.channels,
                                 texture.alphaChannel,
                                 format)) {
        return texture;
    }

    resolve_texture_storage(texture, format);

    const size_t bytesPerComponent = byte_size_for_oiio_format(format);
    if (bytesPerComponent == 0u) {
        if (pixelData != nullptr) {
            free(pixelData);
        }
        throw std::runtime_error("Unsupported image type");
    }

    const size_t byteCount = static_cast<size_t>(texture.width) * static_cast<size_t>(texture.height)
                             * static_cast<size_t>(texture.channels) * bytesPerComponent;
    texture.bytes.resize(byteCount);
    if (pixelData != nullptr && byteCount > 0u) {
        std::memcpy(texture.bytes.data(), pixelData, byteCount);
        free(pixelData);
    }

    texture.valid = true;
    return texture;
}

HostImageData
load_host_image_data(const std::string& path, const std::map<std::string, std::string>& attributes)
{
    return to_host_image_data(load_texture_file_data(path, attributes));
}

}  // namespace rawgl::io
