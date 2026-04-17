// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (c) 2022-2026 Erium Vladlen.

#include "rawgl/rawgl.h"

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

bool
expect_resource_metadata(const rawgl::ShaderResourceInfo& resource,
                         rawgl::ShaderResourceClass resourceClass,
                         rawgl::ShaderTextureShape textureShape,
                         bool isArray,
                         std::size_t arrayLength,
                         int vectorWidth,
                         int matrixColumns,
                         int matrixRows)
{
    return resource.resourceClass == resourceClass && resource.textureShape == textureShape
           && resource.isArray == isArray && resource.arrayLength == arrayLength
           && resource.vectorWidth == vectorWidth && resource.matrixColumns == matrixColumns
           && resource.matrixRows == matrixRows;
}

}  // namespace

int
main()
{
    const rawgl::Session session;
    const rawgl::ShaderInterface computeResult = session.inspectShaderInterface(
        rawgl::ShaderInspectionRequest { rawgl::ShaderProgramKind::compute, { "tests/shaders/atomic_counter.comp" } });

    if (!computeResult.success) {
        std::cerr << "Compute shader inspection failed: " << computeResult.errorMessage << std::endl;
        return 1;
    }
    if (!computeResult.isCompute) {
        std::cerr << "Compute shader inspection returned incorrect pass kind." << std::endl;
        return 1;
    }
    const rawgl::ShaderResourceInfo* computeImage = find_named_resource(computeResult.images, "o_out0");
    if (!computeImage) {
        std::cerr << "Compute shader inspection did not report image uniform o_out0." << std::endl;
        return 1;
    }
    if (!expect_resource_metadata(*computeImage,
                                  rawgl::ShaderResourceClass::image,
                                  rawgl::ShaderTextureShape::tex_2d,
                                  false,
                                  1,
                                  1,
                                  1,
                                  1)) {
        std::cerr << "Compute shader inspection reported incorrect metadata for image uniform o_out0." << std::endl;
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
    if (!expect_resource_metadata(*atomicCounter,
                                  rawgl::ShaderResourceClass::atomic_counter,
                                  rawgl::ShaderTextureShape::unknown,
                                  false,
                                  1,
                                  1,
                                  1,
                                  1)) {
        std::cerr << "Compute shader inspection reported incorrect metadata for atomic counter counter0." << std::endl;
        return 1;
    }

    const rawgl::ShaderInterface inlineComputeResult = session.inspectShaderInterface(rawgl::ShaderInspectionRequest {
        rawgl::ShaderProgramKind::compute,
        {},
        { rawgl::ShaderModuleDefinition {
            rawgl::ShaderModuleRole::compute,
            rawgl::ShaderModuleSourceKind::glslText,
            "",
            R"(#version 450 core

layout(rgba32f) writeonly uniform image2D o_out0;
layout(local_size_x = 1, local_size_y = 1) in;

void main()
{
    imageStore(o_out0, ivec2(0, 0), vec4(1.0));
}
)",
            {},
            "inspect_inline_compute",
        } },
    });
    if (!inlineComputeResult.success) {
        std::cerr << "Inline compute shader inspection failed: " << inlineComputeResult.errorMessage << std::endl;
        return 1;
    }
    if (!has_named_resource(inlineComputeResult.images, "o_out0")) {
        std::cerr << "Inline compute shader inspection did not report image uniform o_out0." << std::endl;
        return 1;
    }

    const rawgl::ShaderInterface vertfragResult =
        session.inspectShaderInterface(rawgl::ShaderInspectionRequest {
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

    const rawgl::ShaderInterface arrayResult =
        session.inspectShaderInterface(rawgl::ShaderInspectionRequest {
            rawgl::ShaderProgramKind::vertfrag,
            { "tests/shaders/empty.vert", "tests/shaders/array_reflection.frag" },
        });

    if (!arrayResult.success) {
        std::cerr << "Array-reflection shader inspection failed: " << arrayResult.errorMessage << std::endl;
        return 1;
    }
    const rawgl::ShaderResourceInfo* weightArray = find_named_resource(arrayResult.uniforms, "weights");
    if (!weightArray) {
        std::cerr << "Array-reflection shader inspection did not report weights uniform array." << std::endl;
        return 1;
    }
    if (!expect_resource_metadata(*weightArray,
                                  rawgl::ShaderResourceClass::uniform_numeric,
                                  rawgl::ShaderTextureShape::unknown,
                                  true,
                                  4,
                                  1,
                                  1,
                                  1)) {
        std::cerr << "Array-reflection shader inspection reported incorrect metadata for weights." << std::endl;
        return 1;
    }
    const rawgl::ShaderResourceInfo* outputArray = find_named_resource(arrayResult.outputs, "OutArray");
    if (!outputArray) {
        std::cerr << "Array-reflection shader inspection did not report OutArray output array." << std::endl;
        return 1;
    }
    if (!expect_resource_metadata(*outputArray,
                                  rawgl::ShaderResourceClass::output,
                                  rawgl::ShaderTextureShape::unknown,
                                  true,
                                  2,
                                  4,
                                  1,
                                  1)) {
        std::cerr << "Array-reflection shader inspection reported incorrect metadata for OutArray." << std::endl;
        return 1;
    }

    const rawgl::ShaderInterface vectorResult =
        session.inspectShaderInterface(rawgl::ShaderInspectionRequest {
            rawgl::ShaderProgramKind::compute,
            { "tests/shaders/vec2_uniform.comp" },
        });

    if (!vectorResult.success) {
        std::cerr << "Vector-uniform shader inspection failed: " << vectorResult.errorMessage << std::endl;
        return 1;
    }
    const rawgl::ShaderResourceInfo* vec2Uniform = find_named_resource(vectorResult.uniforms, "u_value");
    if (!vec2Uniform) {
        std::cerr << "Vector-uniform shader inspection did not report u_value." << std::endl;
        return 1;
    }
    if (!expect_resource_metadata(*vec2Uniform,
                                  rawgl::ShaderResourceClass::uniform_numeric,
                                  rawgl::ShaderTextureShape::unknown,
                                  false,
                                  1,
                                  2,
                                  1,
                                  1)) {
        std::cerr << "Vector-uniform shader inspection reported incorrect metadata for u_value." << std::endl;
        return 1;
    }

    const rawgl::ShaderInterface systemUniformResult =
        session.inspectShaderInterface(rawgl::ShaderInspectionRequest {
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
    const rawgl::ShaderResourceInfo* timeUniform = find_named_resource(systemUniformResult.systemUniforms, "iTime");
    if (!timeUniform
        || !expect_resource_metadata(*timeUniform,
                                     rawgl::ShaderResourceClass::system_uniform,
                                     rawgl::ShaderTextureShape::unknown,
                                     false,
                                     1,
                                     1,
                                     1,
                                     1)) {
        std::cerr << "System-uniform shader inspection reported incorrect metadata for iTime." << std::endl;
        return 1;
    }
    if (!has_named_resource(systemUniformResult.images, "o_out0")) {
        std::cerr << "System-uniform shader inspection did not report o_out0 image output." << std::endl;
        return 1;
    }

    return 0;
}
