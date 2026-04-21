"""
RawGL Python bindings.

The compiled extension module `_rawgl` exposes the full advanced façade.
This package also adds a small high-level convenience layer for common
single-pass render and compute workflows.
"""

from __future__ import annotations

from collections.abc import Mapping, Sequence
from pathlib import Path
import re

from . import _rawgl as _impl  # noqa: F401
from ._rawgl import *  # noqa: F401,F403

try:
    import numpy as _np
except ImportError:  # pragma: no cover - optional dependency
    _np = None

__version__ = getattr(_impl, "__version__", "0.0.0")
advanced = _impl

_BUILTIN_FULLSCREEN_VERTEX_SHADER = """#version 450 core
layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 uv_co;
layout(location = 0) out vec2 UV;
void main()
{
    UV = uv_co;
    gl_Position = vec4(pos.x, pos.y, 0.0, 1.0);
}
"""

_OUTPUT_SPEC_KEYS = frozenset({
    "path",
    "format",
    "channels",
    "alpha_channel",
    "bits",
    "capture_to_host",
    "capture",
    "persistent_texture_name",
    "attributes",
    "attrs",
    "array_element",
})

_SYSTEM_UNIFORM_ALIASES = {
    "time": "time_seconds",
    "time_seconds": "time_seconds",
    "delta_time": "delta_time_seconds",
    "delta_time_seconds": "delta_time_seconds",
    "frame": "frame_number",
    "frame_number": "frame_number",
    "pass": "pass_index",
    "pass_index": "pass_index",
}

_GL_UNSIGNED_BYTE = 0x1401
_GL_UNSIGNED_SHORT = 0x1403
_GL_FLOAT = 0x1406
_GL_HALF_FLOAT = 0x140B

_GL_INTERNAL_FORMATS = {
    (_GL_UNSIGNED_BYTE, 1): 0x8229,   # GL_R8
    (_GL_UNSIGNED_BYTE, 2): 0x822B,   # GL_RG8
    (_GL_UNSIGNED_BYTE, 3): 0x8051,   # GL_RGB8
    (_GL_UNSIGNED_BYTE, 4): 0x8058,   # GL_RGBA8
    (_GL_UNSIGNED_SHORT, 1): 0x822A,  # GL_R16
    (_GL_UNSIGNED_SHORT, 2): 0x822C,  # GL_RG16
    (_GL_UNSIGNED_SHORT, 3): 0x8054,  # GL_RGB16
    (_GL_UNSIGNED_SHORT, 4): 0x805B,  # GL_RGBA16
    (_GL_FLOAT, 1): 0x822E,           # GL_R32F
    (_GL_FLOAT, 2): 0x8230,           # GL_RG32F
    (_GL_FLOAT, 3): 0x8815,           # GL_RGB32F
    (_GL_FLOAT, 4): 0x8814,           # GL_RGBA32F
    (_GL_HALF_FLOAT, 1): 0x822D,      # GL_R16F
    (_GL_HALF_FLOAT, 2): 0x822F,      # GL_RG16F
    (_GL_HALF_FLOAT, 3): 0x881B,      # GL_RGB16F
    (_GL_HALF_FLOAT, 4): 0x881A,      # GL_RGBA16F
}


def _looks_like_shader_text(value: str) -> bool:
    stripped = value.lstrip()
    return "\n" in value or stripped.startswith("#version") or "void main" in value


def _coerce_shader_text_or_path(value) -> str:
    if isinstance(value, (list, tuple)):
        return "\n".join(str(item) for item in value)
    return str(value)


def _coerce_shader_module(shader, role):
    if isinstance(shader, ShaderModuleDefinition):
        return shader

    module = ShaderModuleDefinition()
    module.role = role

    if isinstance(shader, (bytes, bytearray, memoryview)):
        module.source_kind = ShaderModuleSourceKind.spirv_binary
        module.spirv_bytes = bytes(shader)
        return module

    text_or_path = _coerce_shader_text_or_path(shader)
    if _looks_like_shader_text(text_or_path):
        module.source_kind = ShaderModuleSourceKind.glsl_text
        module.glsl_text = text_or_path
    else:
        module.source_kind = ShaderModuleSourceKind.file_path
        module.path = text_or_path
    return module


def _coerce_size(size) -> tuple[int, int]:
    if isinstance(size, int):
        return size, size
    if isinstance(size, Sequence) and len(size) == 2:
        return int(size[0]), int(size[1])
    raise TypeError("size must be an int or a (width, height) pair")


def _image_size_from_host_value(value):
    if _is_numpy_array(value):
        array = _require_numpy().asarray(value)
        if array.ndim == 2:
            return int(array.shape[1]), int(array.shape[0])
        if array.ndim == 3:
            return int(array.shape[1]), int(array.shape[0])
        return None

    if isinstance(value, HostImageData):
        return int(value.width), int(value.height)

    return None


def _image_size_from_input_spec(spec):
    size = _image_size_from_host_value(spec)
    if size is not None:
        return size

    if isinstance(spec, Mapping):
        if "host_texture" in spec:
            return _image_size_from_host_value(spec["host_texture"])
        if "value" in spec:
            return _image_size_from_host_value(spec["value"])
        if "values" in spec:
            return _image_size_from_host_value(spec["values"])

    return None


def _infer_size_from_inputs(inputs):
    if not inputs:
        return None
    for spec in inputs.values():
        size = _image_size_from_input_spec(spec)
        if size is not None:
            return size
    return None


def _coerce_pass_size(size, inputs=None, *, fallback=None) -> tuple[int, int]:
    if size is None:
        inferred = _infer_size_from_inputs(inputs)
        if inferred is not None:
            return inferred
        if fallback is not None:
            return _coerce_size(fallback)
        raise TypeError("size must be provided or inferable from a NumPy/HostImageData input")
    return _coerce_size(size)


def _coerce_attributes(attributes):
    if not attributes:
        return []
    if isinstance(attributes, Mapping):
        result = []
        for name, value in attributes.items():
            attribute = Attribute()
            attribute.name = str(name)
            attribute.value = str(value)
            result.append(attribute)
        return result
    return list(attributes)


def _require_numpy():
    if _np is None:
        raise RuntimeError("NumPy support is optional and numpy is not installed")
    return _np


def _is_numpy_array(value) -> bool:
    return _np is not None and isinstance(value, _np.ndarray)


def _numpy_gl_type(dtype) -> int:
    np_module = _require_numpy()
    dtype = np_module.dtype(dtype)
    if dtype == np_module.uint8:
        return _GL_UNSIGNED_BYTE
    if dtype == np_module.uint16:
        return _GL_UNSIGNED_SHORT
    if dtype == np_module.float16:
        return _GL_HALF_FLOAT
    if dtype == np_module.float32:
        return _GL_FLOAT
    raise TypeError(f"unsupported numpy dtype for host image conversion: {dtype}")


def make_host_image(array):
    """Create HostImageData from a HxW or HxWxC NumPy array."""

    np_module = _require_numpy()
    array = np_module.asarray(array)
    if array.ndim == 2:
        height, width = array.shape
        channels = 1
    elif array.ndim == 3:
        height, width, channels = array.shape
    else:
        raise TypeError("host image numpy arrays must be HxW or HxWxC")

    if channels < 1 or channels > 4:
        raise TypeError("host image numpy arrays must use 1 to 4 channels")

    gl_type = _numpy_gl_type(array.dtype)
    gl_internal_format = _GL_INTERNAL_FORMATS.get((gl_type, channels))
    if gl_internal_format is None:
        raise TypeError("unsupported numpy dtype/channel combination for host image conversion")

    contiguous = np_module.ascontiguousarray(array)
    image = HostImageData()
    image.width = int(width)
    image.height = int(height)
    image.channels = int(channels)
    image.alpha_channel = 3 if channels == 4 else -1
    image.gl_internal_format = int(gl_internal_format)
    image.gl_type = int(gl_type)
    image.bytes = contiguous.tobytes()
    return image


def host_image_to_array(image):
    """Convert HostImageData bytes into a NumPy array."""

    np_module = _require_numpy()
    dtype_map = {
        _GL_UNSIGNED_BYTE: np_module.uint8,
        _GL_UNSIGNED_SHORT: np_module.uint16,
        _GL_FLOAT: np_module.float32,
        _GL_HALF_FLOAT: np_module.float16,
    }
    dtype = dtype_map.get(int(image.gl_type))
    if dtype is None:
        raise TypeError(f"unsupported HostImageData gl_type for NumPy conversion: {image.gl_type}")

    values = np_module.frombuffer(image.bytes, dtype=dtype)
    expected_size = int(image.width) * int(image.height) * int(image.channels)
    if values.size != expected_size:
        raise RuntimeError(
            f"HostImageData byte payload size mismatch: expected {expected_size} values, found {values.size}"
        )
    if int(image.channels) == 1:
        return values.reshape((int(image.height), int(image.width))).copy()
    return values.reshape((int(image.height), int(image.width), int(image.channels))).copy()


def captured_output_arrays(result):
    """Convert all captured host outputs in a RunResult into NumPy arrays."""

    return {
        name: host_image_to_array(image) for name, image in result.captured_outputs.items()
    }


def captured_counter_values(result):
    """Convert captured counter payloads into scalars when possible."""

    converted = {}
    for name, values in result.captured_counters.items():
        if len(values) == 1:
            converted[name] = int(values[0])
        else:
            converted[name] = [int(value) for value in values]
    return converted


def save_image(image, path, *, bits=16, alpha_channel=None, attributes=None, io_runtime=None):
    """Save one HostImageData or NumPy image to disk through rawgl.io."""

    io_runtime_type = globals().get("IoRuntime")
    image_save_request_type = globals().get("ImageSaveRequest")
    if io_runtime_type is None or image_save_request_type is None:
        raise RuntimeError("rawgl.save_image() requires the core Python bindings")

    if _is_numpy_array(image):
        host_image = make_host_image(image)
    elif isinstance(image, HostImageData):
        host_image = image
    else:
        raise TypeError("save_image() expects a HostImageData or NumPy array")

    if io_runtime is None:
        io_runtime = io_runtime_type()

    request = image_save_request_type()
    request.path = str(path)
    request.bits = int(bits)
    request.alpha_channel = -1 if alpha_channel is None else int(alpha_channel)
    request.attributes = _coerce_attributes(attributes)
    request.image = host_image

    result = io_runtime.save_image_file(request)
    if not result.success:
        raise RuntimeError(result.error_message or f"failed to save image '{path}'")
    return result


def _apply_input_scalar_or_sequence(binding, name: str, value) -> InputBinding:
    if isinstance(value, bool):
        binding.source_kind = InputSourceKind.int_values
        binding.int_values = [int(value)]
        return binding
    if isinstance(value, int):
        binding.source_kind = InputSourceKind.int_values
        binding.int_values = [int(value)]
        return binding
    if isinstance(value, float):
        binding.source_kind = InputSourceKind.float_values
        binding.float_values = [float(value)]
        return binding
    if _is_numpy_array(value):
        binding.source_kind = InputSourceKind.host_texture
        binding.host_texture = make_host_image(value)
        return binding
    if isinstance(value, HostImageData):
        binding.source_kind = InputSourceKind.host_texture
        binding.host_texture = value
        return binding
    if isinstance(value, (str, Path)):
        binding.source_kind = InputSourceKind.texture_file
        binding.texture_path = str(value)
        return binding
    if isinstance(value, Sequence):
        if not value:
            raise ValueError(f"input '{name}' cannot use an empty value list")
        if all(isinstance(item, bool) for item in value):
            binding.source_kind = InputSourceKind.int_values
            binding.int_values = [int(item) for item in value]
            return binding
        if all(isinstance(item, int) and not isinstance(item, bool) for item in value):
            binding.source_kind = InputSourceKind.int_values
            binding.int_values = [int(item) for item in value]
            return binding
        if all(isinstance(item, (int, float)) and not isinstance(item, bool) for item in value):
            binding.source_kind = InputSourceKind.float_values
            binding.float_values = [float(item) for item in value]
            return binding
    raise TypeError(f"unsupported input specification for '{name}'")


def _coerce_input_binding(name, spec):
    if isinstance(spec, InputBinding):
        return spec

    binding = InputBinding()
    binding.name = str(name)

    if isinstance(spec, Mapping):
        binding.attributes = _coerce_attributes(spec.get("attributes", spec.get("attrs")))
        if "array_element" in spec:
            binding.uses_array_element = True
            binding.array_element = int(spec["array_element"])

        if "host_texture" in spec:
            binding.source_kind = InputSourceKind.host_texture
            binding.host_texture = spec["host_texture"]
            return binding

        if "workflow_texture" in spec or "persistent_texture_name" in spec:
            binding.source_kind = InputSourceKind.workflow_texture
            binding.workflow_texture_name = str(spec.get("workflow_texture", spec.get("persistent_texture_name")))
            return binding

        if "pass_output" in spec:
            reference = spec["pass_output"]
            if not isinstance(reference, Sequence) or isinstance(reference, (str, bytes, bytearray)) or len(reference) < 2:
                raise TypeError("pass_output must be a (name, pass_index) or (name, pass_index, array_element) sequence")
            binding.source_kind = InputSourceKind.pass_output
            binding.referenced_output_name = str(reference[0])
            binding.referenced_pass_index = int(reference[1])
            if len(reference) > 2:
                binding.uses_referenced_output_array_element = True
                binding.referenced_output_array_element = int(reference[2])
            return binding

        if "texture_path" in spec or "path" in spec:
            binding.source_kind = InputSourceKind.texture_file
            binding.texture_path = str(spec.get("texture_path", spec.get("path")))
            return binding

        if "int_values" in spec:
            binding.source_kind = InputSourceKind.int_values
            binding.int_values = [int(value) for value in spec["int_values"]]
            return binding
        if "uint_values" in spec:
            binding.source_kind = InputSourceKind.uint_values
            binding.uint_values = [int(value) for value in spec["uint_values"]]
            return binding
        if "float_values" in spec:
            binding.source_kind = InputSourceKind.float_values
            binding.float_values = [float(value) for value in spec["float_values"]]
            return binding
        if "double_values" in spec:
            binding.source_kind = InputSourceKind.double_values
            binding.double_values = [float(value) for value in spec["double_values"]]
            return binding

        if "values" in spec:
            return _apply_input_scalar_or_sequence(binding, name, spec["values"])
        if "value" in spec:
            return _apply_input_scalar_or_sequence(binding, name, spec["value"])

    return _apply_input_scalar_or_sequence(binding, name, spec)


def _coerce_output_binding(name, spec):
    if isinstance(spec, OutputBinding):
        return spec

    binding = OutputBinding()
    binding.name = str(name)

    if spec is True:
        binding.capture_to_host = True
        return binding
    if isinstance(spec, (str, Path)):
        binding.path = str(spec)
        binding.format = "rgb16"
        binding.channels = 3
        binding.bits = 16
        return binding
    if not isinstance(spec, Mapping):
        raise TypeError(f"unsupported output specification for '{name}'")

    if "path" in spec:
        binding.path = str(spec["path"])
    binding.format = str(spec.get("format", binding.format))
    binding.channels = int(spec.get("channels", binding.channels))
    binding.alpha_channel = int(spec.get("alpha_channel", binding.alpha_channel))
    binding.bits = int(spec.get("bits", binding.bits))
    binding.capture_to_host = bool(spec.get("capture_to_host", spec.get("capture", False)))
    binding.persistent_texture_name = str(spec.get("persistent_texture_name", binding.persistent_texture_name))
    if "array_element" in spec:
        binding.uses_array_element = True
        binding.array_element = int(spec["array_element"])
    binding.attributes = _coerce_attributes(spec.get("attributes", spec.get("attrs")))
    return binding


def _coerce_mesh_binding(spec):
    if isinstance(spec, MeshBinding):
        return spec

    binding = MeshBinding()
    if isinstance(spec, (str, Path)):
        binding.source_kind = MeshSourceKind.file
        binding.path = str(spec)
        return binding
    if not isinstance(spec, Mapping):
        raise TypeError("mesh bindings must be a MeshBinding, path, or mapping")

    source_kind = spec.get("source_kind", spec.get("source", None))
    if source_kind is None:
        if "path" in spec:
            binding.source_kind = MeshSourceKind.file
        else:
            binding.source_kind = MeshSourceKind.quad
    elif isinstance(source_kind, MeshSourceKind):
        binding.source_kind = source_kind
    else:
        source_name = str(source_kind).lower()
        if source_name == "quad":
            binding.source_kind = MeshSourceKind.quad
        elif source_name == "file":
            binding.source_kind = MeshSourceKind.file
        else:
            raise TypeError(f"unsupported mesh source kind '{source_kind}'")

    if "path" in spec:
        binding.path = str(spec["path"])
    binding.parameters = _coerce_attributes(spec.get("parameters", spec.get("attrs")))
    return binding


def _coerce_input_override(pass_index: int, name, spec):
    binding = _coerce_input_binding(name, spec)
    if binding.source_kind in (InputSourceKind.pass_output, InputSourceKind.workflow_texture):
        raise TypeError(f"run-time input override for '{name}' cannot use pass outputs or workflow textures")

    override = InputOverride()
    override.pass_index = int(pass_index)
    override.name = binding.name
    override.source_kind = binding.source_kind
    override.int_values = list(binding.int_values)
    override.uint_values = list(binding.uint_values)
    override.float_values = list(binding.float_values)
    override.double_values = list(binding.double_values)
    override.texture_path = binding.texture_path
    override.attributes = list(binding.attributes)
    override.uses_array_element = binding.uses_array_element
    override.array_element = binding.array_element
    if binding.host_texture is not None:
        override.host_texture = binding.host_texture
    return override


def _coerce_input_override_entry(key, spec, default_pass_index=0):
    array_element = None
    if isinstance(key, tuple):
        if len(key) == 2:
            pass_index, name = key
        elif len(key) == 3:
            pass_index, name, array_element = key
        else:
            raise TypeError("input override tuple keys must be (pass_index, name) or (pass_index, name, array_element)")
    else:
        if default_pass_index is None:
            raise TypeError("multi-pass input overrides must use tuple keys like (pass_index, name)")
        pass_index = default_pass_index
        name = key

    override = _coerce_input_override(int(pass_index), name, spec)
    if array_element is not None:
        override.uses_array_element = True
        override.array_element = int(array_element)
    return override


def _coerce_input_overrides(inputs, *, default_pass_index=0):
    if inputs is None:
        return []
    return [
        _coerce_input_override_entry(key, spec, default_pass_index=default_pass_index)
        for key, spec in inputs.items()
    ]


def _coerce_counter_binding(name, spec):
    if isinstance(spec, CounterBinding):
        return spec

    binding = CounterBinding()
    binding.name = str(name)

    if isinstance(spec, int):
        binding.initial_value = int(spec)
        return binding
    if not isinstance(spec, Mapping):
        raise TypeError(f"unsupported counter specification for '{name}'")

    binding.initial_value = int(spec.get("initial_value", spec.get("value", 0)))
    binding.persistent_counter_name = str(spec.get("persistent_counter_name", binding.persistent_counter_name))
    if "array_element" in spec:
        binding.uses_array_element = True
        binding.array_element = int(spec["array_element"])
    return binding


def _coerce_counter_bindings(name, spec):
    if isinstance(spec, Mapping) and "array_elements" in spec:
        result = []
        for array_element in spec["array_elements"]:
            expanded = dict(spec)
            expanded.pop("array_elements", None)
            expanded["array_element"] = int(array_element)
            result.append(_coerce_counter_binding(name, expanded))
        return result
    return [_coerce_counter_binding(name, spec)]


def _build_workflow(pass0, verbosity: int) -> Workflow:
    workflow = Workflow()
    workflow.verbosity = int(verbosity)
    workflow.passes = [pass0]
    return workflow


def _default_io_runtime():
    io_runtime_type = globals().get("IoRuntime")
    if io_runtime_type is None:
        return None
    return io_runtime_type()


def _run_workflow(workflow, *, session=None, settings=None, io_runtime=None):
    if session is None:
        session = Session()
    if settings is None:
        settings = RunSettings()
    if io_runtime is None:
        io_runtime = _default_io_runtime()
    if io_runtime is not None:
        return _wrap_run_result(io_runtime.run(session, workflow, settings))
    return _wrap_run_result(session.run(workflow, settings))


def _prepare_workflow(workflow, *, session=None, io_runtime=None, error_label="workflow", default_pass_index=0):
    if session is None:
        session = Session()
    if io_runtime is None:
        io_runtime = _default_io_runtime()

    if io_runtime is not None:
        prepare = io_runtime.prepare(session, workflow)
        if not prepare.success:
            raise RuntimeError(prepare.error_message or f"{error_label} preparation failed")
        prepared_workflow = prepare.take_workflow()
        if prepared_workflow is None:
            raise RuntimeError(f"{error_label} preparation returned no PreparedIoWorkflow")
        return PreparedJob(
            prepared_workflow,
            session=session,
            io_runtime=io_runtime,
            default_pass_index=default_pass_index,
        )

    prepare = session.prepare(workflow)
    if not prepare.success:
        raise RuntimeError(prepare.error_message or f"{error_label} preparation failed")
    prepared_workflow = prepare.take_workflow()
    if prepared_workflow is None:
        raise RuntimeError(f"{error_label} preparation returned no PreparedWorkflow")
    return PreparedJob(
        prepared_workflow,
        session=session,
        io_runtime=io_runtime,
        default_pass_index=default_pass_index,
    )


def _shader_modules_for_inspection(program_kind, shader_modules):
    if program_kind == ShaderProgramKind.vertfrag and len(shader_modules) == 1 and shader_modules[0].role == ShaderModuleRole.fragment:
        return [
            _coerce_shader_module(_BUILTIN_FULLSCREEN_VERTEX_SHADER, ShaderModuleRole.vertex),
            shader_modules[0],
        ]
    return shader_modules


def _inspect_single_output_name(session, program_kind, shader_modules, input_names=None):
    request = ShaderInspectionRequest()
    request.kind = program_kind
    request.modules = _shader_modules_for_inspection(program_kind, shader_modules)

    interface = session.inspect_shader_interface(request)
    if not interface.success:
        raise RuntimeError(interface.error_message or "shader inspection failed while inferring output name")
    if len(interface.outputs) == 1:
        return interface.outputs[0].name

    if program_kind == ShaderProgramKind.compute:
        excluded_names = set() if input_names is None else {str(name) for name in input_names}
        image_outputs = [resource.name for resource in interface.images if resource.name not in excluded_names]
        if len(image_outputs) == 1:
            return image_outputs[0]

    raise RuntimeError(
        f"single-output convenience requires exactly one shader output candidate, found outputs={len(interface.outputs)}"
    )


def _coerce_outputs(session, program_kind, shader_modules, outputs, input_names=None):
    if outputs is None:
        return []
    if isinstance(outputs, OutputBinding):
        return [outputs]
    if isinstance(outputs, (str, Path)) or outputs is True:
        name = _inspect_single_output_name(session, program_kind, shader_modules, input_names=input_names)
        return [_coerce_output_binding(name, outputs)]
    if isinstance(outputs, Mapping):
        if outputs and set(outputs.keys()).issubset(_OUTPUT_SPEC_KEYS):
            name = _inspect_single_output_name(session, program_kind, shader_modules, input_names=input_names)
            return [_coerce_output_binding(name, outputs)]
        return [
            _coerce_output_binding(name, spec) for name, spec in outputs.items()
        ]
    raise TypeError("outputs must be an OutputBinding, a path/bool, or a mapping")


def _outputs_require_inspection(outputs) -> bool:
    if outputs is None:
        return False
    if isinstance(outputs, OutputBinding):
        return False
    if isinstance(outputs, (str, Path)) or outputs is True:
        return True
    if isinstance(outputs, Mapping):
        return bool(outputs) and set(outputs.keys()).issubset(_OUTPUT_SPEC_KEYS)
    return False


def _coerce_run_settings(settings):
    if settings is None:
        return RunSettings()
    if isinstance(settings, RunSettings):
        return settings
    raise TypeError("settings must be a rawgl.RunSettings instance")


def _coerce_system_uniform_state(system_uniforms):
    if system_uniforms is None:
        return None
    if isinstance(system_uniforms, SystemUniformState):
        return system_uniforms
    if not isinstance(system_uniforms, Mapping):
        raise TypeError("system_uniforms must be a rawgl.SystemUniformState or a mapping")

    state = SystemUniformState()
    for key, value in system_uniforms.items():
        field_name = _SYSTEM_UNIFORM_ALIASES.get(str(key))
        if field_name is None:
            raise TypeError(f"unsupported system uniform field '{key}'")
        if field_name in ("time_seconds", "delta_time_seconds"):
            setattr(state, field_name, float(value))
        else:
            setattr(state, field_name, int(value))
    return state


def _merge_run_settings(settings=None, system_uniforms=None):
    run_settings = _coerce_run_settings(settings)
    state = _coerce_system_uniform_state(system_uniforms)
    if state is not None:
        run_settings.system_uniforms = state
    return run_settings


def _resolve_output_arguments(outputs=None, output=None):
    if outputs is not None and output is not None:
        raise TypeError("use either 'outputs' or 'output', not both")
    if output is not None:
        return output
    return outputs


class PreparedJob:
    """High-level prepared workflow wrapper for repeated runs."""

    def __init__(self, prepared_workflow, session=None, io_runtime=None, default_pass_index: int | None = 0):
        self._prepared_workflow = prepared_workflow
        self._session = session
        self._io_runtime = io_runtime
        self._default_pass_index = None if default_pass_index is None else int(default_pass_index)

    def run(self, *, inputs=None, settings=None, system_uniforms=None):
        run_settings = _merge_run_settings(settings=settings, system_uniforms=system_uniforms)
        overrides = list(run_settings.overrides)
        if inputs is not None:
            overrides.extend(
                _coerce_input_overrides(inputs, default_pass_index=self._default_pass_index)
            )
        run_settings.overrides = overrides
        return _wrap_run_result(self._prepared_workflow.run(run_settings))

    @property
    def session(self):
        return self._session

    @property
    def io_runtime(self):
        return self._io_runtime

    def close(self):
        self._prepared_workflow = None
        self._io_runtime = None
        self._session = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()
        return False

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


class RunResultView:
    """High-level Python wrapper over rawgl.RunResult."""

    def __init__(self, result):
        self._result = result

    def __getattr__(self, name):
        return getattr(self._result, name)

    def __bool__(self):
        return bool(self._result.success)

    @property
    def raw_result(self):
        return self._result

    @property
    def output_arrays(self):
        return captured_output_arrays(self._result)

    def output_array(self, name=None):
        arrays = self.output_arrays
        if name is None:
            if len(arrays) != 1:
                raise RuntimeError(f"expected exactly one captured output array, found {len(arrays)}")
            return next(iter(arrays.values()))
        return arrays[name]

    @property
    def counters(self):
        return captured_counter_values(self._result)

    def counter(self, name=None):
        counters = self.counters
        if name is None:
            if len(counters) != 1:
                raise RuntimeError(f"expected exactly one captured counter payload, found {len(counters)}")
            return next(iter(counters.values()))
        return counters[name]

    def counter_array(self, base_name: str, pass_index: int = 0):
        pattern = re.compile(rf"^{re.escape(base_name)}\[(\d+)\]::{int(pass_index)}$")
        items = []
        for name, value in self.counters.items():
            match = pattern.match(name)
            if match is None:
                continue
            items.append((int(match.group(1)), value))
        if not items:
            raise KeyError(f"no captured counter array values found for '{base_name}' pass {pass_index}")
        items.sort(key=lambda item: item[0])
        return [value for _, value in items]


def pass_output(name: str, pass_index: int = 0, array_element=None):
    """Build an input specification that references one earlier pass output."""

    reference = [str(name), int(pass_index)]
    if array_element is not None:
        reference.append(int(array_element))
    return {"pass_output": tuple(reference)}


def workflow_texture(name: str):
    """Build an input specification that references one persistent workflow texture."""

    return {"workflow_texture": str(name)}


def render_pass(
    fragment_shader,
    *,
    outputs=None,
    output=None,
    size=None,
    inputs=None,
    vertex_shader=None,
    clear_color=None,
    meshes=None,
    cull_parameters=None,
    session=None,
):
    """Build one render pass object for use in a multi-pass workflow."""

    pass0 = Pass()
    pass0.program_kind = ShaderProgramKind.vertfrag
    pass0.size_x, pass0.size_y = _coerce_pass_size(size, inputs, fallback=512)

    shader_modules = []
    if vertex_shader is not None:
        shader_modules.append(_coerce_shader_module(vertex_shader, ShaderModuleRole.vertex))
    shader_modules.append(_coerce_shader_module(fragment_shader, ShaderModuleRole.fragment))
    pass0.shader_modules = shader_modules

    if clear_color is not None:
        if not isinstance(clear_color, Sequence) or len(clear_color) != 4:
            raise TypeError("clear_color must be a 4-element sequence")
        pass0.clear_color = [float(value) for value in clear_color]

    output_spec = _resolve_output_arguments(outputs=outputs, output=output)
    pass0.inputs = [] if inputs is None else [
        _coerce_input_binding(name, spec) for name, spec in inputs.items()
    ]
    if session is None and _outputs_require_inspection(output_spec):
        session = Session()
    pass0.outputs = _coerce_outputs(session, ShaderProgramKind.vertfrag, shader_modules, output_spec)
    pass0.meshes = [] if meshes is None else [_coerce_mesh_binding(spec) for spec in meshes]
    pass0.cull_parameters = _coerce_attributes(cull_parameters)
    return pass0


def image_pass(
    fragment_shader,
    *,
    outputs=None,
    output=None,
    size=None,
    inputs=None,
    clear_color=None,
    session=None,
):
    """Build one fullscreen image-processing pass for a multi-pass workflow."""

    return render_pass(
        fragment_shader,
        outputs=outputs,
        output=output,
        size=size,
        inputs=inputs,
        clear_color=clear_color,
        session=session,
    )


def compute_pass(
    shader,
    *,
    outputs=None,
    output=None,
    size=None,
    inputs=None,
    counters=None,
    workgroup_size=(16, 16),
    session=None,
):
    """Build one compute pass object for use in a multi-pass workflow."""

    pass0 = Pass()
    pass0.program_kind = ShaderProgramKind.compute
    pass0.size_x, pass0.size_y = _coerce_pass_size(size, inputs)
    pass0.work_group_size_x, pass0.work_group_size_y = _coerce_size(workgroup_size)
    pass0.has_explicit_work_group_size = True
    pass0.shader_modules = [_coerce_shader_module(shader, ShaderModuleRole.compute)]

    pass0.inputs = [] if inputs is None else [
        _coerce_input_binding(name, spec) for name, spec in inputs.items()
    ]
    pass0.counters = [] if counters is None else [
        binding
        for name, spec in counters.items()
        for binding in _coerce_counter_bindings(name, spec)
    ]
    input_names = [] if inputs is None else list(inputs.keys())
    output_spec = _resolve_output_arguments(outputs=outputs, output=output)
    if session is None and _outputs_require_inspection(output_spec):
        session = Session()
    pass0.outputs = _coerce_outputs(
        session,
        ShaderProgramKind.compute,
        pass0.shader_modules,
        output_spec,
        input_names=input_names,
    )
    return pass0


def build_workflow(*passes, verbosity=3):
    """Build one workflow from already-built pass objects."""

    if len(passes) == 1 and isinstance(passes[0], Workflow):
        workflow = passes[0]
        if verbosity is not None:
            workflow.verbosity = int(verbosity)
        return workflow

    if len(passes) == 1 and isinstance(passes[0], Sequence) and not isinstance(passes[0], (str, bytes, bytearray)):
        passes = tuple(passes[0])

    workflow = Workflow()
    workflow.verbosity = int(verbosity)
    workflow.passes = list(passes)
    return workflow


def _coerce_workflow_definition(workflow=None, *, passes=None, verbosity=3):
    if workflow is not None and passes is not None:
        raise TypeError("use either 'workflow' or 'passes', not both")
    if workflow is not None:
        if not isinstance(workflow, Workflow):
            raise TypeError("workflow must be a rawgl.Workflow instance")
        return workflow
    if passes is None:
        raise TypeError("workflow preparation requires a workflow or pass list")
    return build_workflow(passes, verbosity=verbosity)


def _wrap_run_result(result):
    if isinstance(result, RunResultView):
        return result
    return RunResultView(result)


def render_workflow(
    fragment_shader,
    *,
    outputs=None,
    output=None,
    size=512,
    inputs=None,
    vertex_shader=None,
    verbosity=3,
    session=None,
):
    """
    Build one fullscreen render workflow.

    The common case is fragment-only: RawGL uses its built-in fullscreen quad
    vertex shader automatically when `vertex_shader` is omitted.
    """
    pass0 = render_pass(
        fragment_shader,
        outputs=outputs,
        output=output,
        size=size,
        inputs=inputs,
        vertex_shader=vertex_shader,
        session=session,
    )
    return _build_workflow(pass0, verbosity)


def compute_workflow(
    shader,
    *,
    outputs=None,
    output=None,
    size,
    inputs=None,
    counters=None,
    workgroup_size=(16, 16),
    verbosity=3,
    session=None,
):
    """Build one compute workflow."""
    pass0 = compute_pass(
        shader,
        outputs=outputs,
        output=output,
        size=size,
        inputs=inputs,
        counters=counters,
        workgroup_size=workgroup_size,
        session=session,
    )
    return _build_workflow(pass0, verbosity)


def prepare_workflow(
    workflow=None,
    *,
    passes=None,
    verbosity=3,
    session=None,
    io_runtime=None,
):
    """Prepare one multi-pass workflow for repeated execution."""

    workflow_definition = _coerce_workflow_definition(workflow, passes=passes, verbosity=verbosity)
    default_pass_index = 0 if len(workflow_definition.passes) == 1 else None
    return _prepare_workflow(
        workflow_definition,
        session=session,
        io_runtime=io_runtime,
        error_label="workflow",
        default_pass_index=default_pass_index,
    )


def run_workflow(
    workflow=None,
    *,
    passes=None,
    verbosity=3,
    session=None,
    io_runtime=None,
    settings=None,
    system_uniforms=None,
    inputs=None,
):
    """Run one workflow once, with optional multi-pass input overrides."""

    workflow_definition = _coerce_workflow_definition(workflow, passes=passes, verbosity=verbosity)
    run_settings = _merge_run_settings(settings=settings, system_uniforms=system_uniforms)
    override_pass_index = 0 if len(workflow_definition.passes) == 1 else None
    run_settings.overrides = list(run_settings.overrides) + _coerce_input_overrides(
        inputs,
        default_pass_index=override_pass_index,
    )
    return _run_workflow(workflow_definition, session=session, settings=run_settings, io_runtime=io_runtime)


def prepare_render(
    fragment_shader,
    *,
    outputs=None,
    output=None,
    size=512,
    inputs=None,
    vertex_shader=None,
    verbosity=3,
    session=None,
):
    """Prepare one fullscreen render workflow for repeated execution."""

    workflow = render_workflow(
        fragment_shader,
        outputs=outputs,
        output=output,
        size=size,
        inputs=inputs,
        vertex_shader=vertex_shader,
        verbosity=verbosity,
        session=session,
    )
    return _prepare_workflow(workflow, session=session, error_label="render workflow")


def prepare_image(
    fragment_shader,
    *,
    output=None,
    outputs=None,
    size=512,
    inputs=None,
    verbosity=3,
    session=None,
):
    """Prepare one fullscreen image-processing workflow for repeated execution."""

    return prepare_render(
        fragment_shader,
        output=output,
        outputs=outputs,
        size=size,
        inputs=inputs,
        verbosity=verbosity,
        session=session,
    )


def prepare_compute(
    shader,
    *,
    outputs=None,
    output=None,
    size,
    inputs=None,
    counters=None,
    workgroup_size=(16, 16),
    verbosity=3,
    session=None,
):
    """Prepare one compute workflow for repeated execution."""

    workflow = compute_workflow(
        shader,
        outputs=outputs,
        output=output,
        size=size,
        inputs=inputs,
        counters=counters,
        workgroup_size=workgroup_size,
        verbosity=verbosity,
        session=session,
    )
    return _prepare_workflow(workflow, session=session, error_label="compute workflow")


def render(
    fragment_shader,
    *,
    outputs=None,
    output=None,
    size=512,
    inputs=None,
    vertex_shader=None,
    verbosity=3,
    session=None,
    settings=None,
    system_uniforms=None,
):
    """Build and run one fullscreen render workflow."""

    workflow = render_workflow(
        fragment_shader,
        outputs=outputs,
        output=output,
        size=size,
        inputs=inputs,
        vertex_shader=vertex_shader,
        verbosity=verbosity,
        session=session,
    )
    run_settings = _merge_run_settings(settings=settings, system_uniforms=system_uniforms)
    return _run_workflow(workflow, session=session, settings=run_settings)


def image(
    fragment_shader,
    *,
    output=None,
    outputs=None,
    size=512,
    inputs=None,
    verbosity=3,
    session=None,
    settings=None,
    system_uniforms=None,
):
    """Build and run one fullscreen image-processing workflow."""

    return render(
        fragment_shader,
        output=output,
        outputs=outputs,
        size=size,
        inputs=inputs,
        verbosity=verbosity,
        session=session,
        settings=settings,
        system_uniforms=system_uniforms,
    )


def compute(
    shader,
    *,
    outputs=None,
    output=None,
    size,
    inputs=None,
    counters=None,
    workgroup_size=(16, 16),
    verbosity=3,
    session=None,
    settings=None,
    system_uniforms=None,
):
    """Build and run one compute workflow."""

    workflow = compute_workflow(
        shader,
        outputs=outputs,
        output=output,
        size=size,
        inputs=inputs,
        counters=counters,
        workgroup_size=workgroup_size,
        verbosity=verbosity,
        session=session,
    )
    run_settings = _merge_run_settings(settings=settings, system_uniforms=system_uniforms)
    return _run_workflow(workflow, session=session, settings=run_settings)


class BatchResultView:
    """High-level Python wrapper over rawgl.BatchResult."""

    def __init__(self, result):
        self._result = result
        self._run_result = _wrap_run_result(result.run_result)

    def __getattr__(self, name):
        return getattr(self._result, name)

    @property
    def raw_result(self):
        return self._result

    @property
    def run_result(self):
        return self._run_result

    @property
    def success(self):
        return self._run_result.success

    @property
    def error_message(self):
        return self._run_result.error_message

    @property
    def output_arrays(self):
        return self._run_result.output_arrays

    def output_array(self, name=None):
        return self._run_result.output_array(name)

    @property
    def counters(self):
        return self._run_result.counters

    def counter(self, name=None):
        return self._run_result.counter(name)

    def counter_array(self, base_name: str, pass_index: int = 0):
        return self._run_result.counter_array(base_name, pass_index=pass_index)


class BatchJobHandleView:
    """High-level Python wrapper over rawgl.BatchJobHandle."""

    def __init__(self, handle):
        self._handle = handle

    @property
    def raw_handle(self):
        return self._handle

    def wait(self):
        return BatchResultView(self._handle.wait())

    def close(self):
        self._handle = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()
        return False

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


class BatchPreparedJob:
    """High-level Python wrapper over rawgl.BatchPreparedWorkflow."""

    def __init__(self, prepared_workflow, batch_runner, default_pass_index: int | None = 0):
        self._prepared_workflow = prepared_workflow
        self._batch_runner = batch_runner
        self._default_pass_index = None if default_pass_index is None else int(default_pass_index)

    @property
    def raw_workflow(self):
        return self._prepared_workflow

    @property
    def batch_runner(self):
        return self._batch_runner

    def submit(self, *, inputs=None, settings=None, system_uniforms=None, cancellation=None):
        request = BatchSubmitRequest()
        request.settings = _merge_run_settings(settings=settings, system_uniforms=system_uniforms)
        request.settings.overrides = list(request.settings.overrides) + _coerce_input_overrides(
            inputs,
            default_pass_index=self._default_pass_index,
        )
        return BatchJobHandleView(
            self._batch_runner.submit(self._prepared_workflow, request, cancellation)
        )

    def run(self, *, inputs=None, settings=None, system_uniforms=None, cancellation=None):
        return self.submit(
            inputs=inputs,
            settings=settings,
            system_uniforms=system_uniforms,
            cancellation=cancellation,
        ).wait()

    def close(self):
        self._prepared_workflow = None
        self._batch_runner = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()
        return False

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


class BatchRunnerView:
    """Python lifetime wrapper over rawgl.BatchRunner."""

    def __init__(self, runner, *, session=None, io_runtime=None):
        self._runner = runner
        self._session = session
        self._io_runtime = io_runtime

    def __getattr__(self, name):
        if self._runner is None:
            raise AttributeError(f"BatchRunnerView is closed; no attribute '{name}'")
        return getattr(self._runner, name)

    @property
    def raw_runner(self):
        return self._runner

    @property
    def session(self):
        return self._session

    @property
    def io_runtime(self):
        return self._io_runtime

    def close(self):
        runner = self._runner
        if runner is not None and hasattr(runner, "close"):
            runner.close()
        self._runner = None
        self._io_runtime = None
        self._session = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb):
        self.close()
        return False

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


def prepare_batch_workflow(
    batch_runner,
    workflow=None,
    *,
    passes=None,
    verbosity=3,
):
    """Prepare one workflow for batch submission."""

    workflow_definition = _coerce_workflow_definition(workflow, passes=passes, verbosity=verbosity)
    prepare = batch_runner.prepare(workflow_definition)
    if not prepare.success:
        raise RuntimeError(prepare.error_message or "batch workflow preparation failed")
    prepared_workflow = prepare.take_workflow()
    if prepared_workflow is None:
        raise RuntimeError("batch workflow preparation returned no BatchPreparedWorkflow")
    default_pass_index = 0 if len(workflow_definition.passes) == 1 else None
    return BatchPreparedJob(prepared_workflow, batch_runner, default_pass_index=default_pass_index)


def prepare_batch_render(
    batch_runner,
    fragment_shader,
    *,
    outputs=None,
    output=None,
    size=512,
    inputs=None,
    vertex_shader=None,
    verbosity=3,
):
    output_spec = _resolve_output_arguments(outputs=outputs, output=output)
    if _outputs_require_inspection(output_spec):
        raise RuntimeError("batch render preparation requires explicit output names; unnamed output inference is not supported")
    session = getattr(batch_runner, "session", None)
    workflow = render_workflow(
        fragment_shader,
        outputs=output_spec,
        size=size,
        inputs=inputs,
        vertex_shader=vertex_shader,
        verbosity=verbosity,
        session=session,
    )
    return prepare_batch_workflow(batch_runner, workflow)


def prepare_batch_image(
    batch_runner,
    fragment_shader,
    *,
    output=None,
    outputs=None,
    size=512,
    inputs=None,
    verbosity=3,
):
    output_spec = _resolve_output_arguments(outputs=outputs, output=output)
    if _outputs_require_inspection(output_spec):
        raise RuntimeError("batch image preparation requires explicit output names; unnamed output inference is not supported")
    session = getattr(batch_runner, "session", None)
    workflow = render_workflow(
        fragment_shader,
        outputs=output_spec,
        size=size,
        inputs=inputs,
        verbosity=verbosity,
        session=session,
    )
    return prepare_batch_workflow(batch_runner, workflow)


def prepare_batch_compute(
    batch_runner,
    shader,
    *,
    outputs=None,
    output=None,
    size,
    inputs=None,
    counters=None,
    workgroup_size=(16, 16),
    verbosity=3,
):
    output_spec = _resolve_output_arguments(outputs=outputs, output=output)
    if _outputs_require_inspection(output_spec):
        raise RuntimeError("batch compute preparation requires explicit output names; unnamed output inference is not supported")
    session = getattr(batch_runner, "session", None)
    workflow = compute_workflow(
        shader,
        outputs=output_spec,
        size=size,
        inputs=inputs,
        counters=counters,
        workgroup_size=workgroup_size,
        verbosity=verbosity,
        session=session,
    )
    return prepare_batch_workflow(batch_runner, workflow)


def _session_prepare_render(self, *args, **kwargs):
    return prepare_render(*args, session=self, **kwargs)


def _session_prepare_image(self, *args, **kwargs):
    return prepare_image(*args, session=self, **kwargs)


def _session_prepare_compute(self, *args, **kwargs):
    return prepare_compute(*args, session=self, **kwargs)


def _session_prepare_workflow(self, workflow=None, *, passes=None, verbosity=3, io_runtime=None):
    return prepare_workflow(
        workflow,
        passes=passes,
        verbosity=verbosity,
        session=self,
        io_runtime=io_runtime,
    )


def _session_batch(self, options=None, io_runtime=None):
    batch_runner_type = globals().get("BatchRunner")
    if batch_runner_type is None:
        raise RuntimeError("rawgl batch bindings are not available in this build")
    if options is None:
        options = BatchRunnerOptions()
    if io_runtime is None:
        io_runtime = _default_io_runtime()
    if io_runtime is not None:
        return BatchRunnerView(batch_runner_type(self, io_runtime, options), session=self, io_runtime=io_runtime)
    return BatchRunnerView(batch_runner_type(self, options), session=self, io_runtime=None)


def _batch_runner_prepare_workflow(self, workflow=None, *, passes=None, verbosity=3):
    return prepare_batch_workflow(self, workflow, passes=passes, verbosity=verbosity)


def _batch_runner_prepare_render(self, *args, **kwargs):
    return prepare_batch_render(self, *args, **kwargs)


def _batch_runner_prepare_image(self, *args, **kwargs):
    return prepare_batch_image(self, *args, **kwargs)


def _batch_runner_prepare_compute(self, *args, **kwargs):
    return prepare_batch_compute(self, *args, **kwargs)


Session.prepare_render = _session_prepare_render
Session.prepare_image = _session_prepare_image
Session.prepare_compute = _session_prepare_compute
Session.prepare_workflow = _session_prepare_workflow
Session.batch = _session_batch

if "BatchRunner" in globals():
    BatchRunner.prepare_workflow = _batch_runner_prepare_workflow
    BatchRunner.prepare_render = _batch_runner_prepare_render
    BatchRunner.prepare_image = _batch_runner_prepare_image
    BatchRunner.prepare_compute = _batch_runner_prepare_compute
    BatchRunnerView.prepare_workflow = _batch_runner_prepare_workflow
    BatchRunnerView.prepare_render = _batch_runner_prepare_render
    BatchRunnerView.prepare_image = _batch_runner_prepare_image
    BatchRunnerView.prepare_compute = _batch_runner_prepare_compute


__all__ = [name for name in globals() if not name.startswith("_")]
