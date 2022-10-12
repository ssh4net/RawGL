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
