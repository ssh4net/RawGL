// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl_graph_build.h"

#include "rawgl_graph_resources.h"
#include "rawgl_graph_runtime_plan.h"
#include "rawgl_graph_validation.h"

#include <vector>

namespace rawgl {

void
build_graph_state(const RawGLContextState& contextState, const GraphBuildRequest& request, RawGLGraphState& graphState)
{
    graphState.validatedGraph = validate_graph_definition(contextState, request.definition);
    graphState.resourcePlan   = build_resource_plan(contextState, graphState.validatedGraph);
    graphState.executionPlan  = build_execution_plan(graphState.resourcePlan);
}

std::vector<SequenceExecutionInputOverride>
build_sequence_input_overrides(const RawGLContextState& contextState,
                               const RawGLGraphState& graphState,
                               const GraphExecutionRequest& request)
{
    std::vector<SequenceExecutionInputOverride> inputOverrides;
    inputOverrides.reserve(request.inputOverrides.size());

    for (const GraphInputOverride& inputOverride : request.inputOverrides) {
        validate_execution_input_override(graphState.validatedGraph, inputOverride);
        inputOverrides.push_back(build_sequence_execution_input_override(contextState, inputOverride));
    }

    return inputOverrides;
}

}  // namespace rawgl
