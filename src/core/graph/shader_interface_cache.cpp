// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "shader_interface_cache.h"

#include "program_manager.h"

#include <mutex>
#include <sstream>
#include <stdexcept>

namespace rawgl {
namespace {

static void
set_numeric_shape_metadata(ShaderResourceInfo& info, const GLenum glType)
{
    switch (glType) {
    case GL_BOOL:
    case GL_INT:
    case GL_UNSIGNED_INT:
    case GL_FLOAT:
    case GL_DOUBLE:
        info.vectorWidth   = 1;
        info.matrixColumns = 1;
        info.matrixRows    = 1;
        return;
    case GL_BOOL_VEC2:
    case GL_INT_VEC2:
    case GL_UNSIGNED_INT_VEC2:
    case GL_FLOAT_VEC2:
    case GL_DOUBLE_VEC2:
        info.vectorWidth   = 2;
        info.matrixColumns = 1;
        info.matrixRows    = 1;
        return;
    case GL_BOOL_VEC3:
    case GL_INT_VEC3:
    case GL_UNSIGNED_INT_VEC3:
    case GL_FLOAT_VEC3:
    case GL_DOUBLE_VEC3:
        info.vectorWidth   = 3;
        info.matrixColumns = 1;
        info.matrixRows    = 1;
        return;
    case GL_BOOL_VEC4:
    case GL_INT_VEC4:
    case GL_UNSIGNED_INT_VEC4:
    case GL_FLOAT_VEC4:
    case GL_DOUBLE_VEC4:
        info.vectorWidth   = 4;
        info.matrixColumns = 1;
        info.matrixRows    = 1;
        return;
    case GL_FLOAT_MAT2:
    case GL_DOUBLE_MAT2:
        info.matrixColumns = 2;
        info.matrixRows    = 2;
        return;
    case GL_FLOAT_MAT2x3:
    case GL_DOUBLE_MAT2x3:
        info.matrixColumns = 2;
        info.matrixRows    = 3;
        return;
    case GL_FLOAT_MAT2x4:
    case GL_DOUBLE_MAT2x4:
        info.matrixColumns = 2;
        info.matrixRows    = 4;
        return;
    case GL_FLOAT_MAT3x2:
    case GL_DOUBLE_MAT3x2:
        info.matrixColumns = 3;
        info.matrixRows    = 2;
        return;
    case GL_FLOAT_MAT3:
    case GL_DOUBLE_MAT3:
        info.matrixColumns = 3;
        info.matrixRows    = 3;
        return;
    case GL_FLOAT_MAT3x4:
    case GL_DOUBLE_MAT3x4:
        info.matrixColumns = 3;
        info.matrixRows    = 4;
        return;
    case GL_FLOAT_MAT4x2:
    case GL_DOUBLE_MAT4x2:
        info.matrixColumns = 4;
        info.matrixRows    = 2;
        return;
    case GL_FLOAT_MAT4x3:
    case GL_DOUBLE_MAT4x3:
        info.matrixColumns = 4;
        info.matrixRows    = 3;
        return;
    case GL_FLOAT_MAT4:
    case GL_DOUBLE_MAT4:
        info.matrixColumns = 4;
        info.matrixRows    = 4;
        return;
    default: return;
    }
}

static void
set_texture_shape_metadata(ShaderResourceInfo& info, const GLenum glType)
{
    switch (glType) {
    case GL_SAMPLER_2D:
    case GL_INT_SAMPLER_2D:
    case GL_UNSIGNED_INT_SAMPLER_2D:
    case GL_IMAGE_2D:
    case GL_INT_IMAGE_2D:
    case GL_UNSIGNED_INT_IMAGE_2D:
        info.textureShape = ShaderTextureShape::tex_2d;
        return;
    default: return;
    }
}

static bool
is_sampler_gl_type(const GLenum glType)
{
    switch (glType) {
    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_1D_ARRAY:
    case GL_SAMPLER_2D_ARRAY:
    case GL_SAMPLER_1D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_MULTISAMPLE:
    case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_SAMPLER_CUBE_SHADOW:
    case GL_SAMPLER_BUFFER:
    case GL_SAMPLER_2D_RECT:
    case GL_SAMPLER_2D_RECT_SHADOW:
    case GL_INT_SAMPLER_1D:
    case GL_INT_SAMPLER_2D:
    case GL_INT_SAMPLER_3D:
    case GL_INT_SAMPLER_CUBE:
    case GL_INT_SAMPLER_1D_ARRAY:
    case GL_INT_SAMPLER_2D_ARRAY:
    case GL_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_INT_SAMPLER_BUFFER:
    case GL_INT_SAMPLER_2D_RECT:
    case GL_UNSIGNED_INT_SAMPLER_1D:
    case GL_UNSIGNED_INT_SAMPLER_2D:
    case GL_UNSIGNED_INT_SAMPLER_3D:
    case GL_UNSIGNED_INT_SAMPLER_CUBE:
    case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_BUFFER:
    case GL_UNSIGNED_INT_SAMPLER_2D_RECT: return true;
    default: return false;
    }
}

static bool
is_image_gl_type(const GLenum glType)
{
    switch (glType) {
    case GL_IMAGE_1D:
    case GL_IMAGE_2D:
    case GL_IMAGE_3D:
    case GL_IMAGE_2D_RECT:
    case GL_IMAGE_CUBE:
    case GL_IMAGE_BUFFER:
    case GL_IMAGE_1D_ARRAY:
    case GL_IMAGE_2D_ARRAY:
    case GL_IMAGE_2D_MULTISAMPLE:
    case GL_IMAGE_2D_MULTISAMPLE_ARRAY:
    case GL_INT_IMAGE_1D:
    case GL_INT_IMAGE_2D:
    case GL_INT_IMAGE_3D:
    case GL_INT_IMAGE_2D_RECT:
    case GL_INT_IMAGE_CUBE:
    case GL_INT_IMAGE_BUFFER:
    case GL_INT_IMAGE_1D_ARRAY:
    case GL_INT_IMAGE_2D_ARRAY:
    case GL_INT_IMAGE_2D_MULTISAMPLE:
    case GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
    case GL_UNSIGNED_INT_IMAGE_1D:
    case GL_UNSIGNED_INT_IMAGE_2D:
    case GL_UNSIGNED_INT_IMAGE_3D:
    case GL_UNSIGNED_INT_IMAGE_2D_RECT:
    case GL_UNSIGNED_INT_IMAGE_CUBE:
    case GL_UNSIGNED_INT_IMAGE_BUFFER:
    case GL_UNSIGNED_INT_IMAGE_1D_ARRAY:
    case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
    case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
    case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY: return true;
    default: return false;
    }
}

static void
finalize_resource_info(ShaderResourceInfo& info, const ShaderResourceClass resourceClass)
{
    info.resourceClass = resourceClass;
    info.isArray       = (info.size > 1);
    info.arrayLength   = static_cast<std::size_t>(info.size > 0 ? info.size : 1);
    info.vectorWidth   = 1;
    info.matrixColumns = 1;
    info.matrixRows    = 1;
    set_numeric_shape_metadata(info, info.glType);
    set_texture_shape_metadata(info, info.glType);
}

static ShaderResourceInfo
make_resource_info(const std::string& name, const GLProgramUniform& uniform)
{
    ShaderResourceInfo info;
    info.name     = name;
    info.typeName = uniform.type_name;
    info.location = uniform.location;
    info.size     = uniform.size;
    info.glType   = uniform.type;
    return info;
}

static ShaderResourceInfo
make_resource_info(const std::string& name, const GLProgramOutput& output)
{
    ShaderResourceInfo info;
    info.name     = name;
    info.typeName = output.type_name;
    info.location = static_cast<int>(output.location);
    info.glType   = output.type;
    info.size     = output.size;
    return info;
}

static ShaderResourceInfo
make_resource_info(const std::string& name, const GLProgramBuffers& buffer)
{
    ShaderResourceInfo info;
    info.name     = name;
    info.typeName = buffer.type_name;
    info.location = buffer.location;
    info.binding  = buffer.binding;
    info.offset   = buffer.offset;
    info.size     = buffer.size;
    info.glType   = buffer.type;
    return info;
}

static ShaderBufferVariableInfo
make_buffer_variable_info(const std::string& blockName, const std::string& name, const GLProgramBuffers& buffer)
{
    ShaderBufferVariableInfo info;
    info.blockName = blockName;
    info.name      = name;
    info.typeName  = buffer.type_name;
    info.location  = buffer.location;
    info.binding   = buffer.binding;
    info.offset    = buffer.offset;
    info.size      = buffer.size;
    info.glType    = buffer.type;
    return info;
}

static std::shared_ptr<GLProgram>
load_program(const RawGLContextState& contextState,
             const ShaderProgramKind kind,
             const std::vector<ShaderModuleDefinition>& modules)
{
    std::lock_guard<std::mutex> programLoadLock(contextState.programManagerMutex);

    if (kind == ShaderProgramKind::compute) {
        if (modules.size() != 1u) {
            throw std::runtime_error("Compute shaders require exactly one module.");
        }
        return contextState.programManager.loadCompModule("compute-module", modules[0]);
    }

    if (modules.empty() || modules.size() > 2u) {
        throw std::runtime_error("Vertex/fragment shaders require one combined module or two stage modules.");
    }

    return contextState.programManager.loadVertFragModules("vertfrag-modules", modules);
}

static std::string
build_shader_cache_key(const ShaderProgramKind kind, const std::vector<ShaderModuleDefinition>& modules)
{
    std::ostringstream stream;
    stream << static_cast<int>(kind);
    for (const ShaderModuleDefinition& module : modules) {
        stream << '\n'
               << static_cast<int>(module.role) << ':'
               << static_cast<int>(module.sourceKind) << ':'
               << module.path << ':'
               << module.debugLabel << ':'
               << module.glslText.size() << ':';
        for (const std::byte byteValue : module.spirvBytes) {
            const unsigned int value = std::to_integer<unsigned int>(byteValue);
            const char digits[] = "0123456789abcdef";
            stream << digits[(value >> 4u) & 0x0fu] << digits[value & 0x0fu];
        }
    }
    return stream.str();
}

static bool
is_system_uniform_name(const std::string& name)
{
    return name == "iFBsize" || name == "iFBaspect" || name == "isQuad" || name == "iTime" || name == "iTimeDelta"
           || name == "iFrame" || name == "iPassIndex";
}

}  // namespace

std::vector<ShaderModuleDefinition>
build_file_backed_shader_modules(const ShaderProgramKind kind, const std::vector<std::string>& paths)
{
    std::vector<ShaderModuleDefinition> modules;
    modules.reserve(paths.size());

    if (kind == ShaderProgramKind::compute) {
        if (paths.size() != 1u) {
            throw std::runtime_error("Compute shaders require exactly one path.");
        }

        ShaderModuleDefinition module;
        module.role = ShaderModuleRole::compute;
        module.sourceKind = ShaderModuleSourceKind::filePath;
        module.path = paths[0];
        modules.push_back(std::move(module));
        return modules;
    }

    if (paths.empty() || paths.size() > 2u) {
        throw std::runtime_error("Vertex/fragment shaders require one combined file or two stage files.");
    }

    if (paths.size() == 1u) {
        ShaderModuleDefinition module;
        module.role = ShaderModuleRole::automatic;
        module.sourceKind = ShaderModuleSourceKind::filePath;
        module.path = paths[0];
        modules.push_back(std::move(module));
        return modules;
    }

    ShaderModuleDefinition vertexModule;
    vertexModule.role = ShaderModuleRole::vertex;
    vertexModule.sourceKind = ShaderModuleSourceKind::filePath;
    vertexModule.path = paths[0];
    modules.push_back(std::move(vertexModule));

    ShaderModuleDefinition fragmentModule;
    fragmentModule.role = ShaderModuleRole::fragment;
    fragmentModule.sourceKind = ShaderModuleSourceKind::filePath;
    fragmentModule.path = paths[1];
    modules.push_back(std::move(fragmentModule));
    return modules;
}

ShaderInterface
build_shader_interface(const std::shared_ptr<GLProgram>& program, const ShaderProgramKind kind)
{
    ShaderInterface shaderInterface;
    shaderInterface.isCompute = (kind == ShaderProgramKind::compute);

    if (!program || !program->isValid()) {
        shaderInterface.errorMessage = "Failed to load program for shader inspection.";
        return shaderInterface;
    }

    for (const auto& uniformIt : program->getUniforms()) {
        ShaderResourceInfo info = make_resource_info(uniformIt.first, uniformIt.second);

        if (is_sampler_gl_type(uniformIt.second.type)) {
            finalize_resource_info(info, ShaderResourceClass::sampler);
            shaderInterface.samplers.push_back(info);
            continue;
        }

        if (is_image_gl_type(uniformIt.second.type)) {
            finalize_resource_info(info, ShaderResourceClass::image);
            shaderInterface.images.push_back(info);
            continue;
        }

        if (is_system_uniform_name(uniformIt.first)) {
            finalize_resource_info(info, ShaderResourceClass::system_uniform);
            shaderInterface.systemUniforms.push_back(info);
        } else {
            finalize_resource_info(info, ShaderResourceClass::uniform_numeric);
            shaderInterface.uniforms.push_back(info);
        }
    }

    for (const auto& outputIt : program->getOutputs()) {
        ShaderResourceInfo info = make_resource_info(outputIt.first, outputIt.second);
        finalize_resource_info(info, ShaderResourceClass::output);
        shaderInterface.outputs.push_back(info);
    }

    for (const auto& counterIt : program->getAtomicCounters()) {
        ShaderResourceInfo info = make_resource_info(counterIt.first, *counterIt.second);
        finalize_resource_info(info, ShaderResourceClass::atomic_counter);
        shaderInterface.atomicCounters.push_back(info);
    }

    for (const auto& bufferIt : program->getBufferVariables()) {
        shaderInterface.bufferVariables.push_back(
            make_buffer_variable_info(bufferIt.first, bufferIt.second.first, bufferIt.second.second));
    }

    shaderInterface.success = true;
    return shaderInterface;
}

RawGLContextState::CachedShaderInterface
load_cached_shader_interface(const RawGLContextState& contextState,
                             const ShaderProgramKind kind,
                             const std::vector<ShaderModuleDefinition>& modules)
{
    const std::string cacheKey = build_shader_cache_key(kind, modules);

    {
        std::shared_lock<std::shared_mutex> readLock(contextState.shaderCacheMutex);
        auto cacheIt = contextState.shaderCache.find(cacheKey);
        if (cacheIt != contextState.shaderCache.end()) {
            return cacheIt->second;
        }
    }

    RawGLContextState::CachedShaderInterface cached;
    cached.program         = load_program(contextState, kind, modules);
    cached.shaderInterface = build_shader_interface(cached.program, kind);

    std::unique_lock<std::shared_mutex> writeLock(contextState.shaderCacheMutex);
    auto [cacheIt, inserted] = contextState.shaderCache.insert({ cacheKey, cached });
    if (!inserted) {
        return cacheIt->second;
    }

    return cached;
}

}  // namespace rawgl
