// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl_shader_interface_cache.h"

#include "gl_program_manager.h"

#include <mutex>
#include <sstream>
#include <stdexcept>

namespace rawgl {
namespace {

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
    info.size     = 1;
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
load_program(const ShaderProgramKind kind, const std::vector<std::string>& paths)
{
    if (kind == ShaderProgramKind::compute) {
        if (paths.size() != 1) {
            throw std::runtime_error("Compute shaders require exactly one path.");
        }
        return g_glslProgramManager.loadComp(paths[0]);
    }

    if (paths.empty() || paths.size() > 2) {
        throw std::runtime_error("Vertex/fragment shaders require one combined file or two stage files.");
    }

    if (paths.size() == 1) {
        return g_glslProgramManager.loadVertFrag(paths[0]);
    }

    std::string shaderPaths[] { paths[0], paths[1] };
    return g_glslProgramManager.loadVertFrag(shaderPaths);
}

static std::string
build_shader_cache_key(const ShaderProgramKind kind, const std::vector<std::string>& paths)
{
    std::ostringstream stream;
    stream << static_cast<int>(kind);
    for (const std::string& path : paths) {
        stream << '\n' << path;
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
        const ShaderResourceInfo info = make_resource_info(uniformIt.first, uniformIt.second);

        switch (uniformIt.second.type) {
        case GL_SAMPLER_2D: shaderInterface.samplers.push_back(info); break;
        case GL_IMAGE_2D: shaderInterface.images.push_back(info); break;
        default:
            if (is_system_uniform_name(uniformIt.first)) {
                shaderInterface.systemUniforms.push_back(info);
            } else {
                shaderInterface.uniforms.push_back(info);
            }
            break;
        }
    }

    for (const auto& outputIt : program->getOutputs()) {
        shaderInterface.outputs.push_back(make_resource_info(outputIt.first, outputIt.second));
    }

    for (const auto& counterIt : program->getAtomicCounters()) {
        shaderInterface.atomicCounters.push_back(make_resource_info(counterIt.first, *counterIt.second));
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
                             const std::vector<std::string>& paths)
{
    const std::string cacheKey = build_shader_cache_key(kind, paths);

    {
        std::shared_lock<std::shared_mutex> readLock(contextState.shaderCacheMutex);
        auto cacheIt = contextState.shaderCache.find(cacheKey);
        if (cacheIt != contextState.shaderCache.end()) {
            return cacheIt->second;
        }
    }

    RawGLContextState::CachedShaderInterface cached;
    cached.program         = load_program(kind, paths);
    cached.shaderInterface = build_shader_interface(cached.program, kind);

    std::unique_lock<std::shared_mutex> writeLock(contextState.shaderCacheMutex);
    auto [cacheIt, inserted] = contextState.shaderCache.insert({ cacheKey, cached });
    if (!inserted) {
        return cacheIt->second;
    }

    return cached;
}

}  // namespace rawgl
