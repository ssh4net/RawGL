// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022 Erium Vladlen.

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

namespace rawgl {
struct RuntimeInfo;
}

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

    void makeCurrent() const;
    void releaseCurrent() const;

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

void
rawgl_set_opengl_platform_override(const char* platform);

void
rawgl_fill_runtime_environment_info(rawgl::RuntimeInfo& info);

void
rawgl_fill_current_runtime_info(rawgl::RuntimeInfo& info);

const char*
rawgl_last_opengl_error_message();

void
rawgl_clear_opengl_error_message();
