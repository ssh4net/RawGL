// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "gl_utils.h"
#include "io_runtime.h"
#include "program_manager.h"
#include "rawgl/rawgl_core.h"
#include "sequence.h"

#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

namespace rawgl {

struct RawGLContextState {
    struct CachedShaderInterface {
        std::shared_ptr<GLProgram> program;
        ShaderInterface shaderInterface;
    };

    OpenGLHandle glHandle;
    mutable std::mutex programManagerMutex;
    mutable GLProgramManager programManager;
    mutable std::shared_mutex shaderCacheMutex;
    mutable std::map<std::string, CachedShaderInterface> shaderCache;
    mutable std::shared_mutex textureCacheMutex;
    mutable std::map<std::string, std::shared_ptr<Texture>> textureCache;
    mutable std::shared_mutex meshCacheMutex;
    mutable std::map<std::string, std::shared_ptr<SequenceSharedMeshData>> meshCache;
    mutable std::shared_mutex meshGpuCacheMutex;
    mutable std::map<std::string, std::shared_ptr<SequenceSharedGpuMesh>> meshGpuCache;
    std::shared_ptr<rawgl::io::IoRuntimeService> ioRuntime;
};

struct RawGLGraphState {
    struct ValidatedPass {
        GraphPassDefinition definition;
        std::shared_ptr<GLProgram> program;
        ShaderInterface shaderInterface;
    };

    struct ValidatedGraph {
        int verbosity = 3;
        std::vector<ValidatedPass> passes;
    };

    struct ResourcePass {
        std::shared_ptr<GLProgram> program;
        ShaderInterface shaderInterface;
        bool isCompute = false;
        int size[2] { 512, 512 };
        int workGroupSize[2] { 16, 16 };
        float clearColor[4] { 0.0f, 0.0f, 0.0f, 0.0f };
        SequencePass::CullMode cullMode { GL_CW, GL_BACK, true };
        std::vector<GraphInputDefinition> inputs;
        std::vector<GraphAtomicCounterDefinition> atomicCounters;
        std::vector<GraphOutputDefinition> outputs;
        std::vector<GraphMeshDefinition> meshes;
    };

    struct ResourcePlan {
        int verbosity = 3;
        std::vector<ResourcePass> passes;
        std::map<std::string, std::shared_ptr<Texture>> sharedTextures;
        std::map<std::string, std::shared_ptr<SequenceSharedMeshData>> sharedMeshes;
        std::map<std::string, std::shared_ptr<SequenceSharedGpuMesh>> sharedGpuMeshes;
    };

    struct ExecutionPass {
        size_t passIndex = 0;
        bool isCompute = false;
        std::vector<size_t> dependencyPassIndices;
        std::vector<std::string> inputNames;
        std::vector<std::string> outputNames;
    };

    struct PersistentInputBinding {
        size_t passIndex = 0;
        std::string inputName;
        std::string persistentTextureName;
    };

    struct PersistentOutputBinding {
        size_t passIndex = 0;
        std::string outputName;
        std::string persistentTextureName;
    };

    struct PersistentAtomicCounterBinding {
        size_t passIndex = 0;
        std::string counterName;
        std::string persistentCounterName;
    };

    struct FileOutputBinding {
        size_t passIndex = 0;
        std::string outputName;
        std::string path;
        std::map<std::string, std::string> attributes;
        int alphaChannel = -1;
        int bits = 16;
    };

    struct ExecutionPlan {
        SequenceRuntimeConfig sequenceRuntimeConfig;
        std::vector<ExecutionPass> passes;
        std::vector<PersistentInputBinding> persistentInputs;
        std::vector<PersistentOutputBinding> persistentOutputs;
        std::vector<PersistentAtomicCounterBinding> persistentAtomicCounters;
        std::vector<FileOutputBinding> fileOutputs;
    };

    ValidatedGraph validatedGraph;
    ResourcePlan resourcePlan;
    ExecutionPlan executionPlan;
    std::map<std::string, std::shared_ptr<Texture>> persistentTextures;
    std::map<std::string, std::vector<GLuint>> persistentAtomicCounters;
    std::unique_ptr<Sequence> sequence;
};

void
build_graph_state(const RawGLContextState& contextState,
                  const GraphBuildRequest& request,
                  RawGLGraphState& graphState);

std::vector<SequenceExecutionInputOverride>
build_sequence_input_overrides(const RawGLGraphState& graphState,
                               const GraphExecutionRequest& request);

}  // namespace rawgl
