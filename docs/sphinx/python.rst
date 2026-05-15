..
  SPDX-License-Identifier: Apache-2.0

Python API
==========

The Python API has two layers.

Start with the high-level helper layer.

It is the simplest path for scripts, notebooks, and NumPy-driven processing.

High-level helpers
------------------

The normal path is the high-level helper layer:

- ``rawgl.image(...)``
- ``rawgl.render(...)``
- ``rawgl.compute(...)``
- ``rawgl.prepare_image(...)``
- ``rawgl.prepare_render(...)``
- ``rawgl.prepare_compute(...)``
- ``rawgl.inspect_mesh_file(...)``

Use the explicit ``rawgl.io`` helper surface when the workflow starts or ends
with files:

- ``rawgl.io.image(...)``
- ``rawgl.io.render(...)``
- ``rawgl.io.compute(...)``
- ``rawgl.io.prepare_image(...)``
- ``rawgl.io.prepare_render(...)``
- ``rawgl.io.prepare_compute(...)``
- ``rawgl.io.load_image(...)``
- ``rawgl.io.save_image(...)``
- ``rawgl.io.capabilities()``
- ``rawgl.io.read_metadata(...)``
- ``rawgl.io.read_metadata_document(...)``

``rawgl.io.load_image(...)`` and ``rawgl.io.save_image(...)`` accept
``codec_options=`` for native JPEG, PNG, TIFF, and OpenEXR controls. Prefer
this over string attributes in new Python code.

.. code-block:: python

   jpeg = rawgl.io.JpegLoadOptions()
   jpeg.has_color_transform = True
   jpeg.color_transform = rawgl.io.JpegLoadColorTransform.rgb

   load_codec = rawgl.io.ImageCodecLoadOptions()
   load_codec.has_backend_policy = True
   load_codec.backend_policy = rawgl.io.ImageLoadBackendPolicy.native_only
   load_codec.has_jpeg = True
   load_codec.jpeg = jpeg

   image = rawgl.io.load_image("input.jpg", codec_options=load_codec)

   png = rawgl.io.PngSaveOptions()
   png.has_compression_level = True
   png.compression_level = 0

   save_codec = rawgl.io.ImageCodecSaveOptions()
   save_codec.has_png = True
   save_codec.png = png

   rawgl.io.save_image(image, "output.png", bits=16, codec_options=save_codec)

This layer is intended for:

- scripts
- notebooks
- NumPy-driven processing
- quick pipeline prototyping

The most common entry point is ``rawgl.image(...)`` for fullscreen
image-processing passes.

See :doc:`examples` for example-driven workflow shapes, including single-pass,
multi-pass, mesh, and batch-oriented patterns.

Run one fullscreen pass:

.. code-block:: python

   import rawgl

   result = rawgl.image(
       fragment_shader,
       size=(512, 512),
       inputs={"u_src0": source_array},
       output={"format": "rgba32f", "capture_to_host": True},
   )

   array = result.output_array()

Run one file-oriented fullscreen pass:

.. code-block:: python

   import rawgl

   result = rawgl.io.image(
       fragment_shader,
       size=(512, 512),
       inputs={"u_src0": "input.png"},
       output="output.png",
   )

Read metadata from one file-backed image or container:

.. code-block:: python

   import rawgl

   entries = rawgl.io.read_metadata(
       "output.tif",
       name_style=rawgl.MetadataNameStyle.oiio,
       name_policy=rawgl.MetadataNamePolicy.exif_tool_alias,
   )

   for entry in entries[:8]:
       print(entry.name, entry.value_text)

This path intentionally hides:

- explicit quad mesh setup
- explicit vertex shader setup for the common fullscreen case
- low-level OpenGL command sequencing

NumPy inputs and outputs
------------------------

When NumPy is installed:

- host image inputs can be passed as ``numpy.ndarray``
- captured host outputs can be read back as arrays

Current practical expectations:

- image shape: ``HxW`` or ``HxWxC``
- channels: ``1`` to ``4``
- best-supported dtypes: ``uint8``, ``uint16``, ``uint32``, ``float16``, ``float32``

If your upstream pipeline produces ``float64``, cast to ``float32`` before
passing the array into RawGL.

This is the intended integration point for pipelines built around:

- NumPy
- scikit-image
- OpenCV
- other Python preprocessing stages that already hold image data in memory

Metadata
--------

Metadata inspection is file-oriented and lives under ``rawgl.io``.

Current Python metadata support includes:

- ``rawgl.io.read_metadata(...)`` for preview-oriented inspection
- ``rawgl.io.read_metadata_document(...)`` for typed metadata reads
- ``rawgl.io.transfer_image_metadata(...)`` for explicit transfer into an
  already-written JPEG, TIFF, PNG, or EXR target
- ``rawgl.io.save_image(..., source_metadata=document)`` for the common save
  then transfer workflow
- ``rawgl.MetadataTransferSafety`` for choosing rendered-image or compatible
  file transfer policy
- ``rawgl.MetadataReadRequest`` and ``rawgl.IoRuntime.read_metadata_file(...)``
  for explicit preview control
- ``rawgl.MetadataDocumentReadRequest`` and
  ``rawgl.IoRuntime.read_metadata_document_file(...)`` for explicit typed
  document reads

This path requires an OpenMeta-enabled build of RawGL.

Typical transfer shape:

.. code-block:: python

   import rawgl

   image = rawgl.io.load_image("input.jpg")
   document = rawgl.io.read_metadata_document(
       "input.jpg",
       name_style=rawgl.MetadataNameStyle.oiio,
       name_policy=rawgl.MetadataNamePolicy.exif_tool_alias,
   )
   rawgl.io.save_image(image, "output.tif", bits=16, source_metadata=document)

The default metadata safety mode is rendered-image transfer. Use
``metadata_safety=rawgl.MetadataTransferSafety.compatible_file`` only when the
output is a compatible repackage or recompression of the source pixels.

Mesh inspection
---------------

Use ``rawgl.inspect_mesh_file(...)`` when a script needs mesh facts before it
builds a workflow. The current detailed path is OBJ-focused and reports source
counts, bounds, UV range, group spans, and ``usemtl`` material IDs without
loading any MTL files.

.. code-block:: python

   import rawgl

   mesh_info = rawgl.inspect_mesh_file("model.obj")
   if not mesh_info.success:
       raise RuntimeError(mesh_info.error_message)

   material_ids = {material.name: material.id for material in mesh_info.materials}

Host meshes from NumPy
----------------------

Use ``rawgl.make_host_mesh(...)`` when mesh data already lives in Python memory.
The default vertex layout is stable:

- location 0: ``vec3 position``
- location 1: ``vec2 texcoord``
- location 2: ``vec3 normal``
- location 3: ``uvec4 color_rgba``
- location 4: ``uint id0``

Extra attributes use explicit locations ``>= 5``:

.. code-block:: python

   mesh = rawgl.make_host_mesh(
       positions=positions_f32,              # float32[N, 3]
       indices=triangles_u32,                # uint32[T, 3] or uint32[I]
       normals=normals_f32,
       uint_attrs={"source_triangle_id": ids_u32},
       attributes={
           "region_id": rawgl.vertex_attr(region_ids_u32, location=5),
       },
   )

   workflow = rawgl.build_workflow(
       rawgl.render_pass(
           fragment_shader,
           vertex_shader=vertex_shader,
           size=(1024, 1024),
           meshes={"head": mesh},
           outputs={
               "Color": {"format": "rgba32f", "channels": 4, "capture_to_host": True},
               "TriangleId": {"format": "r32ui", "channels": 1, "capture_to_host": True},
           },
       )
   )

   prepared = rawgl.prepare_workflow(workflow)
   result = prepared.run(
       mesh_updates={
           "head": {
               "positions": next_positions_f32,
               "normals": next_normals_f32,
           }
       }
   )

For topology changes, replace the named binding for one run:

.. code-block:: python

   result = prepared.run(meshes={"head": rebuilt_mesh})
   result = prepared.run(meshes={(2, "head"): rebuilt_mesh_for_pass_2})

``meshes=`` is the short spelling for per-run full mesh replacement.
``mesh_updates=`` keeps the prepared topology and updates only positions and
normals, which is the faster path for animated meshes with fixed indices and
attributes. If the same named mesh appears in several passes, an unscoped
``meshes={"head": mesh}`` override applies to each matching pass; use
``(pass_index, "head")`` when only one pass should change.

``r32ui`` captured outputs are returned as ``numpy.uint32`` arrays. This is the
current path for ID, mask, and picking buffers.

Prepared workflows
------------------

For repeated runs:

.. code-block:: python

   import rawgl

   session = rawgl.Session()
   prepared = session.prepare_image(
       fragment_shader,
       size=(512, 512),
       outputs={
           "out_color": {
               "format": "rgba32f",
               "capture_to_host": True,
           }
       },
   )

   result = prepared.run(inputs={"u_src0": source_array})
   array = result.output_array("out_color::0")

Use prepared workflows when:

- the shaders stay fixed
- the pass graph stays fixed
- only the inputs or uniforms change per run

Batch
-----

Batch bindings exist and are intended for repeated multi-entry processing.

Rule:

- batch helpers require **explicit output names**
- unnamed single-output inference is not supported on batch preparation paths
- per-run file outputs require a runner created with ``io_runtime=``

That means this is preferred:

.. code-block:: python

   prepared = runner.prepare_image(
       fragment_shader,
       size=(512, 512),
       inputs={"u_src0": source_array},
       outputs={
           "out_color": {
               "format": "rgba32f",
               "capture_to_host": True,
           }
       },
   )

and not an unnamed output shorthand that requires shader inspection during
batch preparation.

For sequence or batch output naming, capture the workflow output and pass a
file output target per submitted job:

.. code-block:: python

   io_runtime = rawgl.io.Runtime()
   runner = session.batch(io_runtime=io_runtime)
   prepared = rawgl.prepare_batch_workflow(runner, workflow)

   handle = prepared.submit(
       inputs={(0, "u_src0"): source_array},
       outputs={
           (1, "FrameOut"): {
               "path": "frames/frame_000.png",
               "format": "rgba32f",
               "channels": 4,
               "alpha_channel": 3,
               "bits": 16,
           }
       },
       system_uniforms={"frame": 0, "time": 0.0},
   )

Advanced multi-pass workflows
-----------------------------

The high-level helpers are intentionally biased toward single-pass use. When you
need a real multi-pass graph, use the explicit workflow builder helpers:

- ``rawgl.image_pass(...)``
- ``rawgl.render_pass(...)``
- ``rawgl.compute_pass(...)``
- ``rawgl.pass_output(...)``
- ``rawgl.build_workflow(...)``

That is the current path for more complex graphs while the simpler Python layer
continues to evolve.

Advanced layer
--------------

The direct bound façade is available under:

.. code-block:: python

   rawgl.advanced

Use that when you need explicit control over:

- ``Workflow``
- ``Pass``
- ``InputBinding``
- ``OutputBinding``
- lower-level request/result objects

Top-level helpers are memory-first. Use ``rawgl.io`` for file-backed workflows.
