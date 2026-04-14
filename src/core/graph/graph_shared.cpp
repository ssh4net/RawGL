// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "graph_shared.h"

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
apply_cull_parameters(Pass::CullMode& cullMode, const std::vector<GraphAttribute>& parameters)
{
    for (const GraphAttribute& parameter : parameters) {
        hres result = hres::ERR;
        for (const Pass::CullModeAttr& cullAttr : Pass::CULL_PARM_ARR) {
            if (cullAttr.name != parameter.name) {
                continue;
            }

            for (const Pass::CullModeVal& possibleValue : cullAttr.possible_values) {
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
