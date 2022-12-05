/* 
 * This file is part of the RawGL distribution (https://github.com/ssh4net/RawGL).
 * Copyright (c) 2022 Erium Vladlen.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Texture.h"

Texture::Texture(GLsizei width, GLsizei height, GLenum internalFormat, GLenum type, const GLvoid* data, int alphaChannel) :
    Texture()
{
    glActiveTexture(GL_TEXTURE0);
	
    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);
	// default texture parameters.
	// GL_CLAMP_TO_EDGE works better with convolution filters.
	// GL_REPEAT default for texturing (not filtering).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	// fix for compute shader textures?
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    m_width = width;
    m_height = height;
    m_internalFormat = internalFormat;

    // Save for future output
    m_alphaChannel = alphaChannel;

    // Re-use the variable for swizzle
    if (alphaChannel > 0 && alphaChannel <= 3)
    {
        const GLenum c[] = { GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA };
        alphaChannel = c[alphaChannel];
    }
    else
        alphaChannel = GL_ONE;

    std::array<GLint, 4> swizzleMask;

    // Set base format & channel count
    switch (m_internalFormat)
    {
    case GL_R8:
    case GL_R8I:
    case GL_R8UI:
    case GL_R8_SNORM:
    case GL_R16:
    case GL_R16I:
    case GL_R16UI:
    case GL_R16_SNORM:
    case GL_R16F:
    case GL_R32I:
    case GL_R32UI:
    case GL_R32F:
        swizzleMask = { GL_RED, GL_RED, GL_RED, alphaChannel };
        m_baseFormat = GL_RED;
        m_channels = 1;
        break;
    case GL_RG8:
    case GL_RG8I:
    case GL_RG8UI:
    case GL_RG8_SNORM:
    case GL_RG16:
    case GL_RG16I:
    case GL_RG16UI:
    case GL_RG16_SNORM:
    case GL_RG16F:
    case GL_RG32I:
    case GL_RG32UI:
    case GL_RG32F:
        swizzleMask = { GL_RED, GL_GREEN, GL_ONE, alphaChannel };
        m_baseFormat = GL_RG;
        m_channels = 2;
        break;
    case GL_RGB8:
    case GL_RGB8I:
    case GL_RGB8UI:
    case GL_RGB8_SNORM:
    case GL_RGB10_A2:
    case GL_R11F_G11F_B10F:
    case GL_RGB16:
    case GL_RGB16I:
    case GL_RGB16UI:
    case GL_RGB16_SNORM:
    case GL_RGB16F:
    case GL_RGB32I:
    case GL_RGB32UI:
    case GL_RGB32F:
    case GL_SRGB8:
        swizzleMask = { GL_RED, GL_GREEN, GL_BLUE, alphaChannel };
        m_baseFormat = GL_RGB;
        m_channels = 3;
        break;
    case GL_RGBA8:
    case GL_RGBA8I:
    case GL_RGBA8UI:
    case GL_RGBA8_SNORM:
    case GL_RGBA16:
    case GL_RGBA16I:
    case GL_RGBA16UI:
    case GL_RGBA16_SNORM:
    case GL_RGBA16F:
    case GL_RGBA32I:
    case GL_RGBA32UI:
    case GL_RGBA32F:
    case GL_SRGB8_ALPHA8:
        swizzleMask = { GL_RED, GL_GREEN, GL_BLUE, alphaChannel };
        m_baseFormat = GL_RGBA;
        m_channels = 4;
        break;
    default:
        // should never happen
        assert(0);
        break;
    }

    int bytes = 1;
    switch (type)
    {
    case GL_BYTE:
    case GL_UNSIGNED_BYTE:  //OIIO::TypeDesc::UINT8:
        bytes = 1;
        break;
	case GL_SHORT:
    case GL_UNSIGNED_SHORT: //OIIO::TypeDesc::UINT16:
    case GL_HALF_FLOAT:     //OIIO::TypeDesc::HALF:
        bytes = 2;
        break;
	case GL_INT:
    case GL_UNSIGNED_INT:   //OIIO::TypeDesc::UINT32:
    case GL_FLOAT:          //OIIO::TypeDesc::FLOAT:
        bytes = 4;
        break;
    case GL_DOUBLE:
		bytes = 8;
		break;
    default:
        bytes = 4;
        break;
    }
	
    if (m_channels != 4) {
        if ((m_width * bytes * m_channels) % 4 != 0) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, bytes);
        }
    }
    else {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }
	
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask.data());

    glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat, width, height, 0, m_baseFormat, type, data);

    //glGenerateTextureMipmap(m_id); // NOTE: OpenGL 4.5+
}

Texture::~Texture()
{
    if (m_id)
        glDeleteTextures(1, &m_id);
}

void* Texture::getData(GLenum type) const
{
    int bytes;

    switch (type)
    {
    case GL_UNSIGNED_BYTE:
        bytes = 1;
        break;
    case GL_UNSIGNED_SHORT:
    case GL_HALF_FLOAT:
        bytes = 2;
        break;
    case GL_FLOAT:
        bytes = 4;
        break;
    default:
        assert(0);
        exit(1);
        return nullptr;
    }

	// malloc requred size_t (unsinged int) so we need to cast to that.
	// othwise we have int overflow for large textures.
    std::size_t mem_size = (std::size_t)m_width * (std::size_t)m_height * (std::size_t)(m_channels * bytes);
    void* data = malloc(mem_size);

    //glGenerateTextureMipmap(m_id); // NOTE: OpenGL 4.5+

    glActiveTexture(GL_TEXTURE0);
	
    //glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTexture(GL_TEXTURE_2D, m_id);
    glGetTexImage(GL_TEXTURE_2D, 0, m_baseFormat, type, data);

    return data;
}
