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

#include <iostream>

namespace {

bool
has_named_resource(const std::vector<rawgl::ShaderResourceInfo>& resources, const char* name)
{
    for (const rawgl::ShaderResourceInfo& resource : resources) {
        if (resource.name == name) {
            return true;
        }
    }

    return false;
}

}  // namespace

int
main()
{
    const rawgl::ShaderInspectionResult computeResult = rawgl::InspectShaderInterface(
        rawgl::ShaderInspectionRequest { rawgl::ShaderProgramKind::compute, { "tests/shaders/atomic_counter.comp" } });

    if (!computeResult.success) {
        std::cerr << "Compute shader inspection failed: " << computeResult.errorMessage << std::endl;
        return 1;
    }
    if (!computeResult.isCompute) {
        std::cerr << "Compute shader inspection returned incorrect pass kind." << std::endl;
        return 1;
    }
    if (!has_named_resource(computeResult.uniforms, "o_out0")) {
        std::cerr << "Compute shader inspection did not report image uniform o_out0." << std::endl;
        return 1;
    }
    if (!has_named_resource(computeResult.atomicCounters, "counter0")) {
        std::cerr << "Compute shader inspection did not report atomic counter counter0." << std::endl;
        return 1;
    }

    const rawgl::ShaderInspectionResult vertfragResult =
        rawgl::InspectShaderInterface(rawgl::ShaderInspectionRequest {
            rawgl::ShaderProgramKind::vertfrag,
            { "tests/shaders/single_file_vertfrag.glsl" },
        });

    if (!vertfragResult.success) {
        std::cerr << "Vert/frag shader inspection failed: " << vertfragResult.errorMessage << std::endl;
        return 1;
    }
    if (vertfragResult.isCompute) {
        std::cerr << "Vert/frag shader inspection returned incorrect pass kind." << std::endl;
        return 1;
    }
    if (!has_named_resource(vertfragResult.outputs, "OutSample")) {
        std::cerr << "Vert/frag shader inspection did not report OutSample output." << std::endl;
        return 1;
    }

    return 0;
}
