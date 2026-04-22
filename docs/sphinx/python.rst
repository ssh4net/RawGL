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
- ``rawgl.io.read_metadata(...)``
- ``rawgl.io.read_metadata_document(...)``

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
- best-supported dtypes: ``uint8``, ``uint16``, ``float16``, ``float32``

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
- ``rawgl.MetadataReadRequest`` and ``rawgl.IoRuntime.read_metadata_file(...)``
  for explicit preview control
- ``rawgl.MetadataDocumentReadRequest`` and
  ``rawgl.IoRuntime.read_metadata_document_file(...)`` for explicit typed
  document reads

This path requires an OpenMeta-enabled build of RawGL.

Current save helpers do not preserve source metadata.
Save-side transfer is deferred until RawGL has native format-family metadata
writers instead of the removed OIIO-attribute bridge.

Typical inspection shape:

.. code-block:: python

   import rawgl

   document = rawgl.io.read_metadata_document(
       "input.png",
       name_style=rawgl.MetadataNameStyle.oiio,
       name_policy=rawgl.MetadataNamePolicy.exif_tool_alias,
   )

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
