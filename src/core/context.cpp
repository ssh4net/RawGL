// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl.h"
#include "rawgl/rawgl_cli.h"

#include "cli_graph.h"
#include "cli_parser.h"
#include "common.h"
#include "graph_build.h"
#include "graph_shared.h"
#include "graph_request_materializer.h"
#include "io_runtime.h"
#include "log.h"
#include "shader_interface_cache.h"
#include "timer.h"

#include <cstdlib>
#include <cstring>
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

static size_t
byte_size_for_gl_type(const GLenum type)
{
    switch (type) {
    case GL_UNSIGNED_BYTE: return 1u;
    case GL_UNSIGNED_SHORT:
    case GL_HALF_FLOAT: return 2u;
    case GL_UNSIGNED_INT:
    case GL_FLOAT: return 4u;
    default: break;
    }

    return 0u;
}

static HostImageData
capture_texture_to_host_image(const Texture& texture)
{
    HostImageData hostImage;
    hostImage.width            = texture.getWidth();
    hostImage.height           = texture.getHeight();
    hostImage.channels         = texture.getChannels();
    hostImage.alphaChannel     = texture.getAlphaChannel();
    hostImage.glInternalFormat = texture.getInternalFormat();
    hostImage.glType           = infer_texture_type_from_internal_format(texture.getInternalFormat());

    const size_t bytesPerComponent = byte_size_for_gl_type(hostImage.glType);
    if (bytesPerComponent == 0u) {
        throw std::runtime_error("Unsupported texture readback type");
    }

    const size_t byteCount = static_cast<size_t>(hostImage.width) * static_cast<size_t>(hostImage.height)
                             * static_cast<size_t>(hostImage.channels) * bytesPerComponent;
    void* data             = texture.getData(hostImage.glType);
    if (!data) {
        throw std::runtime_error("Texture readback failed");
    }

    hostImage.bytes.resize(byteCount);
    std::memcpy(hostImage.bytes.data(), data, byteCount);
    std::free(data);
    return hostImage;
}

static const HostImageData&
ensure_captured_output_image(Sequence& sequence,
                             const size_t passIndex,
                             const std::string& outputName,
                             std::map<std::string, HostImageData>& capturedImages)
{
    const std::string outputKey = build_pass_resource_key(outputName, passIndex);
    auto imageIt                = capturedImages.find(outputKey);
    if (imageIt != capturedImages.end()) {
        return imageIt->second;
    }

    std::shared_ptr<Texture> outputTexture = sequence.getPassOutputTexture(passIndex, outputName);
    if (!outputTexture) {
        throw std::runtime_error("output capture failed for " + outputName);
    }

    auto [insertedIt, inserted] =
        capturedImages.insert({ outputKey, capture_texture_to_host_image(*outputTexture) });
    (void)inserted;
    return insertedIt->second;
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

static const rawgl::io::IoRuntimeService&
resolve_io_runtime(const std::shared_ptr<rawgl::io::IoRuntimeService>& ioRuntime)
{
    static const rawgl::io::IoRuntimeService defaultIoRuntime;
    return ioRuntime ? *ioRuntime : defaultIoRuntime;
}

static ShaderInterface
inspect_shader_interface_from_session(const void* userData,
                                      const ShaderProgramKind kind,
                                      const std::vector<std::string>& paths)
{
    const Session* session = static_cast<const Session*>(userData);
    return session->inspectShaderInterface(ShaderInspectionRequest { kind, paths });
}

}  // namespace

RawGLContext::RawGLContext()
    : m_state(std::make_shared<RawGLContextState>())
{
    m_state->ioRuntime = std::make_shared<rawgl::io::IoRuntimeService>();
    Log_Init();
}

RawGLContext::~RawGLContext() = default;

ShaderInterface
RawGLContext::inspectShaderInterface(const ShaderInspectionRequest& request) const
{
    try {
        const std::vector<ShaderModuleDefinition> modules =
            request.modules.empty() ? build_file_backed_shader_modules(request.kind, request.paths) : request.modules;
        return load_cached_shader_interface(*m_state, request.kind, modules).shaderInterface;
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
        rawgl::io::materialize_graph_build_request(*m_state, request);
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

void
RawGLContext::makeContextCurrent() const
{
    m_state->glHandle.makeCurrent();
}

void
RawGLContext::releaseContext() const
{
    m_state->glHandle.releaseCurrent();
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
        const GraphExecutionRequest materializedRequest =
            rawgl::io::materialize_graph_execution_request(m_contextState->ioRuntime, request);
        std::vector<SequenceExecutionInputOverride> inputOverrides =
            build_sequence_input_overrides(*m_state, materializedRequest);
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
        std::map<std::string, HostImageData> capturedOutputImages;
        for (const RawGLGraphState::FileOutputBinding& fileOutput : m_state->executionPlan.fileOutputs) {
            const HostImageData& outputImage =
                ensure_captured_output_image(*m_state->sequence,
                                             fileOutput.passIndex,
                                             fileOutput.outputName,
                                             capturedOutputImages);

            rawgl::io::OutputWriteRequest request;
            request.path         = fileOutput.path;
            request.attributes   = fileOutput.attributes;
            request.alphaChannel = fileOutput.alphaChannel;
            request.bits         = fileOutput.bits;
            request.image        = &outputImage;
            resolve_io_runtime(m_contextState->ioRuntime).saveImageOutput(request);
        }
        for (size_t passIndex = 0; passIndex < m_state->resourcePlan.passes.size(); ++passIndex) {
            const RawGLGraphState::ResourcePass& resourcePass = m_state->resourcePlan.passes[passIndex];

            for (const GraphOutputDefinition& outputDefinition : resourcePass.outputs) {
                if (!outputDefinition.captureToHost) {
                    continue;
                }

                const std::string addressedOutputName =
                    build_addressed_resource_name(outputDefinition.name,
                                                  outputDefinition.usesArrayElement,
                                                  outputDefinition.arrayElement);
                result.capturedOutputs[build_pass_resource_key(addressedOutputName, passIndex)] =
                    ensure_captured_output_image(*m_state->sequence,
                                                 passIndex,
                                                 addressedOutputName,
                                                 capturedOutputImages);
            }

            for (const GraphAtomicCounterDefinition& counterDefinition : resourcePass.atomicCounters) {
                const std::vector<GLuint> counterValues =
                    m_state->sequence->getPassAtomicCounterValues(passIndex, counterDefinition.name);
                if (counterValues.empty()) {
                    throw std::runtime_error("atomic counter capture failed for " + counterDefinition.name);
                }

                const std::string addressedCounterName =
                    build_addressed_resource_name(counterDefinition.name,
                                                  counterDefinition.usesArrayElement,
                                                  counterDefinition.arrayElement);
                const std::string counterKey = build_pass_resource_key(addressedCounterName, passIndex);
                if (counterDefinition.usesArrayElement) {
                    if (counterDefinition.arrayElement >= counterValues.size()) {
                        throw std::runtime_error("atomic counter capture failed for " + addressedCounterName);
                    }
                    result.capturedAtomicCounters[counterKey] = { counterValues[counterDefinition.arrayElement] };
                } else {
                    result.capturedAtomicCounters[counterKey].assign(counterValues.begin(), counterValues.end());
                }
            }
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
    if (HandleImmediateCommandLine(request.argc, request.argv, immediateExitCode)) {
        result.exitCode = immediateExitCode;
        result.immediateExit = true;
        return result;
    }

    try {
        Session session;
        const Workflow workflow = BuildWorkflowFromCommandLine(
            request,
            ShaderInterfaceInspector { &session, inspect_shader_interface_from_session });
        const RunResult executionResult = session.run(workflow);
        if (!executionResult.success) {
            throw std::runtime_error(executionResult.errorMessage.empty() ? "Failed to execute RawGL workflow."
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
