..
  SPDX-License-Identifier: Apache-2.0

IO Runtime
==========

``rawgl::io::IoRuntime`` is the public file-oriented translation layer.

Use it when the workflow naturally begins or ends with files.

What it owns
------------

``IoRuntime`` is responsible for:

- loading file-backed images into ``HostImageData``
- materializing file-backed workflow inputs into host-memory payloads
- materializing file-backed run-time overrides into host-memory overrides
- saving captured outputs after execution

Current split:

- ``rawgl_core`` executes workflows
- ``rawgl_io`` owns file decode/encode and file-oriented workflow translation

Typical file-backed C++ path
----------------------------

.. code-block:: cpp

   #include <rawgl/rawgl.h>
   #include <rawgl/rawgl_io.h>

   rawgl::Session session;
   rawgl::io::IoRuntime io_runtime;

   rawgl::Workflow workflow;
   // Build workflow with file-backed inputs/outputs.

   rawgl::io::PrepareWorkflowResult prepared = io_runtime.prepare(session, workflow);
   if (!prepared.success) {
       // prepared.errorMessage
   }

   rawgl::RunResult result = prepared.workflow->run({});

Direct file helpers
-------------------

``rawgl::io`` also exposes direct file helpers:

- ``LoadImageFile(...)``
- ``SaveImageFile(...)``

These are useful when you want to explicitly move data between files and
``HostImageData`` without preparing a whole workflow.

When to prefer IoRuntime
------------------------

Prefer ``IoRuntime`` when:

- the workflow is file-oriented
- you want file inputs/outputs but still want the public C++ façade
- you want the file translation boundary to stay explicit

Prefer pure ``Session`` / ``PreparedWorkflow`` when:

- your host application already owns the image data in memory
- you want the smallest execution dependency surface
