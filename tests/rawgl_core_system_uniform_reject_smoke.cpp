/* 
 * This file is part of the RawGL distribution (https://github.com/ssh4net/RawGL).
 * Copyright (c) 2022-2026 Erium Vladlen.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "rawgl_core.h"

#include <iostream>

int
main()
{
    rawgl::GraphPassDefinition pass;
    pass.programKind            = rawgl::ShaderProgramKind::compute;
    pass.shaderPaths            = { "tests/shaders/system_uniforms.comp" };
    pass.sizeX                  = 1;
    pass.sizeY                  = 1;
    pass.workGroupSizeX         = 1;
    pass.workGroupSizeY         = 1;
    pass.hasExplicitWorkGroupSize = true;
    pass.inputs.push_back(rawgl::GraphInputDefinition {
        "iTime",
        rawgl::GraphInputSourceKind::floatValues,
        {},
        {},
        { 10.0f },
        {},
        "",
        "",
        0,
        "",
        {},
    });
    pass.outputs.push_back(rawgl::GraphOutputDefinition {
        "o_out0",
        "tests/outputs/rawgl_core_system_uniform_reject_smoke.exr",
        "rgba32f",
        4,
        3,
        16,
        "",
        {},
    });

    rawgl::GraphBuildRequest graphRequest;
    graphRequest.definition.verbosity = 0;
    graphRequest.definition.passes.push_back(std::move(pass));

    rawgl::RawGLContext context;
    rawgl::GraphBuildResult buildResult = context.buildGraph(graphRequest);
    if (buildResult.success) {
        std::cerr << "Expected graph build to reject user-provided system uniform input." << std::endl;
        return 1;
    }
    if (buildResult.errorMessage.find("system uniform is engine controlled") == std::string::npos) {
        std::cerr << "Unexpected graph build error: " << buildResult.errorMessage << std::endl;
        return 1;
    }

    return 0;
}
