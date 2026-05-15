// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl.h"

#include <GL/glew.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

namespace {

const char* VERTEX_SHADER = R"(#version 450 core
layout(location = 0) in vec3 position;
layout(location = 4) in uint id0;
layout(location = 0) flat out uint v_id0;
void main()
{
    v_id0 = id0;
    gl_Position = vec4(position, 1.0);
}
)";

const char* FRAGMENT_SHADER = R"(#version 450 core
layout(location = 0) flat in uint v_id0;
layout(location = 0) out uint TriangleId;
void main()
{
    TriangleId = v_id0;
}
)";

rawgl::ShaderModuleDefinition
make_shader_module(const rawgl::ShaderModuleRole role, const char* source, const char* label)
{
    rawgl::ShaderModuleDefinition module;
    module.role       = role;
    module.sourceKind = rawgl::ShaderModuleSourceKind::glslText;
    module.glslText   = source;
    module.debugLabel = label;
    return module;
}

std::shared_ptr<rawgl::HostMeshData>
make_triangle_mesh(const float x0,
                   const float y0,
                   const float x1,
                   const float y1,
                   const float x2,
                   const float y2,
                   const uint32_t id)
{
    std::shared_ptr<rawgl::HostMeshData> mesh = std::make_shared<rawgl::HostMeshData>();
    mesh->positions = { x0, y0, 0.0f, x1, y1, 0.0f, x2, y2, 0.0f };
    mesh->indices = { 0u, 1u, 2u };
    mesh->id0 = { id, id, id };
    return mesh;
}

rawgl::Pass
make_mesh_id_pass(const std::shared_ptr<rawgl::HostMeshData>& mesh)
{
    rawgl::Pass pass;
    pass.programKind = rawgl::ShaderProgramKind::vertfrag;
    pass.shaderModules.push_back(
        make_shader_module(rawgl::ShaderModuleRole::vertex, VERTEX_SHADER, "mesh_override_smoke_vertex"));
    pass.shaderModules.push_back(
        make_shader_module(rawgl::ShaderModuleRole::fragment, FRAGMENT_SHADER, "mesh_override_smoke_fragment"));
    pass.sizeX = 32;
    pass.sizeY = 32;
    pass.meshes.push_back(rawgl::MeshBinding { "target", rawgl::MeshSourceKind::hostMesh, "", mesh, {} });
    pass.outputs.push_back(rawgl::CapturedOutput("TriangleId", "r32ui", 1, -1, 32));
    pass.cullParameters.push_back(rawgl::Attribute { "enable", "false" });
    return pass;
}

const rawgl::HostImageData*
find_output(const rawgl::RunResult& result, const char* key)
{
    const std::map<std::string, rawgl::HostImageData>::const_iterator it = result.capturedOutputs.find(key);
    if (it == result.capturedOutputs.end()) {
        std::cerr << "Missing captured output: " << key << std::endl;
        return nullptr;
    }
    return &it->second;
}

bool
validate_r32ui_output(const rawgl::HostImageData& image, const char* key)
{
    const size_t expectedBytes = static_cast<size_t>(image.width) * static_cast<size_t>(image.height)
                                 * sizeof(uint32_t);
    if (image.width != 32 || image.height != 32 || image.channels != 1 || image.alphaChannel != -1
        || image.glInternalFormat != GL_R32UI || image.glType != GL_UNSIGNED_INT
        || image.bytes.size() != expectedBytes) {
        std::cerr << "Captured output metadata is invalid for " << key << std::endl;
        return false;
    }
    return true;
}

uint32_t
max_region_value(const rawgl::HostImageData& image, const int xBegin, const int xEnd)
{
    const uint32_t* pixels = reinterpret_cast<const uint32_t*>(image.bytes.data());
    uint32_t maxValue = 0u;
    for (int y = 0; y < image.height; ++y) {
        for (int x = xBegin; x < xEnd; ++x) {
            const uint32_t value = pixels[static_cast<size_t>(y) * static_cast<size_t>(image.width)
                                          + static_cast<size_t>(x)];
            if (value > maxValue) {
                maxValue = value;
            }
        }
    }
    return maxValue;
}

bool
verify_output_regions(const rawgl::RunResult& result,
                      const char* key,
                      const uint32_t minLeft,
                      const uint32_t maxLeft,
                      const uint32_t minRight,
                      const uint32_t maxRight)
{
    const rawgl::HostImageData* image = find_output(result, key);
    if (image == nullptr || !validate_r32ui_output(*image, key)) {
        return false;
    }

    const uint32_t left = max_region_value(*image, 0, 16);
    const uint32_t right = max_region_value(*image, 16, 32);
    if (left < minLeft || left > maxLeft || right < minRight || right > maxRight) {
        std::cerr << "Unexpected captured regions in " << key << ": left=" << left << " right=" << right
                  << std::endl;
        return false;
    }
    return true;
}

rawgl::RunResult
run_prepared(rawgl::PreparedWorkflow& prepared, const rawgl::RunSettings& settings)
{
    rawgl::RunResult result = prepared.run(settings);
    if (!result.success) {
        std::cerr << "Workflow execution failed: " << result.errorMessage << std::endl;
    }
    return result;
}

}  // namespace

int
main()
{
    const std::shared_ptr<rawgl::HostMeshData> original =
        make_triangle_mesh(0.10f, -0.75f, 0.95f, -0.75f, 0.55f, 0.75f, 3u);
    const std::shared_ptr<rawgl::HostMeshData> replacement =
        make_triangle_mesh(-0.95f, -0.75f, -0.10f, -0.75f, -0.55f, 0.75f, 11u);

    rawgl::Workflow workflow;
    workflow.verbosity = 0;
    workflow.passes.push_back(make_mesh_id_pass(original));
    workflow.passes.push_back(make_mesh_id_pass(original));

    rawgl::Session session;
    rawgl::PrepareResult prepareResult = session.prepare(workflow);
    if (!prepareResult.success || !prepareResult.workflow) {
        std::cerr << "Workflow preparation failed: " << prepareResult.errorMessage << std::endl;
        return 1;
    }

    const rawgl::SessionStats stats = session.stats();
    if (stats.meshesHost != 0u || stats.meshesGpu != 0u) {
        std::cerr << "Host-memory meshes should not populate the file-backed session mesh cache." << std::endl;
        return 1;
    }

    rawgl::RunSettings scopedSettings;
    scopedSettings.meshOverrides.push_back(rawgl::MeshOverride { true, 0u, "target", replacement });
    const rawgl::RunResult scopedRun = run_prepared(*prepareResult.workflow, scopedSettings);
    if (!scopedRun.success) {
        return 1;
    }
    if (!verify_output_regions(scopedRun, "TriangleId::0", 11u, 11u, 0u, 0u)
        || !verify_output_regions(scopedRun, "TriangleId::1", 0u, 0u, 3u, 3u)) {
        return 1;
    }

    rawgl::RunSettings unscopedSettings;
    unscopedSettings.meshOverrides.push_back(rawgl::MeshOverride { false, 0u, "target", replacement });
    const rawgl::RunResult unscopedRun = run_prepared(*prepareResult.workflow, unscopedSettings);
    if (!unscopedRun.success) {
        return 1;
    }
    if (!verify_output_regions(unscopedRun, "TriangleId::0", 11u, 11u, 0u, 0u)
        || !verify_output_regions(unscopedRun, "TriangleId::1", 11u, 11u, 0u, 0u)) {
        return 1;
    }

    const rawgl::RunResult restoredRun = run_prepared(*prepareResult.workflow, rawgl::RunSettings {});
    if (!restoredRun.success) {
        return 1;
    }
    if (!verify_output_regions(restoredRun, "TriangleId::0", 0u, 0u, 3u, 3u)
        || !verify_output_regions(restoredRun, "TriangleId::1", 0u, 0u, 3u, 3u)) {
        return 1;
    }

    return 0;
}
