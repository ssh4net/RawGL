// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl_core.h"

#include <iostream>
#include <string>

namespace {

const rawgl::MeshMaterialInfo*
find_material(const rawgl::MeshInspectionResult& result, const std::string& name)
{
    for (const rawgl::MeshMaterialInfo& material : result.materials) {
        if (material.name == name) {
            return &material;
        }
    }
    return nullptr;
}

}  // namespace

int
main()
{
    rawgl::MeshInspectionRequest request;
    request.path = "tests/inputs/fullscreen_triangle_material.obj";

    const rawgl::MeshInspectionResult result = rawgl::InspectMeshFile(request);
    if (!result.success) {
        std::cerr << "Mesh inspection failed: " << result.errorMessage << std::endl;
        return 1;
    }
    if (result.vertexCount != 3u || result.texcoordCount != 3u || result.normalCount != 1u) {
        std::cerr << "Unexpected OBJ attribute counts." << std::endl;
        return 1;
    }
    if (result.faceCount != 1u || result.triangleFaceCount != 1u || result.generatedTriangleCount != 1u) {
        std::cerr << "Unexpected OBJ face counts." << std::endl;
        return 1;
    }
    if (!result.hasBounds || result.boundsMin[0] != -1.0f || result.boundsMax[0] != 3.0f) {
        std::cerr << "Unexpected OBJ bounds." << std::endl;
        return 1;
    }
    if (!result.hasUvRange || result.uvMin[0] != 0.0f || result.uvMax[0] != 2.0f) {
        std::cerr << "Unexpected OBJ UV range." << std::endl;
        return 1;
    }

    const rawgl::MeshMaterialInfo* material = find_material(result, "MaterialB");
    if (material == nullptr || material->id != 1u || material->faceCount != 1u) {
        std::cerr << "Unexpected OBJ material mapping." << std::endl;
        return 1;
    }
    if (result.groups.size() != 1u || result.groups[0].name != "MaterialTriangle"
        || result.groups[0].firstFaceIndex != 0u || result.groups[0].faceCount != 1u) {
        std::cerr << "Unexpected OBJ group mapping." << std::endl;
        return 1;
    }

    return 0;
}
