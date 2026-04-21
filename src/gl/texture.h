// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022 Erium Vladlen.

#pragma once

#include "common.h"
#include "gl_utils.h"

class Texture {
public:
    Texture()
        : m_id(0)
    {
    }
    Texture(GLsizei width, GLsizei height, GLenum internalFormat, GLenum type, const void* data = nullptr,
            int alphaChannel = -1);
    ~Texture();

    int getId() const { return m_id; }
    GLsizei getWidth() const { return m_width; }
    GLsizei getHeight() const { return m_height; }
    int getChannels() const { return m_channels; }
    int getAlphaChannel() const { return m_alphaChannel; }
    GLenum getInternalFormat() const { return m_internalFormat; }
    void* getData(GLenum type) const;

private:
    GLuint m_id = 0;

    GLsizei m_width  = 0;
    GLsizei m_height = 0;

    GLenum m_baseFormat = 0;
    int m_channels      = 0;
    int m_alphaChannel  = -1;

    GLenum m_internalFormat = 0;
};
