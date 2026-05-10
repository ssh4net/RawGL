// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.


#include "common.h"
#include "gl_utils.h"
#include "log.h"
#include "rawgl/rawgl_core.h"

#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <termcolor/termcolor.hpp>
#include <utility>

#include <unordered_map>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace {
thread_local std::string rawgl_opengl_error_message;
std::string rawgl_opengl_platform_override;

enum class RawGLOpenGLPlatform {
    automatic,
    x11,
    wayland,
};

struct RawGLResolvedOpenGLPlatform {
    RawGLOpenGLPlatform platform = RawGLOpenGLPlatform::automatic;
    std::string requested = "auto";
    std::string selected;
};

[[noreturn]] void
rawgl_throw_opengl_error(std::string message);

std::string
rawgl_getenv_text(const char* name)
{
    const char* value = std::getenv(name);
    return value != nullptr ? value : "";
}

std::string
rawgl_normalize_platform_text(const std::string& text)
{
    std::string normalized;
    normalized.reserve(text.size());
    for (const char ch : text) {
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return normalized;
}

bool
rawgl_contains_case_insensitive(const std::string& text, const char* needle)
{
    std::string lowerText = rawgl_normalize_platform_text(text);
    std::string lowerNeedle = rawgl_normalize_platform_text(needle);
    return lowerText.find(lowerNeedle) != std::string::npos;
}

std::string
rawgl_requested_opengl_platform_text()
{
    if (!rawgl_opengl_platform_override.empty()) {
        return rawgl_opengl_platform_override;
    }

    const char* envValue = std::getenv("RAWGL_GL_PLATFORM");
    if (envValue != nullptr && envValue[0] != '\0') {
        return envValue;
    }

    return "auto";
}

RawGLResolvedOpenGLPlatform
rawgl_resolve_opengl_platform()
{
    RawGLResolvedOpenGLPlatform result;
    result.requested = rawgl_requested_opengl_platform_text();

    const std::string normalized = rawgl_normalize_platform_text(result.requested);
    if (normalized == "auto" || normalized == "automatic" || normalized == "default") {
        result.platform = RawGLOpenGLPlatform::automatic;
    } else if (normalized == "x11") {
        result.platform = RawGLOpenGLPlatform::x11;
    } else if (normalized == "wayland") {
        result.platform = RawGLOpenGLPlatform::wayland;
    } else {
        rawgl_throw_opengl_error("Unsupported OpenGL platform '" + result.requested
                                 + "'. Expected auto, x11, or wayland.");
    }

    const std::string display = rawgl_getenv_text("DISPLAY");
    const std::string waylandDisplay = rawgl_getenv_text("WAYLAND_DISPLAY");
    if (result.platform == RawGLOpenGLPlatform::x11) {
        result.selected = "x11";
    } else if (result.platform == RawGLOpenGLPlatform::wayland) {
        result.selected = "wayland";
    } else if (!display.empty()) {
        result.selected = "x11";
    } else if (!waylandDisplay.empty()) {
        result.selected = "wayland";
    } else {
        result.selected = "auto";
    }

    return result;
}

void
rawgl_glfw_error_callback(int error, const char* description)
{
    LOG(error) << "[GLFW] (" << error << "): " << (description != nullptr ? description : "unknown");
}

std::string
rawgl_glfw_error_text(const char* prefix)
{
    const char* description = nullptr;
    const int error         = glfwGetError(&description);

    if (error == GLFW_NO_ERROR) {
        return prefix;
    }

    std::string message(prefix);
    message += " (GLFW ";
    message += std::to_string(error);
    message += ": ";
    message += description != nullptr ? description : "unknown";
    message += ")";
    return message;
}

void
rawgl_set_context_hints(int major, int minor)
{
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
    glfwWindowHint(GLFW_CONTEXT_CREATION_API, GLFW_NATIVE_CONTEXT_API);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
}

[[noreturn]] void
rawgl_throw_opengl_error(std::string message)
{
    rawgl_opengl_error_message = std::move(message);
    throw std::runtime_error(rawgl_opengl_error_message);
}
}  // namespace

void
rawgl_set_opengl_platform_override(const char* platform)
{
    rawgl_opengl_platform_override = platform != nullptr ? platform : "";
}

void
rawgl_fill_runtime_environment_info(rawgl::RuntimeInfo& info)
{
    info.requestedPlatform = rawgl_requested_opengl_platform_text();
    info.display = rawgl_getenv_text("DISPLAY");
    info.waylandDisplay = rawgl_getenv_text("WAYLAND_DISPLAY");

    const RawGLResolvedOpenGLPlatform platform = rawgl_resolve_opengl_platform();
    info.requestedPlatform = platform.requested;
    info.selectedPlatform = platform.selected;
}

void
rawgl_fill_current_runtime_info(rawgl::RuntimeInfo& info)
{
    rawgl_fill_runtime_environment_info(info);

    const GLubyte* vendor = glGetString(GL_VENDOR);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* shadingLanguageVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);

    info.vendor = vendor != nullptr ? reinterpret_cast<const char*>(vendor) : "";
    info.renderer = renderer != nullptr ? reinterpret_cast<const char*>(renderer) : "";
    info.version = version != nullptr ? reinterpret_cast<const char*>(version) : "";
    info.shadingLanguageVersion =
        shadingLanguageVersion != nullptr ? reinterpret_cast<const char*>(shadingLanguageVersion) : "";
    info.softwareRenderer = rawgl_contains_case_insensitive(info.renderer, "llvmpipe")
                            || rawgl_contains_case_insensitive(info.renderer, "softpipe")
                            || rawgl_contains_case_insensitive(info.renderer, "software rasterizer")
                            || rawgl_contains_case_insensitive(info.renderer, "swrast");
}

const char*
rawgl_last_opengl_error_message()
{
    return rawgl_opengl_error_message.empty() ? nullptr : rawgl_opengl_error_message.c_str();
}

void
rawgl_clear_opengl_error_message()
{
    rawgl_opengl_error_message.clear();
}

void
GL_ClearError()
{
    while (glGetError() != GL_NO_ERROR)
        ;
}

bool
GL_LogCall(const char* func, const char* file, int line)
{
    while (GLenum error = glGetError()) {
        LOG(error) << "[OpenGL] (" << error << "):" << func << " " << file << ":" << line;
        return false;
    }

    return true;
}

OpenGLHandle::OpenGLHandle()
{
    rawgl_clear_opengl_error_message();
    rawgl::RuntimeInfo runtimeInfo;
    rawgl_fill_runtime_environment_info(runtimeInfo);
    glfwSetErrorCallback(rawgl_glfw_error_callback);

#if defined(__linux__)
    LOG(info) << "DISPLAY=" << runtimeInfo.display << ", WAYLAND_DISPLAY=" << runtimeInfo.waylandDisplay
              << ", RAWGL_GL_PLATFORM=" << runtimeInfo.requestedPlatform;
    if (runtimeInfo.selectedPlatform == "x11") {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
        LOG(info) << "Using GLFW platform X11";
    } else if (runtimeInfo.selectedPlatform == "wayland") {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
        LOG(info) << "Using GLFW platform Wayland";
    }
#endif

    if (!glfwInit()) {
        rawgl_throw_opengl_error(rawgl_glfw_error_text("Failed to initialize GLFW"));
    }

    int contextMajor = 4;
    int contextMinor = 6;
#if defined(__linux__)
    contextMinor = 5;
#endif

    LOG(info) << "Requesting OpenGL context " << contextMajor << "." << contextMinor;

    rawgl_set_context_hints(contextMajor, contextMinor);

    m_window = glfwCreateWindow(512, 512, "Hidden Window", nullptr, nullptr);

    if (m_window == nullptr) {
        const std::string message = rawgl_glfw_error_text("Failed to create GLFW window");
        glfwTerminate();
        rawgl_throw_opengl_error(message);
    }

    glfwMakeContextCurrent(m_window);

    const GLubyte* glVersionText = glGetString(GL_VERSION);
    LOG(info) << "Created OpenGL context: "
              << (glVersionText != nullptr ? reinterpret_cast<const char*>(glVersionText) : "unknown");
    rawgl_fill_current_runtime_info(runtimeInfo);
    LOG(info) << "OpenGL renderer: " << runtimeInfo.vendor << " / " << runtimeInfo.renderer;
    if (runtimeInfo.softwareRenderer) {
        LOG(warning) << "OpenGL renderer appears to be software. RawGL will run, but GPU performance is not available.";
    }

#if defined(RAWGL_USE_GLEW)
    glewExperimental = GL_TRUE;
    const GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        const GLubyte* glewErrorText = glewGetErrorString(glewError);
        std::string message("Failed to initialize GLEW");
        if (glewErrorText != nullptr) {
            message += ": ";
            message += reinterpret_cast<const char*>(glewErrorText);
        }
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        glfwTerminate();
        rawgl_throw_opengl_error(message);
    }
    GL_ClearError();
#else
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        glfwTerminate();
        rawgl_throw_opengl_error("Failed to initialize GLAD");
    }
#endif
}

OpenGLHandle::~OpenGLHandle()
{
    if (m_window != nullptr) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    glfwTerminate();
}

void
OpenGLHandle::makeCurrent() const
{
    if (m_window != nullptr) {
        glfwMakeContextCurrent(m_window);
    }
}

void
OpenGLHandle::releaseCurrent() const
{
    glfwMakeContextCurrent(nullptr);
}

extern const std::unordered_map<GLenum, const char*> glsl_type_map
    = { { GL_INVALID_ENUM, "invalid" },
        { GL_FLOAT, "float" },
        { GL_FLOAT_VEC2, "vec2" },
        { GL_FLOAT_VEC3, "vec3" },
        { GL_FLOAT_VEC4, "vec4" },
        { GL_DOUBLE, "double" },
        { GL_DOUBLE_VEC2, "dvec2" },
        { GL_DOUBLE_VEC3, "dvec3" },
        { GL_DOUBLE_VEC4, "dvec4" },
        { GL_INT, "int" },
        { GL_INT_VEC2, "ivec2" },
        { GL_INT_VEC3, "ivec3" },
        { GL_INT_VEC4, "ivec4" },
        { GL_UNSIGNED_INT, "unsigned int" },
        { GL_UNSIGNED_INT_VEC2, "uvec2" },
        { GL_UNSIGNED_INT_VEC3, "uvec3" },
        { GL_UNSIGNED_INT_VEC4, "uvec4" },
        { GL_BOOL, "bool" },
        { GL_BOOL_VEC2, "bvec2" },
        { GL_BOOL_VEC3, "bvec3" },
        { GL_BOOL_VEC4, "bvec4" },
        { GL_FLOAT_MAT2, "mat2" },
        { GL_FLOAT_MAT3, "mat3" },
        { GL_FLOAT_MAT4, "mat4" },
        { GL_FLOAT_MAT2x3, "mat2x3" },
        { GL_FLOAT_MAT2x4, "mat2x4" },
        { GL_FLOAT_MAT3x2, "mat3x2" },
        { GL_FLOAT_MAT3x4, "mat3x4" },
        { GL_FLOAT_MAT4x2, "mat4x2" },
        { GL_FLOAT_MAT4x3, "mat4x3" },
        { GL_DOUBLE_MAT2, "dmat2" },
        { GL_DOUBLE_MAT3, "dmat3" },
        { GL_DOUBLE_MAT4, "dmat4" },
        { GL_DOUBLE_MAT2x3, "dmat2x3" },
        { GL_DOUBLE_MAT2x4, "dmat2x4" },
        { GL_DOUBLE_MAT3x2, "dmat3x2" },
        { GL_DOUBLE_MAT3x4, "dmat3x4" },
        { GL_DOUBLE_MAT4x2, "dmat4x2" },
        { GL_DOUBLE_MAT4x3, "dmat4x3" },
        { GL_SAMPLER_1D, "sampler1D" },
        { GL_SAMPLER_2D, "sampler2D" },
        { GL_SAMPLER_3D, "sampler3D" },
        { GL_SAMPLER_CUBE, "samplerCube" },
        { GL_SAMPLER_1D_SHADOW, "sampler1DShadow" },
        { GL_SAMPLER_2D_SHADOW, "sampler2DShadow" },
        { GL_SAMPLER_1D_ARRAY, "sampler1DArray" },
        { GL_SAMPLER_2D_ARRAY, "sampler2DArray" },
        { GL_SAMPLER_1D_ARRAY_SHADOW, "sampler1DArrayShadow" },
        { GL_SAMPLER_2D_ARRAY_SHADOW, "sampler2DArrayShadow" },
        { GL_SAMPLER_2D_MULTISAMPLE, "sampler2DMS" },
        { GL_SAMPLER_2D_MULTISAMPLE_ARRAY, "sampler2DMSArray" },
        { GL_SAMPLER_CUBE_SHADOW, "samplerCubeShadow" },
        { GL_SAMPLER_BUFFER, "samplerBuffer" },
        { GL_SAMPLER_2D_RECT, "sampler2DRect" },
        { GL_SAMPLER_2D_RECT_SHADOW, "sampler2DRectShadow" },
        { GL_INT_SAMPLER_1D, "isampler1D" },
        { GL_INT_SAMPLER_2D, "isampler2D" },
        { GL_INT_SAMPLER_3D, "isampler3D" },
        { GL_INT_SAMPLER_CUBE, "isamplerCube" },
        { GL_INT_SAMPLER_1D_ARRAY, "isampler1DArray" },
        { GL_INT_SAMPLER_2D_ARRAY, "isampler2DArray" },
        { GL_INT_SAMPLER_2D_MULTISAMPLE, "isampler2DMS" },
        { GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY, "isampler2DMSArray" },
        { GL_INT_SAMPLER_BUFFER, "isamplerBuffer" },
        { GL_INT_SAMPLER_2D_RECT, "isampler2DRect" },
        { GL_UNSIGNED_INT_SAMPLER_1D, "usampler1D" },
        { GL_UNSIGNED_INT_SAMPLER_2D, "usampler2D" },
        { GL_UNSIGNED_INT_SAMPLER_3D, "usampler3D" },
        { GL_UNSIGNED_INT_SAMPLER_CUBE, "usamplerCube" },
        { GL_UNSIGNED_INT_SAMPLER_1D_ARRAY, "usampler2DArray" },
        { GL_UNSIGNED_INT_SAMPLER_2D_ARRAY, "usampler2DArray" },
        { GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE, "usampler2DMS" },
        { GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY, "usampler2DMSArray" },
        { GL_UNSIGNED_INT_SAMPLER_BUFFER, "usamplerBuffer" },
        { GL_UNSIGNED_INT_SAMPLER_2D_RECT, "usampler2DRect" },
        { GL_IMAGE_1D, "image1D" },
        { GL_IMAGE_2D, "image2D" },
        { GL_IMAGE_3D, "image3D" },
        { GL_IMAGE_2D_RECT, "image2DRect" },
        { GL_IMAGE_CUBE, "imageCube" },
        { GL_IMAGE_BUFFER, "imageBuffer" },
        { GL_IMAGE_1D_ARRAY, "image1DArray" },
        { GL_IMAGE_2D_ARRAY, "image2DArray" },
        { GL_IMAGE_2D_MULTISAMPLE, "image2DMS" },
        { GL_IMAGE_2D_MULTISAMPLE_ARRAY, "image2DMSArray" },
        { GL_INT_IMAGE_1D, "iimage1D" },
        { GL_INT_IMAGE_2D, "iimage2D" },
        { GL_INT_IMAGE_3D, "iimage3D" },
        { GL_INT_IMAGE_2D_RECT, "iimage2DRect" },
        { GL_INT_IMAGE_CUBE, "iimageCube" },
        { GL_INT_IMAGE_BUFFER, "iimageBuffer" },
        { GL_INT_IMAGE_1D_ARRAY, "iimage1DArray" },
        { GL_INT_IMAGE_2D_ARRAY, "iimage2DArray" },
        { GL_INT_IMAGE_2D_MULTISAMPLE, "iimage2DMS" },
        { GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY, "iimage2DMSArray" },
        { GL_UNSIGNED_INT_IMAGE_1D, "uimage1D" },
        { GL_UNSIGNED_INT_IMAGE_2D, "uimage2D" },
        { GL_UNSIGNED_INT_IMAGE_3D, "uimage3D" },
        { GL_UNSIGNED_INT_IMAGE_2D_RECT, "uimage2DRect" },
        { GL_UNSIGNED_INT_IMAGE_CUBE, "uimageCube" },
        { GL_UNSIGNED_INT_IMAGE_BUFFER, "uimageBuffer" },
        { GL_UNSIGNED_INT_IMAGE_1D_ARRAY, "uimage1DArray" },
        { GL_UNSIGNED_INT_IMAGE_2D_ARRAY, "uimage2DArray" },
        { GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE, "uimage2DMS" },
        { GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY, "uimage2DMSArray" },
        { GL_UNSIGNED_INT_ATOMIC_COUNTER, "atomic_uint" },
        { GL_SHADER_STORAGE_BLOCK, "shader_storage_block" } };

extern const char*
glsl_type_name(GLenum glsl_type)
{
    auto it = glsl_type_map.find(glsl_type);
    if (it != glsl_type_map.end()) {
        return it->second;
    }
    LOG(error) << "glsl_type_name: unknown type " << glsl_type;
    return "unknown";
}

void
get_GPUfeatures()
{
    std::cout << std::endl << "Available GPU features:" << std::endl;
    int value;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &value);
    std::cout << "GL_MAX_TEXTURE_SIZE: " << value << std::endl;
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &value);
    std::cout << "GL_MAX_3D_TEXTURE_SIZE: " << value << std::endl;
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &value);
    std::cout << "GL_MAX_CUBE_MAP_TEXTURE_SIZE: " << value << std::endl;
    glGetIntegeri_v(GL_MAX_VIEWPORT_DIMS, 0, &value);
    std::cout << "GL_MAX_VIEWPORT_DIMS: " << value;
    glGetIntegeri_v(GL_MAX_VIEWPORT_DIMS, 1, &value);
    std::cout << " x " << value << std::endl;
    glGetIntegerv(GL_MAX_SAMPLES, &value);
    std::cout << "GL_MAX_SAMPLES: " << value << std::endl;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &value);
    std::cout << "GL_MAX_COLOR_ATTACHMENTS: " << value << std::endl;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &value);
    std::cout << "GL_MAX_SHADER_STORAGE_BLOCK_SIZE: " << value << std::endl;

    std::cout << termcolor::bright_white;
    glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &value);
    std::cout << "GL_MAX_UNIFORM_BUFFER_BINDINGS: " << value << std::endl;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &value);
    std::cout << "GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS: " << value << std::endl;
    glGetIntegerv(GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, &value);
    std::cout << "GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS: " << value << std::endl;
    glGetIntegerv(GL_MAX_IMAGE_UNITS, &value);
    std::cout << "GL_MAX_IMAGE_UNITS: " << value << std::endl;
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &value);
    std::cout << "GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: " << value << std::endl;
    std::cout << termcolor::reset;
}
