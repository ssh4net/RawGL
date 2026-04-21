..
  SPDX-License-Identifier: Apache-2.0

C++ Facade
==========

The public C++ API describes work at the workflow level, not at the OpenGL
command level.

The main public headers are:

- ``rawgl/rawgl.h``
- ``rawgl/rawgl_io.h``
- ``rawgl/rawgl_batch.h``
- ``rawgl/rawgl_cli.h``

Core workflow shape
-------------------

The normal in-memory C++ path is:

.. code-block:: cpp

   #include <rawgl/rawgl.h>

   rawgl::Session session;

   rawgl::Workflow workflow;
   workflow.verbosity = 3;

   rawgl::Pass pass;
   pass.programKind = rawgl::ShaderProgramKind::vertfrag;
   pass.sizeX = 512;
   pass.sizeY = 512;
   // Fill pass.shaderModules, pass.inputs, pass.outputs...

   workflow.passes.push_back(std::move(pass));

   rawgl::PrepareResult prepared = session.prepare(workflow);
   if (!prepared.success) {
       // prepared.errorMessage
   }

   rawgl::RunSettings settings;
   rawgl::RunResult result = prepared.workflow->run(settings);

When to use this path:

- you already have image data in memory
- your host application owns the surrounding orchestration
- you want explicit control over preparation vs execution

Static versus per-run data
--------------------------

Put stable structure in the workflow:

- pass list
- shaders
- output declarations
- fixed pass-to-pass wiring

Put changing data in ``RunSettings``:

- input overrides
- host-memory image payloads
- system uniform state

That separation is what makes ``PreparedWorkflow`` useful.

Host-memory images
------------------

Prefer host-memory payloads over file paths in the core API.

That means:

- ``rawgl_core`` should remain useful even without file IO
- file-backed workflows should move through ``rawgl::io::IoRuntime``

See :doc:`io` for the file-oriented translation layer.
