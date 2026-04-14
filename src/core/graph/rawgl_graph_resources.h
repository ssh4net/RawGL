// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "rawgl_graph_build.h"

namespace rawgl {

RawGLGraphState::ResourcePlan
build_resource_plan(const RawGLContextState& contextState, const RawGLGraphState::ValidatedGraph& validatedGraph);

SequenceExecutionInputOverride
build_sequence_execution_input_override(const RawGLContextState& contextState, const GraphInputOverride& inputOverride);

}  // namespace rawgl
