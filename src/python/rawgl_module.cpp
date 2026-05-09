// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2022-2026 Erium Vladlen.

#include <nanobind/nanobind.h>
#include <nanobind/stl/array.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <rawgl/rawgl_batch.h>
#include <rawgl/rawgl.h>
#include <rawgl/rawgl_io.h>

namespace nb = nanobind;

namespace {

struct PythonPrepareResult {
    bool success = false;
    std::string errorMessage;
    std::shared_ptr<rawgl::PreparedWorkflow> workflow;
};

struct PythonIoPrepareResult {
    bool success = false;
    std::string errorMessage;
    std::shared_ptr<rawgl::io::PreparedIoWorkflow> workflow;
};

struct PythonBatchPrepareResult {
    bool success = false;
    std::string errorMessage;
    std::shared_ptr<rawgl::batch::BatchPreparedWorkflow> workflow;
};

nb::bytes
to_python_bytes(const std::vector<std::byte>& data)
{
    if (data.empty()) {
        return nb::bytes();
    }
    return nb::bytes(reinterpret_cast<const char*>(data.data()), data.size());
}

std::vector<std::byte>
from_python_bytes(const nb::object& value)
{
    std::vector<std::byte> result;

    PyObject* object = value.ptr();
    if (PyBytes_Check(object) != 0) {
        char* bytesData = nullptr;
        Py_ssize_t bytesSize = 0;
        if (PyBytes_AsStringAndSize(object, &bytesData, &bytesSize) != 0) {
            throw std::runtime_error("failed to extract Python bytes payload");
        }
        result.resize(static_cast<size_t>(bytesSize));
        if (bytesSize > 0) {
            std::memcpy(result.data(), bytesData, static_cast<size_t>(bytesSize));
        }
        return result;
    }

    if (PyByteArray_Check(object) != 0) {
        const Py_ssize_t bytesSize = PyByteArray_Size(object);
        result.resize(static_cast<size_t>(bytesSize));
        if (bytesSize > 0) {
            std::memcpy(result.data(), PyByteArray_AsString(object), static_cast<size_t>(bytesSize));
        }
        return result;
    }

    Py_buffer buffer;
    if (PyObject_GetBuffer(object, &buffer, PyBUF_CONTIG_RO) == 0) {
        result.resize(static_cast<size_t>(buffer.len));
        if (buffer.len > 0) {
            std::memcpy(result.data(), buffer.buf, static_cast<size_t>(buffer.len));
        }
        PyBuffer_Release(&buffer);
        return result;
    }

    throw std::runtime_error("expected a bytes-like object");
}

std::shared_ptr<rawgl::HostImageData>
make_rgba32f_host_image(const int width, const int height, const std::vector<float>& values)
{
    auto image = std::make_shared<rawgl::HostImageData>();
    image->width            = width;
    image->height           = height;
    image->channels         = 4;
    image->alphaChannel     = 3;
    image->glInternalFormat = 0x8814u;  // GL_RGBA32F
    image->glType           = 0x1406u;  // GL_FLOAT

    if (width <= 0 || height <= 0) {
        return image;
    }

    const size_t expectedValueCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    if (values.size() != expectedValueCount) {
        throw std::runtime_error("RGBA32F host image expects width * height * 4 float values");
    }

    image->bytes.resize(expectedValueCount * sizeof(float));
    std::memcpy(image->bytes.data(), values.data(), image->bytes.size());
    return image;
}

std::vector<float>
host_image_to_rgba32f(const rawgl::HostImageData& image)
{
    std::vector<float> values;
    if (image.bytes.empty()) {
        return values;
    }
    if (image.bytes.size() % sizeof(float) != 0u) {
        throw std::runtime_error("Host image byte payload is not float-aligned");
    }
    values.resize(image.bytes.size() / sizeof(float));
    std::memcpy(values.data(), image.bytes.data(), image.bytes.size());
    return values;
}

const char*
rawgl_python_status()
{
    return "rawgl nanobind facade";
}

}  // namespace

NB_MODULE(_rawgl, module)
{
    module.doc() = "RawGL Python workflow facade";
    module.attr("__version__") = nb::str(RAWGL_VERSION_STRING);
    module.def("status", &rawgl_python_status, "Return the current RawGL Python binding status string.");

    module.def("make_rgba32f_host_image",
               &make_rgba32f_host_image,
               nb::arg("width"),
               nb::arg("height"),
               nb::arg("values"),
               "Create a shared HostImageData object from tightly packed RGBA32F float values.");
    module.def("host_image_to_rgba32f",
               &host_image_to_rgba32f,
               nb::arg("image"),
               "Decode HostImageData bytes into a flat RGBA32F float list.");

    nb::enum_<rawgl::ShaderProgramKind>(module, "ShaderProgramKind")
        .value("vertfrag", rawgl::ShaderProgramKind::vertfrag)
        .value("compute", rawgl::ShaderProgramKind::compute);

    nb::enum_<rawgl::ShaderModuleSourceKind>(module, "ShaderModuleSourceKind")
        .value("file_path", rawgl::ShaderModuleSourceKind::filePath)
        .value("glsl_text", rawgl::ShaderModuleSourceKind::glslText)
        .value("spirv_binary", rawgl::ShaderModuleSourceKind::spirvBinary);

    nb::enum_<rawgl::ShaderModuleRole>(module, "ShaderModuleRole")
        .value("automatic", rawgl::ShaderModuleRole::automatic)
        .value("vertex", rawgl::ShaderModuleRole::vertex)
        .value("fragment", rawgl::ShaderModuleRole::fragment)
        .value("compute", rawgl::ShaderModuleRole::compute);

    nb::enum_<rawgl::ShaderResourceClass>(module, "ShaderResourceClass")
        .value("unknown", rawgl::ShaderResourceClass::unknown)
        .value("uniform_numeric", rawgl::ShaderResourceClass::uniform_numeric)
        .value("sampler", rawgl::ShaderResourceClass::sampler)
        .value("image", rawgl::ShaderResourceClass::image)
        .value("output", rawgl::ShaderResourceClass::output)
        .value("atomic_counter", rawgl::ShaderResourceClass::atomic_counter)
        .value("system_uniform", rawgl::ShaderResourceClass::system_uniform)
        .value("buffer_variable", rawgl::ShaderResourceClass::buffer_variable);

    nb::enum_<rawgl::ShaderTextureShape>(module, "ShaderTextureShape")
        .value("unknown", rawgl::ShaderTextureShape::unknown)
        .value("tex_2d", rawgl::ShaderTextureShape::tex_2d);

    nb::enum_<rawgl::InputSourceKind>(module, "InputSourceKind")
        .value("int_values", rawgl::InputSourceKind::intValues)
        .value("uint_values", rawgl::InputSourceKind::uintValues)
        .value("float_values", rawgl::InputSourceKind::floatValues)
        .value("double_values", rawgl::InputSourceKind::doubleValues)
        .value("host_texture", rawgl::InputSourceKind::hostTexture)
        .value("pass_output", rawgl::InputSourceKind::passOutput)
        .value("workflow_texture", rawgl::InputSourceKind::workflowTexture);

    nb::enum_<rawgl::MeshSourceKind>(module, "MeshSourceKind")
        .value("quad", rawgl::MeshSourceKind::quad)
        .value("file", rawgl::MeshSourceKind::file);

    nb::class_<rawgl::Attribute>(module, "Attribute")
        .def(nb::init<>())
        .def_rw("name", &rawgl::Attribute::name)
        .def_rw("value", &rawgl::Attribute::value);

    nb::class_<rawgl::HostImageData>(module, "HostImageData")
        .def(nb::init<>())
        .def_rw("width", &rawgl::HostImageData::width)
        .def_rw("height", &rawgl::HostImageData::height)
        .def_rw("channels", &rawgl::HostImageData::channels)
        .def_rw("alpha_channel", &rawgl::HostImageData::alphaChannel)
        .def_rw("gl_internal_format", &rawgl::HostImageData::glInternalFormat)
        .def_rw("gl_type", &rawgl::HostImageData::glType)
        .def_prop_rw(
            "bytes",
            [](const rawgl::HostImageData& image) { return to_python_bytes(image.bytes); },
            [](rawgl::HostImageData& image, const nb::object& value) { image.bytes = from_python_bytes(value); });

    nb::class_<rawgl::SystemUniformState>(module, "SystemUniformState")
        .def(nb::init<>())
        .def_rw("time_seconds", &rawgl::SystemUniformState::timeSeconds)
        .def_rw("delta_time_seconds", &rawgl::SystemUniformState::deltaTimeSeconds)
        .def_rw("frame_number", &rawgl::SystemUniformState::frameNumber)
        .def_rw("pass_index", &rawgl::SystemUniformState::passIndex);

    nb::class_<rawgl::ShaderModuleDefinition>(module, "ShaderModuleDefinition")
        .def(nb::init<>())
        .def_rw("role", &rawgl::ShaderModuleDefinition::role)
        .def_rw("source_kind", &rawgl::ShaderModuleDefinition::sourceKind)
        .def_rw("path", &rawgl::ShaderModuleDefinition::path)
        .def_rw("glsl_text", &rawgl::ShaderModuleDefinition::glslText)
        .def_rw("debug_label", &rawgl::ShaderModuleDefinition::debugLabel)
        .def_prop_rw(
            "spirv_bytes",
            [](const rawgl::ShaderModuleDefinition& moduleDefinition) {
                return to_python_bytes(moduleDefinition.spirvBytes);
            },
            [](rawgl::ShaderModuleDefinition& moduleDefinition, const nb::object& value) {
                moduleDefinition.spirvBytes = from_python_bytes(value);
            });

    nb::class_<rawgl::ShaderInspectionRequest>(module, "ShaderInspectionRequest")
        .def(nb::init<>())
        .def_rw("kind", &rawgl::ShaderInspectionRequest::kind)
        .def_rw("paths", &rawgl::ShaderInspectionRequest::paths)
        .def_rw("modules", &rawgl::ShaderInspectionRequest::modules);

    nb::class_<rawgl::ShaderResourceInfo>(module, "ShaderResourceInfo")
        .def(nb::init<>())
        .def_rw("name", &rawgl::ShaderResourceInfo::name)
        .def_rw("type_name", &rawgl::ShaderResourceInfo::typeName)
        .def_rw("resource_class", &rawgl::ShaderResourceInfo::resourceClass)
        .def_rw("texture_shape", &rawgl::ShaderResourceInfo::textureShape)
        .def_rw("is_array", &rawgl::ShaderResourceInfo::isArray)
        .def_rw("array_length", &rawgl::ShaderResourceInfo::arrayLength)
        .def_rw("vector_width", &rawgl::ShaderResourceInfo::vectorWidth)
        .def_rw("matrix_columns", &rawgl::ShaderResourceInfo::matrixColumns)
        .def_rw("matrix_rows", &rawgl::ShaderResourceInfo::matrixRows)
        .def_rw("location", &rawgl::ShaderResourceInfo::location)
        .def_rw("binding", &rawgl::ShaderResourceInfo::binding)
        .def_rw("offset", &rawgl::ShaderResourceInfo::offset)
        .def_rw("size", &rawgl::ShaderResourceInfo::size)
        .def_rw("gl_type", &rawgl::ShaderResourceInfo::glType);

    nb::class_<rawgl::ShaderBufferVariableInfo>(module, "ShaderBufferVariableInfo")
        .def(nb::init<>())
        .def_rw("block_name", &rawgl::ShaderBufferVariableInfo::blockName)
        .def_rw("name", &rawgl::ShaderBufferVariableInfo::name)
        .def_rw("type_name", &rawgl::ShaderBufferVariableInfo::typeName)
        .def_rw("location", &rawgl::ShaderBufferVariableInfo::location)
        .def_rw("binding", &rawgl::ShaderBufferVariableInfo::binding)
        .def_rw("offset", &rawgl::ShaderBufferVariableInfo::offset)
        .def_rw("size", &rawgl::ShaderBufferVariableInfo::size)
        .def_rw("gl_type", &rawgl::ShaderBufferVariableInfo::glType);

    nb::class_<rawgl::ShaderInterface>(module, "ShaderInterface")
        .def(nb::init<>())
        .def_rw("success", &rawgl::ShaderInterface::success)
        .def_rw("error_message", &rawgl::ShaderInterface::errorMessage)
        .def_rw("is_compute", &rawgl::ShaderInterface::isCompute)
        .def_rw("uniforms", &rawgl::ShaderInterface::uniforms)
        .def_rw("samplers", &rawgl::ShaderInterface::samplers)
        .def_rw("images", &rawgl::ShaderInterface::images)
        .def_rw("outputs", &rawgl::ShaderInterface::outputs)
        .def_rw("atomic_counters", &rawgl::ShaderInterface::atomicCounters)
        .def_rw("system_uniforms", &rawgl::ShaderInterface::systemUniforms)
        .def_rw("buffer_variables", &rawgl::ShaderInterface::bufferVariables);

    nb::class_<rawgl::MeshInspectionRequest>(module, "MeshInspectionRequest")
        .def(nb::init<>())
        .def_rw("path", &rawgl::MeshInspectionRequest::path);

    nb::class_<rawgl::MeshMaterialInfo>(module, "MeshMaterialInfo")
        .def(nb::init<>())
        .def_rw("id", &rawgl::MeshMaterialInfo::id)
        .def_rw("name", &rawgl::MeshMaterialInfo::name)
        .def_rw("face_count", &rawgl::MeshMaterialInfo::faceCount);

    nb::class_<rawgl::MeshGroupInfo>(module, "MeshGroupInfo")
        .def(nb::init<>())
        .def_rw("name", &rawgl::MeshGroupInfo::name)
        .def_rw("first_face_index", &rawgl::MeshGroupInfo::firstFaceIndex)
        .def_rw("face_count", &rawgl::MeshGroupInfo::faceCount);

    nb::class_<rawgl::MeshInspectionResult>(module, "MeshInspectionResult")
        .def(nb::init<>())
        .def_rw("success", &rawgl::MeshInspectionResult::success)
        .def_rw("error_message", &rawgl::MeshInspectionResult::errorMessage)
        .def_rw("path", &rawgl::MeshInspectionResult::path)
        .def_rw("vertex_count", &rawgl::MeshInspectionResult::vertexCount)
        .def_rw("texcoord_count", &rawgl::MeshInspectionResult::texcoordCount)
        .def_rw("normal_count", &rawgl::MeshInspectionResult::normalCount)
        .def_rw("face_count", &rawgl::MeshInspectionResult::faceCount)
        .def_rw("triangle_face_count", &rawgl::MeshInspectionResult::triangleFaceCount)
        .def_rw("quad_face_count", &rawgl::MeshInspectionResult::quadFaceCount)
        .def_rw("ngon_face_count", &rawgl::MeshInspectionResult::ngonFaceCount)
        .def_rw("generated_triangle_count", &rawgl::MeshInspectionResult::generatedTriangleCount)
        .def_rw("has_bounds", &rawgl::MeshInspectionResult::hasBounds)
        .def_rw("has_uv_range", &rawgl::MeshInspectionResult::hasUvRange)
        .def_rw("bounds_min", &rawgl::MeshInspectionResult::boundsMin)
        .def_rw("bounds_max", &rawgl::MeshInspectionResult::boundsMax)
        .def_rw("uv_min", &rawgl::MeshInspectionResult::uvMin)
        .def_rw("uv_max", &rawgl::MeshInspectionResult::uvMax)
        .def_rw("materials", &rawgl::MeshInspectionResult::materials)
        .def_rw("groups", &rawgl::MeshInspectionResult::groups);

    module.def("inspect_mesh_file",
               [](const std::string& path) {
                   rawgl::MeshInspectionRequest request;
                   request.path = path;
                   return rawgl::InspectMeshFile(request);
               },
               nb::arg("path"),
               "Inspect a mesh file without compiling or executing a workflow.");

    nb::class_<rawgl::InputBinding>(module, "InputBinding")
        .def(nb::init<>())
        .def_rw("name", &rawgl::InputBinding::name)
        .def_rw("source_kind", &rawgl::InputBinding::sourceKind)
        .def_rw("int_values", &rawgl::InputBinding::intValues)
        .def_rw("uint_values", &rawgl::InputBinding::uintValues)
        .def_rw("float_values", &rawgl::InputBinding::floatValues)
        .def_rw("double_values", &rawgl::InputBinding::doubleValues)
        .def_rw("referenced_output_name", &rawgl::InputBinding::referencedOutputName)
        .def_rw("referenced_pass_index", &rawgl::InputBinding::referencedPassIndex)
        .def_rw("workflow_texture_name", &rawgl::InputBinding::workflowTextureName)
        .def_rw("uses_array_element", &rawgl::InputBinding::usesArrayElement)
        .def_rw("array_element", &rawgl::InputBinding::arrayElement)
        .def_rw("uses_referenced_output_array_element", &rawgl::InputBinding::usesReferencedOutputArrayElement)
        .def_rw("referenced_output_array_element", &rawgl::InputBinding::referencedOutputArrayElement)
        .def_rw("attributes", &rawgl::InputBinding::attributes)
        .def_rw("host_texture", &rawgl::InputBinding::hostTexture);

    nb::class_<rawgl::CounterBinding>(module, "CounterBinding")
        .def(nb::init<>())
        .def_rw("name", &rawgl::CounterBinding::name)
        .def_rw("initial_value", &rawgl::CounterBinding::initialValue)
        .def_rw("persistent_counter_name", &rawgl::CounterBinding::persistentCounterName)
        .def_rw("uses_array_element", &rawgl::CounterBinding::usesArrayElement)
        .def_rw("array_element", &rawgl::CounterBinding::arrayElement);

    nb::class_<rawgl::OutputBinding>(module, "OutputBinding")
        .def(nb::init<>())
        .def_rw("name", &rawgl::OutputBinding::name)
        .def_rw("format", &rawgl::OutputBinding::format)
        .def_rw("channels", &rawgl::OutputBinding::channels)
        .def_rw("alpha_channel", &rawgl::OutputBinding::alphaChannel)
        .def_rw("bits", &rawgl::OutputBinding::bits)
        .def_rw("persistent_texture_name", &rawgl::OutputBinding::persistentTextureName)
        .def_rw("uses_array_element", &rawgl::OutputBinding::usesArrayElement)
        .def_rw("array_element", &rawgl::OutputBinding::arrayElement)
        .def_rw("attributes", &rawgl::OutputBinding::attributes)
        .def_rw("capture_to_host", &rawgl::OutputBinding::captureToHost);

    nb::class_<rawgl::MeshBinding>(module, "MeshBinding")
        .def(nb::init<>())
        .def_rw("source_kind", &rawgl::MeshBinding::sourceKind)
        .def_rw("path", &rawgl::MeshBinding::path)
        .def_rw("parameters", &rawgl::MeshBinding::parameters);

    nb::class_<rawgl::Pass>(module, "Pass")
        .def(nb::init<>())
        .def_rw("program_kind", &rawgl::Pass::programKind)
        .def_rw("shader_modules", &rawgl::Pass::shaderModules)
        .def_rw("size_x", &rawgl::Pass::sizeX)
        .def_rw("size_y", &rawgl::Pass::sizeY)
        .def_rw("work_group_size_x", &rawgl::Pass::workGroupSizeX)
        .def_rw("work_group_size_y", &rawgl::Pass::workGroupSizeY)
        .def_rw("has_explicit_work_group_size", &rawgl::Pass::hasExplicitWorkGroupSize)
        .def_rw("clear_color", &rawgl::Pass::clearColor)
        .def_rw("inputs", &rawgl::Pass::inputs)
        .def_rw("counters", &rawgl::Pass::counters)
        .def_rw("outputs", &rawgl::Pass::outputs)
        .def_rw("meshes", &rawgl::Pass::meshes)
        .def_rw("cull_parameters", &rawgl::Pass::cullParameters);

    nb::class_<rawgl::Workflow>(module, "Workflow")
        .def(nb::init<>())
        .def_rw("verbosity", &rawgl::Workflow::verbosity)
        .def_rw("passes", &rawgl::Workflow::passes);

    nb::class_<rawgl::InputOverride>(module, "InputOverride")
        .def(nb::init<>())
        .def_rw("pass_index", &rawgl::InputOverride::passIndex)
        .def_rw("name", &rawgl::InputOverride::name)
        .def_rw("source_kind", &rawgl::InputOverride::sourceKind)
        .def_rw("int_values", &rawgl::InputOverride::intValues)
        .def_rw("uint_values", &rawgl::InputOverride::uintValues)
        .def_rw("float_values", &rawgl::InputOverride::floatValues)
        .def_rw("double_values", &rawgl::InputOverride::doubleValues)
        .def_rw("attributes", &rawgl::InputOverride::attributes)
        .def_rw("uses_array_element", &rawgl::InputOverride::usesArrayElement)
        .def_rw("array_element", &rawgl::InputOverride::arrayElement)
        .def_rw("host_texture", &rawgl::InputOverride::hostTexture);

    nb::class_<rawgl::RunSettings>(module, "RunSettings")
        .def(nb::init<>())
        .def_rw("system_uniforms", &rawgl::RunSettings::systemUniforms)
        .def_rw("overrides", &rawgl::RunSettings::overrides);

    nb::class_<rawgl::RunResult>(module, "RunResult")
        .def(nb::init<>())
        .def_rw("success", &rawgl::RunResult::success)
        .def_rw("error_message", &rawgl::RunResult::errorMessage)
        .def_rw("captured_outputs", &rawgl::RunResult::capturedOutputs)
        .def_rw("captured_counters", &rawgl::RunResult::capturedCounters);

    nb::class_<rawgl::io::RunRequest>(module, "IoRunRequest")
        .def(nb::init<>())
        .def_rw("settings", &rawgl::io::RunRequest::settings)
        .def_rw("file_inputs", &rawgl::io::RunRequest::fileInputs)
        .def_rw("file_outputs", &rawgl::io::RunRequest::fileOutputs);

    nb::class_<rawgl::PreparedWorkflow>(module, "PreparedWorkflow")
        .def("run", &rawgl::PreparedWorkflow::run, nb::arg("settings") = rawgl::RunSettings {});

    nb::class_<rawgl::io::PreparedIoWorkflow>(module, "PreparedIoWorkflow")
        .def("run", &rawgl::io::PreparedIoWorkflow::run, nb::arg("request") = rawgl::io::RunRequest {});

    nb::class_<PythonPrepareResult>(module, "PrepareResult")
        .def(nb::init<>())
        .def_rw("success", &PythonPrepareResult::success)
        .def_rw("error_message", &PythonPrepareResult::errorMessage)
        .def("has_workflow", [](const PythonPrepareResult& result) { return static_cast<bool>(result.workflow); })
        .def("take_workflow",
             [](PythonPrepareResult& result) { return std::move(result.workflow); },
             "Consume and return the prepared workflow, or None when preparation failed.");

    nb::class_<PythonIoPrepareResult>(module, "IoPrepareResult")
        .def(nb::init<>())
        .def_rw("success", &PythonIoPrepareResult::success)
        .def_rw("error_message", &PythonIoPrepareResult::errorMessage)
        .def("has_workflow", [](const PythonIoPrepareResult& result) { return static_cast<bool>(result.workflow); })
        .def("take_workflow",
             [](PythonIoPrepareResult& result) { return std::move(result.workflow); },
             "Consume and return the prepared IO workflow, or None when preparation failed.");

    nb::class_<PythonBatchPrepareResult>(module, "BatchPrepareResult")
        .def(nb::init<>())
        .def_rw("success", &PythonBatchPrepareResult::success)
        .def_rw("error_message", &PythonBatchPrepareResult::errorMessage)
        .def("has_workflow", [](const PythonBatchPrepareResult& result) { return static_cast<bool>(result.workflow); })
        .def("take_workflow",
             [](PythonBatchPrepareResult& result) { return std::move(result.workflow); },
             "Consume and return the prepared batch workflow, or None when preparation failed.");

    nb::class_<rawgl::SessionStats>(module, "SessionStats")
        .def(nb::init<>())
        .def_rw("shader_interfaces", &rawgl::SessionStats::shaderInterfaces)
        .def_rw("textures", &rawgl::SessionStats::textures)
        .def_rw("meshes_host", &rawgl::SessionStats::meshesHost)
        .def_rw("meshes_gpu", &rawgl::SessionStats::meshesGpu);

    nb::class_<rawgl::Session>(module, "Session")
        .def(nb::init<>())
        .def("inspect_shader_interface", &rawgl::Session::inspectShaderInterface, nb::arg("request"))
        .def("prepare",
             [](const rawgl::Session& session, const rawgl::Workflow& workflow) {
                 rawgl::PrepareResult prepareResult = session.prepare(workflow);
                 PythonPrepareResult result;
                 result.success = prepareResult.success;
                 result.errorMessage = std::move(prepareResult.errorMessage);
                 if (prepareResult.workflow) {
                     result.workflow.reset(prepareResult.workflow.release());
                 }
                 return result;
             },
             nb::arg("workflow"))
        .def("run",
             &rawgl::Session::run,
             nb::arg("workflow"),
             nb::arg("settings") = rawgl::RunSettings {},
             "Prepare and run one workflow in a single call.")
        .def("stats", &rawgl::Session::stats);

    nb::enum_<rawgl::io::ImageLoadBackendPolicy>(module, "ImageLoadBackendPolicy")
        .value("auto", rawgl::io::ImageLoadBackendPolicy::Auto)
        .value("native_only", rawgl::io::ImageLoadBackendPolicy::NativeOnly)
        .value("openimageio_only", rawgl::io::ImageLoadBackendPolicy::OpenImageIoOnly);

    nb::enum_<rawgl::io::JpegLoadColorTransform>(module, "JpegLoadColorTransform")
        .value("auto", rawgl::io::JpegLoadColorTransform::Auto)
        .value("grayscale", rawgl::io::JpegLoadColorTransform::Grayscale)
        .value("rgb", rawgl::io::JpegLoadColorTransform::Rgb);

    nb::class_<rawgl::io::JpegLoadOptions>(module, "JpegLoadOptions")
        .def(nb::init<>())
        .def_rw("has_color_transform", &rawgl::io::JpegLoadOptions::hasColorTransform)
        .def_rw("color_transform", &rawgl::io::JpegLoadOptions::colorTransform);

    nb::class_<rawgl::io::PngLoadOptions>(module, "PngLoadOptions")
        .def(nb::init<>())
        .def_rw("has_expand_transparency", &rawgl::io::PngLoadOptions::hasExpandTransparency)
        .def_rw("expand_transparency", &rawgl::io::PngLoadOptions::expandTransparency);

    nb::class_<rawgl::io::TiffLoadOptions>(module, "TiffLoadOptions")
        .def(nb::init<>())
        .def_rw("has_directory_index", &rawgl::io::TiffLoadOptions::hasDirectoryIndex)
        .def_rw("directory_index", &rawgl::io::TiffLoadOptions::directoryIndex);

    nb::enum_<rawgl::io::OpenExrChannelSelection>(module, "OpenExrChannelSelection")
        .value("auto", rawgl::io::OpenExrChannelSelection::Auto)
        .value("luminance", rawgl::io::OpenExrChannelSelection::Luminance)
        .value("rgb", rawgl::io::OpenExrChannelSelection::Rgb)
        .value("rgba", rawgl::io::OpenExrChannelSelection::Rgba)
        .value("all", rawgl::io::OpenExrChannelSelection::All);

    nb::class_<rawgl::io::OpenExrLoadOptions>(module, "OpenExrLoadOptions")
        .def(nb::init<>())
        .def_rw("has_channel_selection", &rawgl::io::OpenExrLoadOptions::hasChannelSelection)
        .def_rw("channel_selection", &rawgl::io::OpenExrLoadOptions::channelSelection);

    nb::class_<rawgl::io::Jpeg2000LoadOptions>(module, "Jpeg2000LoadOptions")
        .def(nb::init<>())
        .def_rw("has_reduce_factor", &rawgl::io::Jpeg2000LoadOptions::hasReduceFactor)
        .def_rw("reduce_factor", &rawgl::io::Jpeg2000LoadOptions::reduceFactor)
        .def_rw("has_layer_limit", &rawgl::io::Jpeg2000LoadOptions::hasLayerLimit)
        .def_rw("layer_limit", &rawgl::io::Jpeg2000LoadOptions::layerLimit);

    nb::class_<rawgl::io::ImageCodecLoadOptions>(module, "ImageCodecLoadOptions")
        .def(nb::init<>())
        .def_rw("has_backend_policy", &rawgl::io::ImageCodecLoadOptions::hasBackendPolicy)
        .def_rw("backend_policy", &rawgl::io::ImageCodecLoadOptions::backendPolicy)
        .def_rw("has_jpeg", &rawgl::io::ImageCodecLoadOptions::hasJpeg)
        .def_rw("jpeg", &rawgl::io::ImageCodecLoadOptions::jpeg)
        .def_rw("has_png", &rawgl::io::ImageCodecLoadOptions::hasPng)
        .def_rw("png", &rawgl::io::ImageCodecLoadOptions::png)
        .def_rw("has_tiff", &rawgl::io::ImageCodecLoadOptions::hasTiff)
        .def_rw("tiff", &rawgl::io::ImageCodecLoadOptions::tiff)
        .def_rw("has_openexr", &rawgl::io::ImageCodecLoadOptions::hasOpenExr)
        .def_rw("openexr", &rawgl::io::ImageCodecLoadOptions::openExr)
        .def_rw("has_jpeg2000", &rawgl::io::ImageCodecLoadOptions::hasJpeg2000)
        .def_rw("jpeg2000", &rawgl::io::ImageCodecLoadOptions::jpeg2000);

    nb::enum_<rawgl::io::JpegChromaSubsampling>(module, "JpegChromaSubsampling")
        .value("default", rawgl::io::JpegChromaSubsampling::Default)
        .value("s444", rawgl::io::JpegChromaSubsampling::S444)
        .value("s422", rawgl::io::JpegChromaSubsampling::S422)
        .value("s420", rawgl::io::JpegChromaSubsampling::S420)
        .value("s440", rawgl::io::JpegChromaSubsampling::S440)
        .value("s411", rawgl::io::JpegChromaSubsampling::S411);

    nb::class_<rawgl::io::JpegSaveOptions>(module, "JpegSaveOptions")
        .def(nb::init<>())
        .def_rw("has_quality", &rawgl::io::JpegSaveOptions::hasQuality)
        .def_rw("quality", &rawgl::io::JpegSaveOptions::quality)
        .def_rw("has_progressive", &rawgl::io::JpegSaveOptions::hasProgressive)
        .def_rw("progressive", &rawgl::io::JpegSaveOptions::progressive)
        .def_rw("has_optimize", &rawgl::io::JpegSaveOptions::hasOptimize)
        .def_rw("optimize", &rawgl::io::JpegSaveOptions::optimize)
        .def_rw("has_subsampling", &rawgl::io::JpegSaveOptions::hasSubsampling)
        .def_rw("subsampling", &rawgl::io::JpegSaveOptions::subsampling);

    nb::class_<rawgl::io::PngSaveOptions>(module, "PngSaveOptions")
        .def(nb::init<>())
        .def_rw("has_compression_level", &rawgl::io::PngSaveOptions::hasCompressionLevel)
        .def_rw("compression_level", &rawgl::io::PngSaveOptions::compressionLevel)
        .def_rw("has_interlaced", &rawgl::io::PngSaveOptions::hasInterlaced)
        .def_rw("interlaced", &rawgl::io::PngSaveOptions::interlaced);

    nb::enum_<rawgl::io::TiffCompressionMode>(module, "TiffCompressionMode")
        .value("none", rawgl::io::TiffCompressionMode::None)
        .value("lzw", rawgl::io::TiffCompressionMode::Lzw)
        .value("packbits", rawgl::io::TiffCompressionMode::PackBits)
        .value("deflate", rawgl::io::TiffCompressionMode::Deflate)
        .value("adobe_deflate", rawgl::io::TiffCompressionMode::AdobeDeflate)
        .value("jpeg", rawgl::io::TiffCompressionMode::Jpeg)
        .value("lzma", rawgl::io::TiffCompressionMode::Lzma)
        .value("zstd", rawgl::io::TiffCompressionMode::Zstd)
        .value("webp", rawgl::io::TiffCompressionMode::Webp)
        .value("jxl", rawgl::io::TiffCompressionMode::Jxl)
        .value("jxl_dng", rawgl::io::TiffCompressionMode::JxlDng)
        .value("lerc", rawgl::io::TiffCompressionMode::Lerc);

    nb::enum_<rawgl::io::TiffPredictorMode>(module, "TiffPredictorMode")
        .value("none", rawgl::io::TiffPredictorMode::None)
        .value("horizontal", rawgl::io::TiffPredictorMode::Horizontal)
        .value("float", rawgl::io::TiffPredictorMode::Float);

    nb::enum_<rawgl::io::TiffStorageLayout>(module, "TiffStorageLayout")
        .value("strips", rawgl::io::TiffStorageLayout::Strips)
        .value("tiled", rawgl::io::TiffStorageLayout::Tiled);

    nb::class_<rawgl::io::TiffSaveOptions>(module, "TiffSaveOptions")
        .def(nb::init<>())
        .def_rw("has_compression", &rawgl::io::TiffSaveOptions::hasCompression)
        .def_rw("compression", &rawgl::io::TiffSaveOptions::compression)
        .def_rw("has_predictor", &rawgl::io::TiffSaveOptions::hasPredictor)
        .def_rw("predictor", &rawgl::io::TiffSaveOptions::predictor)
        .def_rw("has_layout", &rawgl::io::TiffSaveOptions::hasLayout)
        .def_rw("layout", &rawgl::io::TiffSaveOptions::layout)
        .def_rw("has_force_big_tiff", &rawgl::io::TiffSaveOptions::hasForceBigTiff)
        .def_rw("force_big_tiff", &rawgl::io::TiffSaveOptions::forceBigTiff)
        .def_rw("has_unassociated_alpha", &rawgl::io::TiffSaveOptions::hasUnassociatedAlpha)
        .def_rw("unassociated_alpha", &rawgl::io::TiffSaveOptions::unassociatedAlpha)
        .def_rw("has_rows_per_strip", &rawgl::io::TiffSaveOptions::hasRowsPerStrip)
        .def_rw("rows_per_strip", &rawgl::io::TiffSaveOptions::rowsPerStrip)
        .def_rw("has_tile_width", &rawgl::io::TiffSaveOptions::hasTileWidth)
        .def_rw("tile_width", &rawgl::io::TiffSaveOptions::tileWidth)
        .def_rw("has_tile_height", &rawgl::io::TiffSaveOptions::hasTileHeight)
        .def_rw("tile_height", &rawgl::io::TiffSaveOptions::tileHeight)
        .def_rw("has_jpeg_quality", &rawgl::io::TiffSaveOptions::hasJpegQuality)
        .def_rw("jpeg_quality", &rawgl::io::TiffSaveOptions::jpegQuality)
        .def_rw("has_deflate_level", &rawgl::io::TiffSaveOptions::hasDeflateLevel)
        .def_rw("deflate_level", &rawgl::io::TiffSaveOptions::deflateLevel)
        .def_rw("has_zstd_level", &rawgl::io::TiffSaveOptions::hasZstdLevel)
        .def_rw("zstd_level", &rawgl::io::TiffSaveOptions::zstdLevel)
        .def_rw("has_lzma_preset", &rawgl::io::TiffSaveOptions::hasLzmaPreset)
        .def_rw("lzma_preset", &rawgl::io::TiffSaveOptions::lzmaPreset)
        .def_rw("has_webp_level", &rawgl::io::TiffSaveOptions::hasWebpLevel)
        .def_rw("webp_level", &rawgl::io::TiffSaveOptions::webpLevel)
        .def_rw("has_webp_lossless", &rawgl::io::TiffSaveOptions::hasWebpLossless)
        .def_rw("webp_lossless", &rawgl::io::TiffSaveOptions::webpLossless)
        .def_rw("has_webp_lossless_exact", &rawgl::io::TiffSaveOptions::hasWebpLosslessExact)
        .def_rw("webp_lossless_exact", &rawgl::io::TiffSaveOptions::webpLosslessExact);

    nb::enum_<rawgl::io::OpenExrCompressionMode>(module, "OpenExrCompressionMode")
        .value("none", rawgl::io::OpenExrCompressionMode::None)
        .value("rle", rawgl::io::OpenExrCompressionMode::Rle)
        .value("zips", rawgl::io::OpenExrCompressionMode::Zips)
        .value("zip", rawgl::io::OpenExrCompressionMode::Zip)
        .value("piz", rawgl::io::OpenExrCompressionMode::Piz)
        .value("pxr24", rawgl::io::OpenExrCompressionMode::Pxr24)
        .value("b44", rawgl::io::OpenExrCompressionMode::B44)
        .value("b44a", rawgl::io::OpenExrCompressionMode::B44A)
        .value("dwaa", rawgl::io::OpenExrCompressionMode::Dwaa)
        .value("dwab", rawgl::io::OpenExrCompressionMode::Dwab)
        .value("htj2k256", rawgl::io::OpenExrCompressionMode::Htj2k256)
        .value("htj2k32", rawgl::io::OpenExrCompressionMode::Htj2k32);

    nb::enum_<rawgl::io::OpenExrStorageLayout>(module, "OpenExrStorageLayout")
        .value("scanlines", rawgl::io::OpenExrStorageLayout::Scanlines)
        .value("tiled", rawgl::io::OpenExrStorageLayout::Tiled);

    nb::enum_<rawgl::io::OpenExrLineOrder>(module, "OpenExrLineOrder")
        .value("increasing_y", rawgl::io::OpenExrLineOrder::IncreasingY)
        .value("decreasing_y", rawgl::io::OpenExrLineOrder::DecreasingY)
        .value("random_y", rawgl::io::OpenExrLineOrder::RandomY);

    nb::class_<rawgl::io::OpenExrSaveOptions>(module, "OpenExrSaveOptions")
        .def(nb::init<>())
        .def_rw("has_compression", &rawgl::io::OpenExrSaveOptions::hasCompression)
        .def_rw("compression", &rawgl::io::OpenExrSaveOptions::compression)
        .def_rw("has_layout", &rawgl::io::OpenExrSaveOptions::hasLayout)
        .def_rw("layout", &rawgl::io::OpenExrSaveOptions::layout)
        .def_rw("has_tile_width", &rawgl::io::OpenExrSaveOptions::hasTileWidth)
        .def_rw("tile_width", &rawgl::io::OpenExrSaveOptions::tileWidth)
        .def_rw("has_tile_height", &rawgl::io::OpenExrSaveOptions::hasTileHeight)
        .def_rw("tile_height", &rawgl::io::OpenExrSaveOptions::tileHeight)
        .def_rw("has_line_order", &rawgl::io::OpenExrSaveOptions::hasLineOrder)
        .def_rw("line_order", &rawgl::io::OpenExrSaveOptions::lineOrder)
        .def_rw("has_dwa_compression_level", &rawgl::io::OpenExrSaveOptions::hasDwaCompressionLevel)
        .def_rw("dwa_compression_level", &rawgl::io::OpenExrSaveOptions::dwaCompressionLevel);

    nb::class_<rawgl::io::Jpeg2000SaveOptions>(module, "Jpeg2000SaveOptions")
        .def(nb::init<>())
        .def_rw("has_lossless", &rawgl::io::Jpeg2000SaveOptions::hasLossless)
        .def_rw("lossless", &rawgl::io::Jpeg2000SaveOptions::lossless)
        .def_rw("has_compression_ratio", &rawgl::io::Jpeg2000SaveOptions::hasCompressionRatio)
        .def_rw("compression_ratio", &rawgl::io::Jpeg2000SaveOptions::compressionRatio)
        .def_rw("has_quality", &rawgl::io::Jpeg2000SaveOptions::hasQuality)
        .def_rw("quality", &rawgl::io::Jpeg2000SaveOptions::quality);

    nb::class_<rawgl::io::ImageCodecSaveOptions>(module, "ImageCodecSaveOptions")
        .def(nb::init<>())
        .def_rw("has_jpeg", &rawgl::io::ImageCodecSaveOptions::hasJpeg)
        .def_rw("jpeg", &rawgl::io::ImageCodecSaveOptions::jpeg)
        .def_rw("has_png", &rawgl::io::ImageCodecSaveOptions::hasPng)
        .def_rw("png", &rawgl::io::ImageCodecSaveOptions::png)
        .def_rw("has_tiff", &rawgl::io::ImageCodecSaveOptions::hasTiff)
        .def_rw("tiff", &rawgl::io::ImageCodecSaveOptions::tiff)
        .def_rw("has_openexr", &rawgl::io::ImageCodecSaveOptions::hasOpenExr)
        .def_rw("openexr", &rawgl::io::ImageCodecSaveOptions::openExr)
        .def_rw("has_jpeg2000", &rawgl::io::ImageCodecSaveOptions::hasJpeg2000)
        .def_rw("jpeg2000", &rawgl::io::ImageCodecSaveOptions::jpeg2000);

    nb::class_<rawgl::io::FileInputBinding>(module, "FileInputBinding")
        .def(nb::init<>())
        .def_rw("pass_index", &rawgl::io::FileInputBinding::passIndex)
        .def_rw("name", &rawgl::io::FileInputBinding::name)
        .def_rw("path", &rawgl::io::FileInputBinding::path)
        .def_rw("attributes", &rawgl::io::FileInputBinding::attributes)
        .def_rw("codec_options", &rawgl::io::FileInputBinding::codecOptions)
        .def_rw("uses_array_element", &rawgl::io::FileInputBinding::usesArrayElement)
        .def_rw("array_element", &rawgl::io::FileInputBinding::arrayElement);

    nb::class_<rawgl::io::FileInputOverride>(module, "FileInputOverride")
        .def(nb::init<>())
        .def_rw("pass_index", &rawgl::io::FileInputOverride::passIndex)
        .def_rw("name", &rawgl::io::FileInputOverride::name)
        .def_rw("path", &rawgl::io::FileInputOverride::path)
        .def_rw("attributes", &rawgl::io::FileInputOverride::attributes)
        .def_rw("codec_options", &rawgl::io::FileInputOverride::codecOptions)
        .def_rw("uses_array_element", &rawgl::io::FileInputOverride::usesArrayElement)
        .def_rw("array_element", &rawgl::io::FileInputOverride::arrayElement);

    nb::class_<rawgl::io::FileOutputBinding>(module, "FileOutputBinding")
        .def(nb::init<>())
        .def_rw("pass_index", &rawgl::io::FileOutputBinding::passIndex)
        .def_rw("name", &rawgl::io::FileOutputBinding::name)
        .def_rw("path", &rawgl::io::FileOutputBinding::path)
        .def_rw("format", &rawgl::io::FileOutputBinding::format)
        .def_rw("channels", &rawgl::io::FileOutputBinding::channels)
        .def_rw("alpha_channel", &rawgl::io::FileOutputBinding::alphaChannel)
        .def_rw("bits", &rawgl::io::FileOutputBinding::bits)
        .def_rw("attributes", &rawgl::io::FileOutputBinding::attributes)
        .def_rw("codec_options", &rawgl::io::FileOutputBinding::codecOptions)
        .def_rw("uses_array_element", &rawgl::io::FileOutputBinding::usesArrayElement)
        .def_rw("array_element", &rawgl::io::FileOutputBinding::arrayElement);

    nb::class_<rawgl::io::IoRuntimeOptions>(module, "IoRuntimeOptions")
        .def(nb::init<>())
        .def_rw("decode_worker_count", &rawgl::io::IoRuntimeOptions::decodeWorkerCount)
        .def_rw("encode_worker_count", &rawgl::io::IoRuntimeOptions::encodeWorkerCount);

    nb::class_<rawgl::io::ImageLoadRequest>(module, "ImageLoadRequest")
        .def(nb::init<>())
        .def_rw("path", &rawgl::io::ImageLoadRequest::path)
        .def_rw("attributes", &rawgl::io::ImageLoadRequest::attributes)
        .def_rw("codec_options", &rawgl::io::ImageLoadRequest::codecOptions);

    nb::class_<rawgl::io::ImageLoadResult>(module, "ImageLoadResult")
        .def(nb::init<>())
        .def_rw("success", &rawgl::io::ImageLoadResult::success)
        .def_rw("error_message", &rawgl::io::ImageLoadResult::errorMessage)
        .def_rw("image", &rawgl::io::ImageLoadResult::image);

    nb::class_<rawgl::io::ImageSaveRequest>(module, "ImageSaveRequest")
        .def(nb::init<>())
        .def_rw("path", &rawgl::io::ImageSaveRequest::path)
        .def_rw("attributes", &rawgl::io::ImageSaveRequest::attributes)
        .def_rw("codec_options", &rawgl::io::ImageSaveRequest::codecOptions)
        .def_rw("alpha_channel", &rawgl::io::ImageSaveRequest::alphaChannel)
        .def_rw("bits", &rawgl::io::ImageSaveRequest::bits)
        .def_rw("image", &rawgl::io::ImageSaveRequest::image);

    nb::class_<rawgl::io::ImageSaveResult>(module, "ImageSaveResult")
        .def(nb::init<>())
        .def_rw("success", &rawgl::io::ImageSaveResult::success)
        .def_rw("error_message", &rawgl::io::ImageSaveResult::errorMessage);

    nb::class_<rawgl::io::ImageIoCapabilityDetail>(module, "ImageIoCapabilityDetail")
        .def(nb::init<>())
        .def_rw("name", &rawgl::io::ImageIoCapabilityDetail::name)
        .def_rw("value", &rawgl::io::ImageIoCapabilityDetail::value);

    nb::class_<rawgl::io::ImageCodecCapabilities>(module, "ImageCodecCapabilities")
        .def(nb::init<>())
        .def_rw("name", &rawgl::io::ImageCodecCapabilities::name)
        .def_rw("extensions", &rawgl::io::ImageCodecCapabilities::extensions)
        .def_rw("native_read", &rawgl::io::ImageCodecCapabilities::nativeRead)
        .def_rw("native_write", &rawgl::io::ImageCodecCapabilities::nativeWrite)
        .def_rw("fallback_read", &rawgl::io::ImageCodecCapabilities::fallbackRead)
        .def_rw("fallback_write", &rawgl::io::ImageCodecCapabilities::fallbackWrite)
        .def_rw("native_read_component_types", &rawgl::io::ImageCodecCapabilities::nativeReadComponentTypes)
        .def_rw("native_write_component_types", &rawgl::io::ImageCodecCapabilities::nativeWriteComponentTypes)
        .def_rw("native_read_options", &rawgl::io::ImageCodecCapabilities::nativeReadOptions)
        .def_rw("native_write_compression_modes", &rawgl::io::ImageCodecCapabilities::nativeWriteCompressionModes)
        .def_rw("unavailable_native_write_compression_modes",
                &rawgl::io::ImageCodecCapabilities::unavailableNativeWriteCompressionModes)
        .def_rw("native_write_options", &rawgl::io::ImageCodecCapabilities::nativeWriteOptions)
        .def_rw("details", &rawgl::io::ImageCodecCapabilities::details);

    nb::class_<rawgl::io::ImageIoCapabilities>(module, "ImageIoCapabilities")
        .def(nb::init<>())
        .def_rw("open_image_io_fallback", &rawgl::io::ImageIoCapabilities::openImageIoFallback)
        .def_rw("codecs", &rawgl::io::ImageIoCapabilities::codecs);

    nb::enum_<rawgl::io::MetadataNameStyle>(module, "MetadataNameStyle")
        .value("canonical", rawgl::io::MetadataNameStyle::Canonical)
        .value("xmp_portable", rawgl::io::MetadataNameStyle::XmpPortable)
        .value("oiio", rawgl::io::MetadataNameStyle::Oiio);

    nb::enum_<rawgl::io::MetadataNamePolicy>(module, "MetadataNamePolicy")
        .value("spec", rawgl::io::MetadataNamePolicy::Spec)
        .value("exif_tool_alias", rawgl::io::MetadataNamePolicy::ExifToolAlias);

    nb::enum_<rawgl::io::MetadataKeyKind>(module, "MetadataKeyKind")
        .value("exif_tag", rawgl::io::MetadataKeyKind::ExifTag)
        .value("comment", rawgl::io::MetadataKeyKind::Comment)
        .value("exr_attribute", rawgl::io::MetadataKeyKind::ExrAttribute)
        .value("iptc_dataset", rawgl::io::MetadataKeyKind::IptcDataset)
        .value("xmp_property", rawgl::io::MetadataKeyKind::XmpProperty)
        .value("icc_header_field", rawgl::io::MetadataKeyKind::IccHeaderField)
        .value("icc_tag", rawgl::io::MetadataKeyKind::IccTag)
        .value("photoshop_irb", rawgl::io::MetadataKeyKind::PhotoshopIrb)
        .value("photoshop_irb_field", rawgl::io::MetadataKeyKind::PhotoshopIrbField)
        .value("geotiff_key", rawgl::io::MetadataKeyKind::GeotiffKey)
        .value("printim_field", rawgl::io::MetadataKeyKind::PrintImField)
        .value("bmff_field", rawgl::io::MetadataKeyKind::BmffField)
        .value("jumbf_field", rawgl::io::MetadataKeyKind::JumbfField)
        .value("jumbf_cbor_key", rawgl::io::MetadataKeyKind::JumbfCborKey)
        .value("png_text", rawgl::io::MetadataKeyKind::PngText);

    nb::enum_<rawgl::io::MetadataValueKind>(module, "MetadataValueKind")
        .value("empty", rawgl::io::MetadataValueKind::Empty)
        .value("scalar", rawgl::io::MetadataValueKind::Scalar)
        .value("array", rawgl::io::MetadataValueKind::Array)
        .value("bytes", rawgl::io::MetadataValueKind::Bytes)
        .value("text", rawgl::io::MetadataValueKind::Text);

    nb::enum_<rawgl::io::MetadataElementType>(module, "MetadataElementType")
        .value("u8", rawgl::io::MetadataElementType::U8)
        .value("i8", rawgl::io::MetadataElementType::I8)
        .value("u16", rawgl::io::MetadataElementType::U16)
        .value("i16", rawgl::io::MetadataElementType::I16)
        .value("u32", rawgl::io::MetadataElementType::U32)
        .value("i32", rawgl::io::MetadataElementType::I32)
        .value("u64", rawgl::io::MetadataElementType::U64)
        .value("i64", rawgl::io::MetadataElementType::I64)
        .value("f32", rawgl::io::MetadataElementType::F32)
        .value("f64", rawgl::io::MetadataElementType::F64)
        .value("u_rational", rawgl::io::MetadataElementType::URational)
        .value("s_rational", rawgl::io::MetadataElementType::SRational);

    nb::enum_<rawgl::io::MetadataTextEncoding>(module, "MetadataTextEncoding")
        .value("unknown", rawgl::io::MetadataTextEncoding::Unknown)
        .value("ascii", rawgl::io::MetadataTextEncoding::Ascii)
        .value("utf8", rawgl::io::MetadataTextEncoding::Utf8)
        .value("utf16le", rawgl::io::MetadataTextEncoding::Utf16LE)
        .value("utf16be", rawgl::io::MetadataTextEncoding::Utf16BE);

    nb::enum_<rawgl::io::MetadataTransferSafety>(module, "MetadataTransferSafety")
        .value("compatible_file", rawgl::io::MetadataTransferSafety::CompatibleFile)
        .value("rendered_image", rawgl::io::MetadataTransferSafety::RenderedImage);

    nb::enum_<rawgl::io::MetadataEntryFlags>(module, "MetadataEntryFlags")
        .value("none", rawgl::io::MetadataEntryFlags::None)
        .value("deleted", rawgl::io::MetadataEntryFlags::Deleted)
        .value("dirty", rawgl::io::MetadataEntryFlags::Dirty)
        .value("derived", rawgl::io::MetadataEntryFlags::Derived)
        .value("truncated", rawgl::io::MetadataEntryFlags::Truncated)
        .value("unreadable", rawgl::io::MetadataEntryFlags::Unreadable)
        .value("contextual_name", rawgl::io::MetadataEntryFlags::ContextualName);

    nb::class_<rawgl::io::MetadataEntry>(module, "MetadataEntry")
        .def(nb::init<>())
        .def_rw("key_kind", &rawgl::io::MetadataEntry::keyKind)
        .def_rw("value_kind", &rawgl::io::MetadataEntry::valueKind)
        .def_rw("element_type", &rawgl::io::MetadataEntry::elementType)
        .def_rw("text_encoding", &rawgl::io::MetadataEntry::textEncoding)
        .def_rw("flags", &rawgl::io::MetadataEntry::flags)
        .def_rw("count", &rawgl::io::MetadataEntry::count)
        .def_rw("name", &rawgl::io::MetadataEntry::name)
        .def_rw("value_text", &rawgl::io::MetadataEntry::valueText);

    nb::class_<rawgl::io::MetadataValue>(module, "MetadataValue")
        .def(nb::init<>())
        .def_rw("kind", &rawgl::io::MetadataValue::kind)
        .def_rw("element_type", &rawgl::io::MetadataValue::elementType)
        .def_rw("text_encoding", &rawgl::io::MetadataValue::textEncoding)
        .def_rw("count", &rawgl::io::MetadataValue::count)
        .def_prop_rw(
            "bytes",
            [](const rawgl::io::MetadataValue& value) { return to_python_bytes(value.bytes); },
            [](rawgl::io::MetadataValue& value, const nb::object& object) { value.bytes = from_python_bytes(object); });

    nb::class_<rawgl::io::MetadataField>(module, "MetadataField")
        .def(nb::init<>())
        .def_rw("key_kind", &rawgl::io::MetadataField::keyKind)
        .def_rw("flags", &rawgl::io::MetadataField::flags)
        .def_rw("name", &rawgl::io::MetadataField::name)
        .def_rw("value", &rawgl::io::MetadataField::value);

    nb::class_<rawgl::io::MetadataDocument>(module, "MetadataDocument")
        .def(nb::init<>())
        .def_rw("fields", &rawgl::io::MetadataDocument::fields);

    nb::class_<rawgl::io::MetadataReadRequest>(module, "MetadataReadRequest")
        .def(nb::init<>())
        .def_rw("path", &rawgl::io::MetadataReadRequest::path)
        .def_rw("name_style", &rawgl::io::MetadataReadRequest::nameStyle)
        .def_rw("name_policy", &rawgl::io::MetadataReadRequest::namePolicy)
        .def_rw("include_makernotes", &rawgl::io::MetadataReadRequest::includeMakernotes)
        .def_rw("max_value_preview_bytes", &rawgl::io::MetadataReadRequest::maxValuePreviewBytes)
        .def_rw("max_value_preview_elements", &rawgl::io::MetadataReadRequest::maxValuePreviewElements);

    nb::class_<rawgl::io::MetadataReadResult>(module, "MetadataReadResult")
        .def(nb::init<>())
        .def_rw("success", &rawgl::io::MetadataReadResult::success)
        .def_rw("error_message", &rawgl::io::MetadataReadResult::errorMessage)
        .def_rw("entries", &rawgl::io::MetadataReadResult::entries);

    nb::class_<rawgl::io::MetadataDocumentReadRequest>(module, "MetadataDocumentReadRequest")
        .def(nb::init<>())
        .def_rw("path", &rawgl::io::MetadataDocumentReadRequest::path)
        .def_rw("name_style", &rawgl::io::MetadataDocumentReadRequest::nameStyle)
        .def_rw("name_policy", &rawgl::io::MetadataDocumentReadRequest::namePolicy)
        .def_rw("include_makernotes", &rawgl::io::MetadataDocumentReadRequest::includeMakernotes);

    nb::class_<rawgl::io::MetadataDocumentReadResult>(module, "MetadataDocumentReadResult")
        .def(nb::init<>())
        .def_rw("success", &rawgl::io::MetadataDocumentReadResult::success)
        .def_rw("error_message", &rawgl::io::MetadataDocumentReadResult::errorMessage)
        .def_rw("document", &rawgl::io::MetadataDocumentReadResult::document);

    nb::class_<rawgl::io::ImageMetadataTransferRequest>(module, "ImageMetadataTransferRequest")
        .def(nb::init<>())
        .def_rw("path", &rawgl::io::ImageMetadataTransferRequest::path)
        .def_rw("source_metadata", &rawgl::io::ImageMetadataTransferRequest::sourceMetadata)
        .def_rw("safety", &rawgl::io::ImageMetadataTransferRequest::safety)
        .def_rw("has_target_image", &rawgl::io::ImageMetadataTransferRequest::hasTargetImage)
        .def_rw("target_image", &rawgl::io::ImageMetadataTransferRequest::targetImage);

    nb::class_<rawgl::io::ImageMetadataTransferResult>(module, "ImageMetadataTransferResult")
        .def(nb::init<>())
        .def_rw("success", &rawgl::io::ImageMetadataTransferResult::success)
        .def_rw("error_message", &rawgl::io::ImageMetadataTransferResult::errorMessage);

    nb::class_<rawgl::io::IoRuntime>(module, "IoRuntime")
        .def(nb::init<const rawgl::io::IoRuntimeOptions&>(), nb::arg("options") = rawgl::io::IoRuntimeOptions {})
        .def("load_image_file", &rawgl::io::IoRuntime::loadImageFile, nb::arg("request"))
        .def("save_image_file", &rawgl::io::IoRuntime::saveImageFile, nb::arg("request"))
        .def("read_metadata_file", &rawgl::io::IoRuntime::readMetadataFile, nb::arg("request"))
        .def("read_metadata_document_file", &rawgl::io::IoRuntime::readMetadataDocumentFile, nb::arg("request"))
        .def("transfer_image_metadata_file",
             &rawgl::io::IoRuntime::transferImageMetadataFile,
             nb::arg("request"))
        .def("prepare",
             [](const rawgl::io::IoRuntime& ioRuntime,
                const rawgl::Session& session,
                const rawgl::Workflow& workflow,
                const std::vector<rawgl::io::FileInputBinding>& fileInputs,
                const std::vector<rawgl::io::FileOutputBinding>& fileOutputs) {
                 rawgl::io::PrepareWorkflowResult prepareResult = ioRuntime.prepare(session,
                                                                                   workflow,
                                                                                   fileInputs,
                                                                                   fileOutputs);
                 PythonIoPrepareResult result;
                 result.success = prepareResult.success;
                 result.errorMessage = std::move(prepareResult.errorMessage);
                 if (prepareResult.workflow) {
                     result.workflow.reset(prepareResult.workflow.release());
                 }
                 return result;
             },
             nb::arg("session"),
             nb::arg("workflow"),
             nb::arg("file_inputs") = std::vector<rawgl::io::FileInputBinding> {},
             nb::arg("file_outputs") = std::vector<rawgl::io::FileOutputBinding> {})
        .def("run",
             &rawgl::io::IoRuntime::run,
             nb::arg("session"),
             nb::arg("workflow"),
             nb::arg("request") = rawgl::io::RunRequest {},
             nb::arg("file_inputs") = std::vector<rawgl::io::FileInputBinding> {},
             nb::arg("file_outputs") = std::vector<rawgl::io::FileOutputBinding> {});

    module.def("load_image_file", &rawgl::io::LoadImageFile, nb::arg("request"));
    module.def("save_image_file", &rawgl::io::SaveImageFile, nb::arg("request"));
    module.def("get_image_io_capabilities", &rawgl::io::GetImageIoCapabilities);
    module.def("read_metadata_file", &rawgl::io::ReadMetadataFile, nb::arg("request"));
    module.def("read_metadata_document_file", &rawgl::io::ReadMetadataDocumentFile, nb::arg("request"));
    module.def("transfer_image_metadata_file",
               &rawgl::io::TransferImageMetadataFile,
               nb::arg("request"));

    nb::class_<rawgl::batch::BatchRunnerOptions>(module, "BatchRunnerOptions")
        .def(nb::init<>())
        .def_rw("max_in_flight_jobs", &rawgl::batch::BatchRunnerOptions::maxInFlightJobs)
        .def_rw("prepare_queue_capacity", &rawgl::batch::BatchRunnerOptions::prepareQueueCapacity)
        .def_rw("execute_queue_capacity", &rawgl::batch::BatchRunnerOptions::executeQueueCapacity)
        .def_rw("save_queue_capacity", &rawgl::batch::BatchRunnerOptions::saveQueueCapacity)
        .def_rw("prepare_worker_count", &rawgl::batch::BatchRunnerOptions::prepareWorkerCount)
        .def_rw("save_worker_count", &rawgl::batch::BatchRunnerOptions::saveWorkerCount)
        .def_rw("gpu_worker_count", &rawgl::batch::BatchRunnerOptions::gpuWorkerCount)
        .def_rw("host_memory_budget_bytes", &rawgl::batch::BatchRunnerOptions::hostMemoryBudgetBytes)
        .def_rw("gpu_memory_budget_bytes", &rawgl::batch::BatchRunnerOptions::gpuMemoryBudgetBytes)
        .def_rw("preserve_submit_order", &rawgl::batch::BatchRunnerOptions::preserveSubmitOrder);

    nb::class_<rawgl::batch::BatchProgress>(module, "BatchProgress")
        .def(nb::init<>())
        .def_rw("submitted_jobs", &rawgl::batch::BatchProgress::submittedJobs)
        .def_rw("completed_jobs", &rawgl::batch::BatchProgress::completedJobs)
        .def_rw("failed_jobs", &rawgl::batch::BatchProgress::failedJobs)
        .def_rw("cancelled_jobs", &rawgl::batch::BatchProgress::cancelledJobs)
        .def_rw("in_flight_jobs", &rawgl::batch::BatchProgress::inFlightJobs);

    nb::class_<rawgl::batch::BatchSubmitRequest>(module, "BatchSubmitRequest")
        .def(nb::init<>())
        .def_rw("settings", &rawgl::batch::BatchSubmitRequest::settings)
        .def_rw("file_inputs", &rawgl::batch::BatchSubmitRequest::fileInputs)
        .def_rw("file_outputs", &rawgl::batch::BatchSubmitRequest::fileOutputs);

    nb::class_<rawgl::batch::BatchResult>(module, "BatchResult")
        .def(nb::init<>())
        .def_rw("submit_index", &rawgl::batch::BatchResult::submitIndex)
        .def_rw("cancelled", &rawgl::batch::BatchResult::cancelled)
        .def_rw("run_result", &rawgl::batch::BatchResult::runResult);

    nb::class_<rawgl::batch::BatchCancellationToken>(module, "BatchCancellationToken")
        .def(nb::init<>())
        .def("cancel", &rawgl::batch::BatchCancellationToken::cancel)
        .def("is_cancellation_requested", &rawgl::batch::BatchCancellationToken::isCancellationRequested);

    nb::class_<rawgl::batch::BatchPreparedWorkflow>(module, "BatchPreparedWorkflow");

    nb::class_<rawgl::batch::BatchJobHandle>(module, "BatchJobHandle")
        .def("wait", &rawgl::batch::BatchJobHandle::wait);

    nb::class_<rawgl::batch::BatchRunner>(module, "BatchRunner")
        .def(nb::init<rawgl::Session&, const rawgl::batch::BatchRunnerOptions&>(),
             nb::arg("session"),
             nb::arg("options") = rawgl::batch::BatchRunnerOptions {},
             nb::keep_alive<1, 2>())
        .def(nb::init<rawgl::Session&, const rawgl::io::IoRuntime&, const rawgl::batch::BatchRunnerOptions&>(),
             nb::arg("session"),
             nb::arg("io_runtime"),
             nb::arg("options") = rawgl::batch::BatchRunnerOptions {},
             nb::keep_alive<1, 2>(),
             nb::keep_alive<1, 3>())
        .def("prepare",
             [](const rawgl::batch::BatchRunner& runner,
                const rawgl::Workflow& workflow,
                const std::vector<rawgl::io::FileInputBinding>& fileInputs,
                const std::vector<rawgl::io::FileOutputBinding>& fileOutputs) {
                 rawgl::batch::BatchPrepareResult prepareResult = runner.prepare(workflow, fileInputs, fileOutputs);
                 PythonBatchPrepareResult result;
                 result.success = prepareResult.success;
                 result.errorMessage = std::move(prepareResult.errorMessage);
                 if (prepareResult.workflow) {
                     result.workflow.reset(prepareResult.workflow.release());
                 }
                 return result;
             },
             nb::arg("workflow"),
             nb::arg("file_inputs") = std::vector<rawgl::io::FileInputBinding> {},
             nb::arg("file_outputs") = std::vector<rawgl::io::FileOutputBinding> {})
        .def("submit",
             &rawgl::batch::BatchRunner::submit,
             nb::arg("workflow"),
             nb::arg("request") = rawgl::batch::BatchSubmitRequest {},
             nb::arg("cancellation") = static_cast<const rawgl::batch::BatchCancellationToken*>(nullptr))
        .def("progress", &rawgl::batch::BatchRunner::progress)
        .def("close", &rawgl::batch::BatchRunner::close);
}
