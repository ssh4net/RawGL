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

#if defined(RAWGL_USE_GLEW)
#    include <GL/glew.h>
using GLADloadproc = void* (*)(const char*);
inline int
gladLoadGLLoader(GLADloadproc)
{
    return glewInit() == GLEW_OK;
}
#else
#    include <glad/glad.h>
#endif
#include <cstdlib>
#include <unordered_map>

#ifdef APIENTRY
#    undef APIENTRY
#endif

#if defined(_MSC_VER)
#    define RAWGL_DEBUGBREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
#    define RAWGL_DEBUGBREAK() __builtin_trap()
#else
#    define RAWGL_DEBUGBREAK() std::abort()
#endif

#define GLASSERT(x)             \
    do {                        \
        if (!(x)) {             \
            RAWGL_DEBUGBREAK(); \
        }                       \
    } while (false)
#define GLCall(x)                                     \
    do {                                              \
        GL_ClearError();                              \
        x;                                            \
        GLASSERT(GL_LogCall(#x, __FILE__, __LINE__)); \
    } while (false)

struct GLFWwindow;

void
GL_ClearError();
bool
GL_LogCall(const char* func, const char* file, int line);

struct OpenGLHandle {
    OpenGLHandle();
    ~OpenGLHandle();

private:
    GLFWwindow* m_window = nullptr;
};

//struct glsl_type_set {
//    GLenum      type;
//    const char* name;
//};

//extern const glsl_type_set type_set[];
extern const std::unordered_map<GLenum, const char*> glsl_type_map;
//extern int type_set_size;
extern const char*
glsl_type_name(GLenum type);

void
get_GPUfeatures();
