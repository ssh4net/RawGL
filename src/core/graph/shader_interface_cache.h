// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "graph_build.h"

namespace rawgl {

ShaderInterface
build_shader_interface(const std::shared_ptr<GLProgram>& program, ShaderProgramKind kind);

RawGLContextState::CachedShaderInterface
load_cached_shader_interface(const RawGLContextState& contextState,
                             ShaderProgramKind kind,
                             const std::vector<std::string>& paths);

}  // namespace rawgl
