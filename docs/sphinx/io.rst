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
   rawgl::Pass pass;
   workflow.passes.push_back(pass);

   std::vector<rawgl::io::FileInputBinding> file_inputs;
   file_inputs.push_back(rawgl::io::FileTextureInput(0, "u_src0", "input.png"));

   std::vector<rawgl::io::FileOutputBinding> file_outputs;
   file_outputs.push_back(rawgl::io::FileOutput(0, "out_color", "output.png"));

   rawgl::io::PrepareWorkflowResult prepared = io_runtime.prepare(session, workflow, file_inputs, file_outputs);
   if (!prepared.success) {
       // prepared.errorMessage
   }

   rawgl::RunResult result = prepared.workflow->run(rawgl::io::RunRequest{});

Direct file helpers
-------------------

``rawgl::io`` also exposes direct file helpers:

- ``LoadImageFile(...)``
- ``SaveImageFile(...)``
- ``ReadMetadataFile(...)``
- ``ReadMetadataDocumentFile(...)``

These are useful when you want to explicitly move data between files and
``HostImageData`` or inspect metadata without preparing a whole workflow.

File-oriented workflow helpers
------------------------------

``rawgl::io`` also exposes small workflow-construction helpers for the
file-backed path:

- ``FileTextureInput(...)``
- ``FileTextureOverride(...)``
- ``FileOutput(...)``

Use these to keep file-backed state in ``rawgl_io`` instead of mixing it into
the memory-first ``rawgl::Workflow`` surface.

Metadata
--------

``rawgl::io`` also owns file-backed metadata readback.

Use metadata readback when:

- you need EXIF, XMP, IPTC, ICC, BMFF, JUMBF, or PNG text fields from a file
- the metadata inspection step should stay separate from workflow execution
- you want the file boundary to remain explicit

Current support includes preview reads and typed metadata documents for
inspection.

Python exposes the same path through:

- ``rawgl.io.read_metadata(...)``
- ``rawgl.io.read_metadata_document(...)``
- ``rawgl.MetadataReadRequest``
- ``rawgl.IoRuntime.read_metadata_file(...)``

Use the preview path when you want printable metadata entries.

Use the typed document path when you want a RawGL-owned typed representation of
the metadata families that were read.

Current save helpers do not preserve source metadata. Native metadata write and
transfer are deferred until `rawgl_io` has format-family-aware writers instead
of the removed OIIO-attribute bridge.

When to prefer IoRuntime
------------------------

Prefer ``IoRuntime`` when:

- the workflow is file-oriented
- you want file inputs/outputs but still want the public C++ façade
- you want the file translation boundary to stay explicit

Prefer pure ``Session`` / ``PreparedWorkflow`` when:

- your host application already owns the image data in memory
- you want the smallest execution dependency surface
