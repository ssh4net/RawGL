/* 
 * This file is part of the RawGL distribution (https://github.com/ssh4net/RawGL).
 * Copyright (c) 2022-2026 Erium Vladlen.
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

#pragma once

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

class Sequence;

namespace rawgl {

enum class ShaderProgramKind {
    vertfrag,
    compute,
};

struct ShaderInspectionRequest {
    ShaderProgramKind kind = ShaderProgramKind::vertfrag;
    std::vector<std::string> paths;
};

struct ShaderResourceInfo {
    std::string name;
    std::string typeName;
    int location = -1;
    int binding  = -1;
    int offset   = 0;
    int size     = 0;
    unsigned int glType = 0;
};

struct ShaderBufferVariableInfo {
    std::string blockName;
    std::string name;
    std::string typeName;
    int location = -1;
    int binding  = -1;
    int offset   = 0;
    int size     = 0;
    unsigned int glType = 0;
};

struct ShaderInspectionResult {
    bool success = false;
    bool isCompute = false;
    std::string errorMessage;
    std::vector<ShaderResourceInfo> uniforms;
    std::vector<ShaderResourceInfo> outputs;
    std::vector<ShaderResourceInfo> atomicCounters;
    std::vector<ShaderBufferVariableInfo> bufferVariables;
};

struct CommandLineRequest {
    int argc                = 0;
    const char** argv       = nullptr;
};

struct CommandLineResult {
    int exitCode       = 0;
    bool immediateExit = false;
    bool executed      = false;
};

struct SystemUniformState {
    double timeSeconds      = 0.0;
    double deltaTimeSeconds = 0.0;
    int frameNumber         = 0;
    int passIndex           = -1;
};

struct GraphAttribute {
    std::string name;
    std::string value;
};

enum class GraphInputSourceKind {
    intValues,
    uintValues,
    floatValues,
    doubleValues,
    textureFile,
    passOutput,
};

struct GraphInputDefinition {
    std::string name;
    GraphInputSourceKind sourceKind = GraphInputSourceKind::intValues;
    std::vector<int32_t> intValues;
    std::vector<uint32_t> uintValues;
    std::vector<float> floatValues;
    std::vector<double> doubleValues;
    std::string texturePath;
    std::string referencedOutputName;
    size_t referencedPassIndex = 0;
    std::vector<GraphAttribute> attributes;
};

struct GraphAtomicCounterDefinition {
    std::string name;
    uint32_t initialValue = 0;
};

struct GraphOutputDefinition {
    std::string name;
    std::string path;
    std::string format = "rgb32f";
    int channels       = 3;
    int alphaChannel   = -1;
    int bits           = 16;
    std::vector<GraphAttribute> attributes;
};

enum class GraphMeshSourceKind {
    quad,
    file,
};

struct GraphMeshDefinition {
    GraphMeshSourceKind sourceKind = GraphMeshSourceKind::quad;
    std::string path;
    std::vector<GraphAttribute> parameters;
};

struct GraphPassDefinition {
    ShaderProgramKind programKind = ShaderProgramKind::vertfrag;
    std::vector<std::string> shaderPaths;
    int sizeX                     = 512;
    int sizeY                     = 512;
    int workGroupSizeX            = 16;
    int workGroupSizeY            = 16;
    bool hasExplicitWorkGroupSize = false;
    float clearColor[4]           = { 0.0f, 0.0f, 0.0f, 0.0f };
    std::vector<GraphInputDefinition> inputs;
    std::vector<GraphAtomicCounterDefinition> atomicCounters;
    std::vector<GraphOutputDefinition> outputs;
    std::vector<GraphMeshDefinition> meshes;
    std::vector<GraphAttribute> cullParameters;
};

struct GraphDefinition {
    int verbosity = 3;
    std::vector<GraphPassDefinition> passes;
};

struct GraphBuildRequest {
    GraphDefinition definition;
};

struct GraphExecutionRequest {
    SystemUniformState systemUniforms;
};

struct GraphExecutionResult {
    bool success = false;
    std::string errorMessage;
};

class RawGLGraph;

struct GraphBuildResult {
    bool success = false;
    std::string errorMessage;
    std::unique_ptr<RawGLGraph> graph;
};

struct RawGLContextState;

class RawGLContext {
public:
    RawGLContext();
    ~RawGLContext();

    RawGLContext(const RawGLContext&) = delete;
    RawGLContext& operator=(const RawGLContext&) = delete;

    ShaderInspectionResult inspectShaderInterface(const ShaderInspectionRequest& request) const;
    GraphBuildResult buildGraph(const GraphBuildRequest& request) const;

private:
    std::shared_ptr<RawGLContextState> m_state;
};

class RawGLGraph {
public:
    RawGLGraph(std::shared_ptr<RawGLContextState> contextState, std::unique_ptr<Sequence> sequence);
    ~RawGLGraph();

    RawGLGraph(const RawGLGraph&) = delete;
    RawGLGraph& operator=(const RawGLGraph&) = delete;

    GraphExecutionResult execute(const GraphExecutionRequest& request);

    Sequence& sequence();
    const Sequence& sequence() const;

private:
    std::shared_ptr<RawGLContextState> m_contextState;
    std::unique_ptr<Sequence> m_sequence;
};

class CoreSession {
public:
    CoreSession(const GraphBuildRequest& request);
    ~CoreSession();

    CoreSession(const CoreSession&) = delete;
    CoreSession& operator=(const CoreSession&) = delete;

    void run();

    Sequence& sequence();
    const Sequence& sequence() const;

private:
    RawGLContext m_context;
    std::unique_ptr<RawGLGraph> m_graph;
};

CommandLineResult
Run(const CommandLineRequest& request);

ShaderInspectionResult
InspectShaderInterface(const ShaderInspectionRequest& request);

int
RunCommandLine(int argc, const char* argv[]);

}  // namespace rawgl
