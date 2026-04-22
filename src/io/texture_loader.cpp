// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "texture_loader.h"

#include "gl_utils.h"
#include "image_backend.h"

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
resolve_texture_storage(LoadedTextureData& texture, const ImageComponentType componentType)
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

    switch (componentType) {
    case ImageComponentType::U8:
        texture.internalFormat = formats[0][texture.channels - 1];
        texture.type           = GL_UNSIGNED_BYTE;
        break;
    case ImageComponentType::U16:
        texture.internalFormat = formats[1][texture.channels - 1];
        texture.type           = GL_UNSIGNED_SHORT;
        break;
    case ImageComponentType::U32:
        texture.internalFormat = formats[2][texture.channels - 1];
        texture.type           = GL_UNSIGNED_INT;
        break;
    case ImageComponentType::F16:
        texture.internalFormat = formats[3][texture.channels - 1];
        texture.type           = GL_HALF_FLOAT;
        break;
    case ImageComponentType::F32:
        texture.internalFormat = formats[4][texture.channels - 1];
        texture.type           = GL_FLOAT;
        break;
    case ImageComponentType::F64:
    case ImageComponentType::Unknown:
    default: throw std::runtime_error("Unsupported image type");
    }
}

}  // namespace

LoadedTextureData
load_texture_file_data(const std::string& path, const std::map<std::string, std::string>& attributes)
{
    LoadedTextureData texture;
    const DecodedImageData decoded = decode_image_file(path, attributes);
    if (!decoded.success) {
        return texture;
    }

    texture.width = decoded.width;
    texture.height = decoded.height;
    texture.channels = decoded.channels;
    texture.alphaChannel = decoded.alphaChannel;

    resolve_texture_storage(texture, decoded.componentType);

    const size_t bytesPerComponent = byte_size_for_image_component(decoded.componentType);
    if (bytesPerComponent == 0u) {
        throw std::runtime_error("Unsupported image type");
    }

    const size_t byteCount = static_cast<size_t>(texture.width) * static_cast<size_t>(texture.height)
                             * static_cast<size_t>(texture.channels) * bytesPerComponent;
    if (decoded.bytes.size() != byteCount) {
        throw std::runtime_error("Decoded image byte size mismatch");
    }

    texture.bytes = decoded.bytes;

    texture.valid = true;
    return texture;
}

HostImageData
load_host_image_data(const std::string& path, const std::map<std::string, std::string>& attributes)
{
    return to_host_image_data(load_texture_file_data(path, attributes));
}

}  // namespace rawgl::io
