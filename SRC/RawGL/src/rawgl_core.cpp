// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl_core.h"

#include "command_line_graph.h"
#include "common.h"
#include "log.h"
#include "rawgl_graph_build.h"
#include "timer.h"

#include <iostream>
#include <map>
#include <stdexcept>

const char* APP_NAME    = "RawGL";
const char* APP_AUTHOR  = "Erium Vladlen";
const int APP_VERSION[] = { 2, 0, 0 };

namespace rawgl {

namespace {
static GLenum
infer_texture_type_from_internal_format(const GLenum internalFormat)
{
    switch (internalFormat) {
    case GL_R8:
    case GL_RG8:
    case GL_RGB8:
    case GL_RGBA8: return GL_UNSIGNED_BYTE;
    case GL_R16:
    case GL_RG16:
    case GL_RGB16:
    case GL_RGBA16: return GL_UNSIGNED_SHORT;
    case GL_R32UI:
    case GL_RG32UI:
    case GL_RGB32UI:
    case GL_RGBA32UI: return GL_UNSIGNED_INT;
    case GL_R16F:
    case GL_RG16F:
    case GL_RGB16F:
    case GL_RGBA16F: return GL_HALF_FLOAT;
    case GL_R32F:
    case GL_RG32F:
    case GL_RGB32F:
    case GL_RGBA32F: return GL_FLOAT;
    default: return GL_FLOAT;
    }
}

static std::shared_ptr<Texture>
clone_texture_resource(const std::shared_ptr<Texture>& sourceTexture)
{
    std::shared_ptr<Texture> cloneTexture = std::make_shared<Texture>(
        sourceTexture->getWidth(),
        sourceTexture->getHeight(),
        sourceTexture->getInternalFormat(),
        infer_texture_type_from_internal_format(sourceTexture->getInternalFormat()),
        nullptr);

    GLCall(glCopyImageSubData(sourceTexture->getId(), GL_TEXTURE_2D, 0, 0, 0, 0,
                              cloneTexture->getId(), GL_TEXTURE_2D, 0, 0, 0, 0,
                              sourceTexture->getWidth(), sourceTexture->getHeight(), 1));

    return cloneTexture;
}

static bool
has_sequence_override(const std::vector<SequenceExecutionInputOverride>& inputOverrides,
                      const size_t passIndex,
                      const std::string& inputName)
{
    for (const SequenceExecutionInputOverride& inputOverride : inputOverrides) {
        if (inputOverride.passIndex == passIndex && inputOverride.inputName == inputName) {
            return true;
        }
    }

    return false;
}

}  // namespace

RawGLContext::RawGLContext()
    : m_state(std::make_shared<RawGLContextState>())
{
    Log_Init();
}

RawGLContext::~RawGLContext() = default;

ShaderInterface
RawGLContext::inspectShaderInterface(const ShaderInspectionRequest& request) const
{
    try {
        return load_cached_shader_interface(*m_state, request.kind, request.paths).shaderInterface;
    } catch (const std::exception& exception) {
        ShaderInterface result;
        result.isCompute    = (request.kind == ShaderProgramKind::compute);
        result.errorMessage = exception.what();
        return result;
    }
}

GraphBuildResult
RawGLContext::buildGraph(const GraphBuildRequest& request) const
{
    GraphBuildResult result;

    try {
        auto state = std::make_unique<RawGLGraphState>();
        build_graph_state(*m_state, request, *state);
        result.graph    = std::make_unique<RawGLGraph>(m_state, std::move(state));
        result.success  = true;
    } catch (const std::exception& exception) {
        result.errorMessage = exception.what();
    }

    return result;
}

ContextCacheStats
RawGLContext::cacheStats() const
{
    ContextCacheStats stats;

    {
        std::shared_lock<std::shared_mutex> readLock(m_state->shaderCacheMutex);
        stats.shaderInterfaces = m_state->shaderCache.size();
    }
    {
        std::shared_lock<std::shared_mutex> readLock(m_state->textureCacheMutex);
        stats.textures = m_state->textureCache.size();
    }
    {
        std::shared_lock<std::shared_mutex> readLock(m_state->meshCacheMutex);
        stats.meshesHost = m_state->meshCache.size();
    }
    {
        std::shared_lock<std::shared_mutex> readLock(m_state->meshGpuCacheMutex);
        stats.meshesGpu = m_state->meshGpuCache.size();
    }

    return stats;
}

RawGLGraph::RawGLGraph(std::shared_ptr<RawGLContextState> contextState, std::unique_ptr<RawGLGraphState> state)
    : m_contextState(std::move(contextState))
    , m_state(std::move(state))
{
}

RawGLGraph::~RawGLGraph() = default;

GraphExecutionResult
RawGLGraph::execute(const GraphExecutionRequest& request)
{
    GraphExecutionResult result;

    try {
        std::vector<SequenceExecutionInputOverride> inputOverrides =
            build_sequence_input_overrides(*m_contextState, *m_state, request);
        std::map<std::string, std::shared_ptr<Texture>> persistentTextureSnapshot;
        for (const auto& persistentTexture : m_state->persistentTextures) {
            persistentTextureSnapshot[persistentTexture.first] = clone_texture_resource(persistentTexture.second);
        }
        for (const RawGLGraphState::PersistentInputBinding& persistentInput : m_state->executionPlan.persistentInputs) {
            if (has_sequence_override(inputOverrides, persistentInput.passIndex, persistentInput.inputName)) {
                continue;
            }

            auto textureIt = persistentTextureSnapshot.find(persistentInput.persistentTextureName);
            if (textureIt == persistentTextureSnapshot.end() || !textureIt->second) {
                continue;
            }

            SequenceExecutionInputOverride inputOverride;
            inputOverride.passIndex = persistentInput.passIndex;
            inputOverride.inputName = persistentInput.inputName;
            inputOverride.kind      = SequenceExecutionInputOverrideKind::texture;
            inputOverride.texture   = textureIt->second;
            inputOverrides.push_back(std::move(inputOverride));
        }

        if (!m_state->sequence) {
            m_state->sequence = std::make_unique<Sequence>(m_state->executionPlan.sequenceRuntimeConfig);
        }

        for (const RawGLGraphState::PersistentAtomicCounterBinding& persistentCounter :
             m_state->executionPlan.persistentAtomicCounters) {
            auto counterIt = m_state->persistentAtomicCounters.find(persistentCounter.persistentCounterName);
            if (counterIt == m_state->persistentAtomicCounters.end() || counterIt->second.empty()) {
                continue;
            }

            m_state->sequence->setPassAtomicCounterValues(persistentCounter.passIndex,
                                                          persistentCounter.counterName,
                                                          counterIt->second);
        }

        SequenceSystemUniformState systemUniforms;
        systemUniforms.timeSeconds      = request.systemUniforms.timeSeconds;
        systemUniforms.deltaTimeSeconds = request.systemUniforms.deltaTimeSeconds;
        systemUniforms.frameNumber      = request.systemUniforms.frameNumber;
        systemUniforms.passIndex        = request.systemUniforms.passIndex;
        m_state->sequence->run(systemUniforms, inputOverrides);
        for (const RawGLGraphState::PersistentOutputBinding& persistentOutput : m_state->executionPlan.persistentOutputs) {
            std::shared_ptr<Texture> outputTexture =
                m_state->sequence->getPassOutputTexture(persistentOutput.passIndex, persistentOutput.outputName);
            if (!outputTexture) {
                throw std::runtime_error("persistent output capture failed for " + persistentOutput.outputName);
            }
            m_state->persistentTextures[persistentOutput.persistentTextureName] = outputTexture;
        }
        for (const RawGLGraphState::PersistentAtomicCounterBinding& persistentCounter :
             m_state->executionPlan.persistentAtomicCounters) {
            const std::vector<GLuint> counterValues =
                m_state->sequence->getPassAtomicCounterValues(persistentCounter.passIndex, persistentCounter.counterName);
            if (counterValues.empty()) {
                throw std::runtime_error("persistent atomic counter capture failed for "
                                         + persistentCounter.counterName);
            }
            m_state->persistentAtomicCounters[persistentCounter.persistentCounterName] = counterValues;
        }
        m_state->sequence->releaseRunOutputTextures();
        result.success = true;
    } catch (const std::exception& exception) {
        result.errorMessage = exception.what();
    }

    return result;
}

CoreSession::CoreSession(const GraphBuildRequest& request)
{
    GraphBuildResult buildResult = m_context.buildGraph(request);
    if (!buildResult.success || !buildResult.graph) {
        throw std::runtime_error(buildResult.errorMessage.empty() ? "Failed to build RawGL graph."
                                                                  : buildResult.errorMessage);
    }

    m_graph = std::move(buildResult.graph);
}

CoreSession::~CoreSession() = default;

void
CoreSession::run()
{
    GraphExecutionResult executionResult = m_graph->execute(GraphExecutionRequest {});
    if (!executionResult.success) {
        throw std::runtime_error(executionResult.errorMessage.empty() ? "Failed to execute RawGL graph."
                                                                      : executionResult.errorMessage);
    }
}

int
RunCommandLine(int argc, const char* argv[])
{
    return Run(CommandLineRequest { argc, argv }).exitCode;
}

CommandLineResult
Run(const CommandLineRequest& request)
{
    Log_Init();

    Timer timer;
    CommandLineResult result;

    int immediateExitCode = 0;
    if (Sequence_HandleImmediateCommandLine(request.argc, request.argv, immediateExitCode)) {
        result.exitCode = immediateExitCode;
        result.immediateExit = true;
        return result;
    }

    try {
        RawGLContext context;
        const GraphBuildRequest graphRequest = BuildGraphRequestFromCommandLine(request);
        GraphBuildResult buildResult = context.buildGraph(graphRequest);
        if (!buildResult.success || !buildResult.graph) {
            throw std::runtime_error(buildResult.errorMessage.empty() ? "Failed to build RawGL graph."
                                                                      : buildResult.errorMessage);
        }

        GraphExecutionResult executionResult = buildResult.graph->execute(GraphExecutionRequest {});
        if (!executionResult.success) {
            throw std::runtime_error(executionResult.errorMessage.empty() ? "Failed to execute RawGL graph."
                                                                          : executionResult.errorMessage);
        }

        std::cout << std::endl;
        LOG(info) << "Total processing time : " << timer.nowText() << std::endl;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << std::endl;
        result.exitCode = 1;
        return result;
    }

    result.executed = true;
    return result;
}

ShaderInterface
InspectShaderInterface(const ShaderInspectionRequest& request)
{
    RawGLContext context;
    return context.inspectShaderInterface(request);
}

}  // namespace rawgl
