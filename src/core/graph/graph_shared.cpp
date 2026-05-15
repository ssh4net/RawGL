// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include "graph_shared.h"

#include <sstream>
#include <stdexcept>

namespace rawgl {

bool
extract_numeric_layout(const GLenum uniformType, GraphInputSourceKind& sourceKind, uint8_t& fieldCount)
{
    switch (uniformType) {
    case GL_BOOL:
    case GL_INT: sourceKind = GraphInputSourceKind::intValues; fieldCount = 1; return true;
    case GL_BOOL_VEC2:
    case GL_INT_VEC2: sourceKind = GraphInputSourceKind::intValues; fieldCount = 2; return true;
    case GL_BOOL_VEC3:
    case GL_INT_VEC3: sourceKind = GraphInputSourceKind::intValues; fieldCount = 3; return true;
    case GL_BOOL_VEC4:
    case GL_INT_VEC4: sourceKind = GraphInputSourceKind::intValues; fieldCount = 4; return true;
    case GL_UNSIGNED_INT: sourceKind = GraphInputSourceKind::uintValues; fieldCount = 1; return true;
    case GL_UNSIGNED_INT_VEC2: sourceKind = GraphInputSourceKind::uintValues; fieldCount = 2; return true;
    case GL_UNSIGNED_INT_VEC3: sourceKind = GraphInputSourceKind::uintValues; fieldCount = 3; return true;
    case GL_UNSIGNED_INT_VEC4: sourceKind = GraphInputSourceKind::uintValues; fieldCount = 4; return true;
    case GL_FLOAT: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 1; return true;
    case GL_FLOAT_VEC2: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 2; return true;
    case GL_FLOAT_VEC3: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 3; return true;
    case GL_FLOAT_VEC4:
    case GL_FLOAT_MAT2: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 4; return true;
    case GL_FLOAT_MAT2x3:
    case GL_FLOAT_MAT3x2: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 6; return true;
    case GL_FLOAT_MAT2x4:
    case GL_FLOAT_MAT4x2: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 8; return true;
    case GL_FLOAT_MAT3: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 9; return true;
    case GL_FLOAT_MAT3x4:
    case GL_FLOAT_MAT4x3: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 12; return true;
    case GL_FLOAT_MAT4: sourceKind = GraphInputSourceKind::floatValues; fieldCount = 16; return true;
    case GL_DOUBLE: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 1; return true;
    case GL_DOUBLE_VEC2: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 2; return true;
    case GL_DOUBLE_VEC3: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 3; return true;
    case GL_DOUBLE_VEC4:
    case GL_DOUBLE_MAT2: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 4; return true;
    case GL_DOUBLE_MAT2x3:
    case GL_DOUBLE_MAT3x2: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 6; return true;
    case GL_DOUBLE_MAT2x4:
    case GL_DOUBLE_MAT4x2: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 8; return true;
    case GL_DOUBLE_MAT3: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 9; return true;
    case GL_DOUBLE_MAT3x4:
    case GL_DOUBLE_MAT4x3: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 12; return true;
    case GL_DOUBLE_MAT4: sourceKind = GraphInputSourceKind::doubleValues; fieldCount = 16; return true;
    default: break;
    }

    return false;
}

std::string
build_addressed_resource_name(const std::string& name, const bool usesArrayElement, const size_t arrayElement)
{
    if (!usesArrayElement) {
        return name;
    }

    return name + "[" + std::to_string(arrayElement) + "]";
}

std::string
build_pass_resource_key(const std::string& name, const size_t passIndex)
{
    return name + "::" + std::to_string(passIndex);
}

std::string
build_texture_resource_key(const std::string& path, const std::vector<GraphAttribute>& attributes)
{
    std::ostringstream stream;
    stream << "file:" << path;
    for (const GraphAttribute& attribute : attributes) {
        stream << '\x1F' << attribute.name << '=' << attribute.value;
    }
    return stream.str();
}

std::string
build_mesh_resource_key(const GraphMeshDefinition& mesh)
{
    bool assumeTriangles = true;
    for (const GraphAttribute& attribute : mesh.parameters) {
        if (attribute.name == "tris") {
            assumeTriangles = (attribute.value == "true");
            break;
        }
    }

    std::ostringstream stream;
    switch (mesh.sourceKind) {
    case GraphMeshSourceKind::file:
        stream << "file:" << mesh.path;
        break;
    case GraphMeshSourceKind::hostMesh:
        stream << "host:" << mesh.hostMesh.get();
        break;
    case GraphMeshSourceKind::quad:
        stream << "quad";
        break;
    }
    stream << '\x1F' << "tris=" << (assumeTriangles ? 1 : 0);
    return stream.str();
}

const ShaderResourceInfo*
find_resource_by_name(const std::vector<ShaderResourceInfo>& resources, const std::string& name)
{
    for (const ShaderResourceInfo& resource : resources) {
        if (resource.name == name) {
            return &resource;
        }
    }

    return nullptr;
}

void
apply_texture_attributes(PassInput& input, const std::vector<GraphAttribute>& attributes)
{
    for (const GraphAttribute& attribute : attributes) {
        hres result = hres::OK;
        input.eval_tex_attr(result, attribute.name, attribute.value);
        if (result != hres::OK) {
            throw std::runtime_error("in (" + input.uniform->type_name + "): unknown texture attribute " + attribute.name);
        }
        input.attributes[attribute.name] = attribute.value;
    }
}

void
apply_mesh_parameters(MeshInput& meshInput, const std::vector<GraphAttribute>& parameters)
{
    for (const GraphAttribute& parameter : parameters) {
        hres result = hres::ERR;
        for (const MeshInput::MeshParm& meshParm : MeshInput::MESH_PARM_ARR) {
            if (meshParm.name != parameter.name) {
                continue;
            }

            for (const MeshInput::MeshParmValue& possibleValue : meshParm.possible_values) {
                if (possibleValue.key != parameter.value) {
                    continue;
                }

                meshParm.func(meshInput, possibleValue.gl_value);
                result = hres::OK;
                break;
            }

            if (result == hres::OK) {
                break;
            }
        }

        if (result != hres::OK) {
            throw std::runtime_error("pass_mesh: unknown mesh parameter " + parameter.name);
        }
    }
}

void
apply_cull_parameters(SequencePass::CullMode& cullMode, const std::vector<GraphAttribute>& parameters)
{
    for (const GraphAttribute& parameter : parameters) {
        hres result = hres::ERR;
        for (const SequencePass::CullModeAttr& cullAttr : SequencePass::CULL_PARM_ARR) {
            if (cullAttr.name != parameter.name) {
                continue;
            }

            for (const SequencePass::CullModeVal& possibleValue : cullAttr.possible_values) {
                if (possibleValue.key != parameter.value) {
                    continue;
                }

                cullAttr.func(cullMode, possibleValue.gl_value);
                result = hres::OK;
                break;
            }

            if (result == hres::OK) {
                break;
            }
        }

        if (result != hres::OK) {
            throw std::runtime_error("cull: unknown cull parameter " + parameter.name);
        }
    }
}

}  // namespace rawgl
