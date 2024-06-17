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

#include <glad/glad.h>

#ifdef APIENTRY
#undef APIENTRY
#endif

#define GLASSERT(x) if (!(x)) __debugbreak();
#define GLCall(x) GL_ClearError(); x; GLASSERT(GL_LogCall(#x, __FILE__, __LINE__))

void GL_ClearError();
bool GL_LogCall(const char* func, const char* file, int line);

struct OpenGLHandle
{
    OpenGLHandle();
    ~OpenGLHandle();
};
