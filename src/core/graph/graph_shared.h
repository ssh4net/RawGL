// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "graph_build.h"

namespace rawgl {

bool
extract_numeric_layout(GLenum uniformType, GraphInputSourceKind& sourceKind, uint8_t& fieldCount);

std::string
build_addressed_resource_name(const std::string& name, bool usesArrayElement, size_t arrayElement);

std::string
build_pass_resource_key(const std::string& name, size_t passIndex);

std::string
build_texture_resource_key(const std::string& path, const std::vector<GraphAttribute>& attributes);

const ShaderResourceInfo*
find_resource_by_name(const std::vector<ShaderResourceInfo>& resources, const std::string& name);

void
apply_texture_attributes(PassInput& input, const std::vector<GraphAttribute>& attributes);

void
apply_mesh_parameters(MeshInput& meshInput, const std::vector<GraphAttribute>& parameters);

void
apply_cull_parameters(SequencePass::CullMode& cullMode, const std::vector<GraphAttribute>& parameters);

}  // namespace rawgl
