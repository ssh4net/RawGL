#include "Common.h"
#include "OpenGLUtils.h"
#include "Log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

void GL_ClearError()
{
    while (glGetError() != GL_NO_ERROR);
}

bool GL_LogCall(const char* func, const char* file, int line)
{
    while (GLenum error = glGetError())
    {
        LOG(error) << "[OpenGL] (" << error << "):" << func << " " << file << ":" << line;
        return false;
    }

    return true;
}

OpenGLHandle::OpenGLHandle()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(512, 512, "Hidden Window", NULL, NULL);

    if (window == NULL)
    {
        glfwTerminate();
        throw std::exception("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        glfwTerminate();
        throw std::exception("Failed to initialize GLAD");
    }
}

OpenGLHandle::~OpenGLHandle()
{
    glfwTerminate();
}
