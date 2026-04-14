// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "rawgl_graph_build.h"

namespace rawgl {

RawGLGraphState::ValidatedGraph
validate_graph_definition(const RawGLContextState& contextState, const GraphDefinition& definition);

void
validate_execution_input_override(const RawGLGraphState::ValidatedGraph& graph, const GraphInputOverride& inputOverride);

}  // namespace rawgl
