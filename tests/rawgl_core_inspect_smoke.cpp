// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl_core.h"

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

const rawgl::ShaderResourceInfo*
find_named_resource(const std::vector<rawgl::ShaderResourceInfo>& resources, const char* name)
{
    for (const rawgl::ShaderResourceInfo& resource : resources) {
        if (resource.name == name) {
            return &resource;
        }
    }

    return nullptr;
}

}  // namespace

int
main()
{
    const rawgl::ShaderInterface computeResult = rawgl::InspectShaderInterface(
        rawgl::ShaderInspectionRequest { rawgl::ShaderProgramKind::compute, { "tests/shaders/atomic_counter.comp" } });

    if (!computeResult.success) {
        std::cerr << "Compute shader inspection failed: " << computeResult.errorMessage << std::endl;
        return 1;
    }
    if (!computeResult.isCompute) {
        std::cerr << "Compute shader inspection returned incorrect pass kind." << std::endl;
        return 1;
    }
    if (!has_named_resource(computeResult.images, "o_out0")) {
        std::cerr << "Compute shader inspection did not report image uniform o_out0." << std::endl;
        return 1;
    }
    const rawgl::ShaderResourceInfo* atomicCounter = find_named_resource(computeResult.atomicCounters, "counter0");
    if (!atomicCounter) {
        std::cerr << "Compute shader inspection did not report atomic counter counter0." << std::endl;
        return 1;
    }
    if (!computeResult.bufferVariables.empty()) {
        std::cerr << "Atomic-counter-only shader unexpectedly reported SSBO variables." << std::endl;
        return 1;
    }
    if (atomicCounter->binding < 0) {
        std::cerr << "Compute shader inspection reported atomic counter without a valid binding." << std::endl;
        return 1;
    }

    const rawgl::ShaderInterface vertfragResult =
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

    const rawgl::ShaderInterface systemUniformResult =
        rawgl::InspectShaderInterface(rawgl::ShaderInspectionRequest {
            rawgl::ShaderProgramKind::compute,
            { "tests/shaders/system_uniforms.comp" },
        });

    if (!systemUniformResult.success) {
        std::cerr << "System-uniform shader inspection failed: " << systemUniformResult.errorMessage << std::endl;
        return 1;
    }
    if (!has_named_resource(systemUniformResult.systemUniforms, "iTime")) {
        std::cerr << "System-uniform shader inspection did not classify iTime correctly." << std::endl;
        return 1;
    }
    if (!has_named_resource(systemUniformResult.systemUniforms, "iFrame")) {
        std::cerr << "System-uniform shader inspection did not classify iFrame correctly." << std::endl;
        return 1;
    }
    if (!has_named_resource(systemUniformResult.systemUniforms, "iPassIndex")) {
        std::cerr << "System-uniform shader inspection did not classify iPassIndex correctly." << std::endl;
        return 1;
    }
    if (!has_named_resource(systemUniformResult.images, "o_out0")) {
        std::cerr << "System-uniform shader inspection did not report o_out0 image output." << std::endl;
        return 1;
    }

    return 0;
}
