..
  SPDX-License-Identifier: Apache-2.0

Quick Start
===========

This page shows the shortest usable paths for RawGL.

For fuller workflow patterns, see :doc:`examples`.

Dependency Prefixes
-------------------

RawGL uses standard CMake discovery. Put dependency prefixes in
``CMAKE_PREFIX_PATH``; do not rely on machine-local paths in project files.

For a libc++ dependency stack on Linux:

.. code-block:: sh

   export CMAKE_PREFIX_PATH=/path/to/dependency/prefix
   cmake --preset linux-release-python-core-libcxx
   cmake --build --preset linux-release-python-core-libcxx
   ctest --test-dir build_linux_release_python_core_libcxx --output-on-failure

For Visual Studio builds on Windows:

.. code-block:: bat

   cmake -S . -B build_vs2022 -G "Visual Studio 17 2022" -A x64 ^
     -DCMAKE_PREFIX_PATH=C:/path/to/dependency/prefix

Python
------

Use the high-level Python helpers for the fastest start.

.. code-block:: python

   import rawgl

   result = rawgl.io.image(
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

Start with ``rawgl.image(...)`` for in-memory or NumPy-driven work.

Use ``rawgl.io.image(...)`` when the workflow is file-oriented. Use
``capture_to_host=True`` to read the result back as a NumPy array.

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
   #include <rawgl/rawgl_io.h>

   rawgl::Session session;
   rawgl::io::IoRuntime io_runtime;

   rawgl::Workflow workflow;
   rawgl::Pass pass;
   workflow.passes.push_back(std::move(pass));

   std::vector<rawgl::io::FileInputBinding> file_inputs;
   file_inputs.push_back(rawgl::io::FileTextureInput(0, "u_src0", "input.png"));

   std::vector<rawgl::io::FileOutputBinding> file_outputs;
   file_outputs.push_back(rawgl::io::FileOutput(0, "out_color", "output.png"));

   rawgl::io::PrepareWorkflowResult prepared = io_runtime.prepare(session, workflow, file_inputs, file_outputs);
   if (!prepared.success) {
       // Handle prepared.errorMessage.
   }

   rawgl::RunResult result = prepared.workflow->run(rawgl::io::RunRequest{});

Create a ``Session``. Build a ``Workflow``. Prepare it. Run it.

Use:

- ``rawgl::io::IoRuntime`` for file-backed workflows
- ``rawgl::HostTextureInput(...)`` and ``rawgl::CapturedOutput(...)`` for in-memory workflows
