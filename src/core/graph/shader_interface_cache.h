// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#pragma once

#include "graph_build.h"

namespace rawgl {

ShaderInterface
build_shader_interface(const std::shared_ptr<GLProgram>& program, ShaderProgramKind kind);

RawGLContextState::CachedShaderInterface
load_cached_shader_interface(const RawGLContextState& contextState,
                             ShaderProgramKind kind,
                             const std::vector<ShaderModuleDefinition>& modules);

std::vector<ShaderModuleDefinition>
build_file_backed_shader_modules(ShaderProgramKind kind, const std::vector<std::string>& paths);

}  // namespace rawgl
