#pragma once

#include "Common.h"
#include "OpenGLUtils.h"

class Texture
{
public:
	Texture() : m_id(0) {}
    Texture(int width, int height, GLenum internalFormat, GLenum type, const void* data = nullptr, int alphaChannel = -1);
    ~Texture();

    int getId() { return m_id; }
    int getWidth() { return m_width; }
    int getHeight() { return m_height; }
    int getChannels() { return m_channels; }
    GLenum getInternalFormat() { return m_internalFormat; }
    void* getData(GLenum type) const;

private:
    GLuint m_id;

    int m_width;
	int m_height;

	GLenum m_baseFormat;
	int m_channels;
    int m_alphaChannel;

    GLenum m_internalFormat;
};
