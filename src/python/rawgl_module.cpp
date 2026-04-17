// SPDX-License-Identifier: LGPL-2.1-or-later
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

#if RAWGL_PYTHON_BIND_CORE
#    include <rawgl/rawgl.h>
#endif

namespace nb = nanobind;

namespace {

#if RAWGL_PYTHON_BIND_CORE
struct PythonPrepareResult {
    bool success = false;
    std::string errorMessage;
    std::shared_ptr<rawgl::PreparedWorkflow> workflow;
};
#endif

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

#if RAWGL_PYTHON_BIND_CORE
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
#endif

const char*
rawgl_python_status()
{
#if RAWGL_PYTHON_BIND_CORE
    return "rawgl nanobind facade";
#else
    return "rawgl nanobind scaffold (core bindings disabled)";
#endif
}

}  // namespace

NB_MODULE(_rawgl, module)
{
    module.doc() = "RawGL Python workflow facade";
    module.attr("__version__") = nb::str(RAWGL_VERSION_STRING);
    module.def("status", &rawgl_python_status, "Return the current RawGL Python binding status string.");

#if RAWGL_PYTHON_BIND_CORE
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
        .value("texture_file", rawgl::InputSourceKind::textureFile)
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

    nb::class_<rawgl::InputBinding>(module, "InputBinding")
        .def(nb::init<>())
        .def_rw("name", &rawgl::InputBinding::name)
        .def_rw("source_kind", &rawgl::InputBinding::sourceKind)
        .def_rw("int_values", &rawgl::InputBinding::intValues)
        .def_rw("uint_values", &rawgl::InputBinding::uintValues)
        .def_rw("float_values", &rawgl::InputBinding::floatValues)
        .def_rw("double_values", &rawgl::InputBinding::doubleValues)
        .def_rw("texture_path", &rawgl::InputBinding::texturePath)
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
        .def_rw("path", &rawgl::OutputBinding::path)
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
        .def_rw("texture_path", &rawgl::InputOverride::texturePath)
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

    nb::class_<rawgl::PreparedWorkflow>(module, "PreparedWorkflow")
        .def("run", &rawgl::PreparedWorkflow::run, nb::arg("settings") = rawgl::RunSettings {});

    nb::class_<PythonPrepareResult>(module, "PrepareResult")
        .def(nb::init<>())
        .def_rw("success", &PythonPrepareResult::success)
        .def_rw("error_message", &PythonPrepareResult::errorMessage)
        .def("has_workflow", [](const PythonPrepareResult& result) { return static_cast<bool>(result.workflow); })
        .def("take_workflow",
             [](PythonPrepareResult& result) { return std::move(result.workflow); },
             "Consume and return the prepared workflow, or None when preparation failed.");

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
#endif
}
