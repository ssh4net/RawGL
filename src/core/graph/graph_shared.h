// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "graph_build.h"

namespace rawgl {

bool
extract_numeric_layout(GLenum uniformType, GraphInputSourceKind& sourceKind, uint8_t& fieldCount);

const ShaderResourceInfo*
find_resource_by_name(const std::vector<ShaderResourceInfo>& resources, const std::string& name);

void
apply_texture_attributes(PassInput& input, const std::vector<GraphAttribute>& attributes);

void
apply_mesh_parameters(MeshInput& meshInput, const std::vector<GraphAttribute>& parameters);

void
apply_cull_parameters(Pass::CullMode& cullMode, const std::vector<GraphAttribute>& parameters);

}  // namespace rawgl
