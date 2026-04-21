// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "graph_build.h"

namespace rawgl::io {

void
materialize_graph_build_request(const RawGLContextState& contextState, const GraphBuildRequest& request);

GraphExecutionRequest
materialize_graph_execution_request(const std::shared_ptr<IoRuntimeService>& ioRuntime,
                                   const GraphExecutionRequest& request);

}  // namespace rawgl::io
