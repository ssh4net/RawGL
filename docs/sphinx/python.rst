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
