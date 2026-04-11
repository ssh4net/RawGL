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

#include "RawGLCore.h"

#include <filesystem>
#include <iostream>

int
main()
{
    const std::filesystem::path outputPath = "tests/outputs/rawgl_core_graph_smoke.exr";
    std::error_code removeError;
    std::filesystem::remove(outputPath, removeError);

    rawgl::GraphPassDefinition pass;
    pass.programKind            = rawgl::ShaderProgramKind::compute;
    pass.shaderPaths            = { "tests/shaders/atomic_counter.comp" };
    pass.sizeX                  = 1;
    pass.sizeY                  = 1;
    pass.workGroupSizeX         = 1;
    pass.workGroupSizeY         = 1;
    pass.hasExplicitWorkGroupSize = true;
    pass.atomicCounters.push_back(rawgl::GraphAtomicCounterDefinition { "counter0", 5 });
    pass.outputs.push_back(rawgl::GraphOutputDefinition {
        "o_out0",
        outputPath.string(),
        "rgba32f",
        4,
        3,
        16,
        {},
    });

    rawgl::GraphBuildRequest graphRequest;
    graphRequest.definition.verbosity = 0;
    graphRequest.definition.passes.push_back(std::move(pass));

    rawgl::RawGLContext context;
    rawgl::GraphBuildResult buildResult = context.buildGraph(graphRequest);
    if (!buildResult.success || !buildResult.graph) {
        std::cerr << "Graph build failed: " << buildResult.errorMessage << std::endl;
        return 1;
    }

    rawgl::GraphExecutionResult executionResult = buildResult.graph->execute(rawgl::GraphExecutionRequest {
        rawgl::SystemUniformState { 1.0, 1.0 / 24.0, 24, 0 },
    });
    if (!executionResult.success) {
        std::cerr << "Graph execution failed: " << executionResult.errorMessage << std::endl;
        return 1;
    }

    if (!std::filesystem::exists(outputPath)) {
        std::cerr << "Graph execution did not produce output image: " << outputPath << std::endl;
        return 1;
    }

    return 0;
}
