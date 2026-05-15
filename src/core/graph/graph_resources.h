// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "graph_build.h"

namespace rawgl {

RawGLGraphState::ResourcePlan
build_resource_plan(const RawGLContextState& contextState, const RawGLGraphState::ValidatedGraph& validatedGraph);

std::shared_ptr<Texture>
create_host_texture_resource(const HostImageData& hostImage, const std::string& context);

SequenceExecutionInputOverride
build_sequence_execution_input_override(const GraphInputOverride& inputOverride);

SequenceExecutionMeshUpdate
build_sequence_execution_mesh_update(const GraphMeshUpdate& meshUpdate);

SequenceExecutionMeshOverride
build_sequence_execution_mesh_override(const GraphMeshOverride& meshOverride);

}  // namespace rawgl
