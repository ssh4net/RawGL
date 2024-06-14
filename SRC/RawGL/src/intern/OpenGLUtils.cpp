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
    : window(nullptr), hDC(nullptr), hRC(nullptr)
{
    // Initialize GLFW
    if (!glfwInit())
    {
        LOG(error) << "Failed to initialize GLFW";
        throw std::runtime_error("Failed to initialize GLFW");
    }

    // Initialize NVAPI
    InitializeNVAPI();

    // Select the GPU using NVAPI
    NvPhysicalGpuHandle gpu = SelectGPU();

    // Set GLFW window hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    // Create a hidden window
    window = glfwCreateWindow(512, 512, "Hidden Window", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    // Make the context current
    glfwMakeContextCurrent(window);

    // Initialize GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        glfwTerminate();
        LOG(error) << "Failed to initialize GLAD";
        throw std::runtime_error("Failed to initialize GLAD");
    }

    // At this point, GLAD is initialized with OpenGL functions and you can proceed with rendering
}

OpenGLHandle::~OpenGLHandle()
{
    // Clean up GLFW
    if (window)
    {
        glfwDestroyWindow(window);
    }
    glfwTerminate();

    // Unload NVAPI
    NvAPI_Unload();
}

void OpenGLHandle::InitializeNVAPI()
{
    NvAPI_Status status = NvAPI_Initialize();
    if (status != NVAPI_OK)
    {
        LOG(error) << "Failed to initialize NVAPI";
        throw std::runtime_error("Failed to initialize NVAPI");
    }
}

unsigned long OpenGLHandle::gpus = 0;
unsigned int OpenGLHandle::gpu = 0;

NvPhysicalGpuHandle OpenGLHandle::SelectGPU()
{
    NvPhysicalGpuHandle nvGPUHandles[NVAPI_MAX_PHYSICAL_GPUS] = { 0 };
    NvU32 gpuCount = 0;
    NvAPI_Status status = NvAPI_EnumPhysicalGPUs(nvGPUHandles, &gpuCount);
    if (status != NVAPI_OK || gpuCount == 0)
    {
        LOG(error) << "Failed to enumerate NVIDIA GPUs";
        throw std::runtime_error("No NVIDIA GPUs found");
    }
    gpus = gpuCount > NVAPI_MAX_PHYSICAL_GPUS ? NVAPI_MAX_PHYSICAL_GPUS : gpuCount;
    LOG(debug) << "Found " << gpuCount << " NVIDIA GPUs";

    // check if N-gpu less than the number of GPUs
    if (gpu + 1 > gpus)
	{
		gpu = gpus - 1;
        LOG(debug) << "GPU index out of range, using the last GPU";
	}

    NvPhysicalGpuHandle selectedGPU = nvGPUHandles[gpu];

    // Additional code to select a specific GPU can be added here

    return selectedGPU;
}

//OpenGLHandle::OpenGLHandle()
//{
//    glfwInit();
//
//    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
//    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
//    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
//    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
//
//#ifdef __APPLE__
//    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
//#endif
//
//    GLFWwindow* window = glfwCreateWindow(512, 512, "Hidden Window", NULL, NULL);
//
//    if (window == NULL)
//    {
//        glfwTerminate();
//        throw std::exception("Failed to create GLFW window");
//    }
//
//    glfwMakeContextCurrent(window);
//
//    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
//    {
//        glfwTerminate();
//        throw std::exception("Failed to initialize GLAD");
//    }
//}
//
//OpenGLHandle::~OpenGLHandle()
//{
//    glfwTerminate();
//}
