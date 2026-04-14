// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "graph_build.h"

namespace rawgl {

RawGLGraphState::ExecutionPlan
build_execution_plan(const RawGLGraphState::ResourcePlan& resourcePlan);

}  // namespace rawgl
