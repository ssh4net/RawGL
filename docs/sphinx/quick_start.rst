..
  SPDX-License-Identifier: Apache-2.0

Quick Start
===========

This page shows the shortest usable paths for RawGL.

For fuller workflow patterns, see :doc:`examples`.

Python
------

Use the high-level Python helpers for the fastest start.

.. code-block:: python

   import rawgl

   result = rawgl.image(
       """#version 450 core
   layout(location = 0) in vec2 UV;
   layout(location = 0) out vec3 OutColor;
   void main()
   {
       OutColor = vec3(UV, 0.0);
   }
   """,
       size=512,
       output="output.png",
   )

   if not result.success:
       raise RuntimeError(result.error_message)

Start with ``rawgl.image(...)`` for one fullscreen pass.

Use ``output="output.png"`` to write a file. Use ``capture_to_host=True`` to
read the result back as a NumPy array.

CLI
---

Use the command-line interface (CLI) when your workflow starts and ends with
files.

Run one fragment pass:

.. code-block:: bat

   RawGL.exe ^
     --verbosity 3 ^
     -P shaders\empty.vert shaders\EmptyLUT.frag ^
     --pass_size 512 ^
     --in img_size 512 ^
     --in lut_size 8 ^
     --out EmptyLUT outputs\EmptyLUT.png ^
     --out_format rgb16 ^
     --out_channels 3 ^
     --out_bits 16

Run one compute pass:

.. code-block:: bat

   RawGL.exe ^
     --verbosity 3 ^
     --pass_comp shaders\test.comp ^
     --pass_size 512 ^
     --pass_workgroupsize 32 32 ^
     --in u_test true ^
     --in u_texture0 inputs\EmptyPresetLUT.png ^
     --out o_out0 outputs\comp_o_out0.tif ^
     --out_format rgba32f ^
     --out_channels 4 ^
     --out_alpha_channel 3 ^
     --out_bits 16

C++ facade
----------

Use the C++ facade when your host application already owns the surrounding
control flow.

.. code-block:: cpp

   #include <rawgl/rawgl.h>

   rawgl::Session session;
   rawgl::Workflow workflow;
   // Fill workflow.passes.

   rawgl::PrepareResult prepared = session.prepare(workflow);
   if (!prepared.success) {
       // Handle prepared.errorMessage.
   }

   rawgl::RunResult result = prepared.workflow->run({});

Create a ``Session``. Build a ``Workflow``. Prepare it. Run it.

Use ``rawgl::io::IoRuntime`` when the workflow needs file loading or saving.
