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

#pragma once

#include "Common.h"
#include "OpenGLUtils.h"

class Texture
{
public:
	Texture() : m_id(0) {}
    Texture(GLsizei width, GLsizei height, GLenum internalFormat, GLenum type, const void* data = nullptr, int alphaChannel = -1);
    ~Texture();

    int getId() { return m_id; }
	GLsizei getWidth() { return m_width; }
	GLsizei getHeight() { return m_height; }
    int getChannels() { return m_channels; }
    GLenum getInternalFormat() { return m_internalFormat; }
    void* getData(GLenum type) const;

private:
	GLuint m_id = 0;

	GLsizei m_width = 0;
	GLsizei m_height = 0;

	GLenum m_baseFormat = 0;
	int m_channels = 0;
	int m_alphaChannel = -1;

	GLenum m_internalFormat = 0;
};
