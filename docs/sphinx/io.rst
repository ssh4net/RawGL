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

Codec backends
--------------

``rawgl_io`` uses native backends for the common formats that need predictable
settings control:

- JPEG through libjpeg-turbo/libjpeg
- PNG through libpng
- TIFF through libtiff, including tiled TIFF and BigTIFF
- OpenEXR through OpenEXR, including tiled EXR

OpenImageIO remains the fallback path for formats that have not moved to native
backends yet, such as camera RAW, JPEG-2000, TGA, HDR, WebP, and other plugins
available in the dependency build.

Native writers are strict for native output families. If a native JPEG, PNG,
TIFF, or OpenEXR writer rejects an option, RawGL returns that error instead of
silently retrying the write through OpenImageIO. The read path may still fall
back to OpenImageIO when a native reader cannot decode a supported format
variant.

Native codec options
--------------------

JPEG output accepts:

- ``jpeg:quality`` or ``jpg:quality``: integer quality from ``1`` to ``100``
- ``jpeg:progressive`` or ``jpg:progressive``: ``true``/``false``
- ``oiio:Compression`` with ``jpeg:N`` or ``jpg:N``: legacy quality spelling

JPEG writes grayscale or RGB data. Alpha channels are omitted.

PNG output accepts:

- ``png:compressionLevel``: integer from ``0`` to ``9``
- ``png:interlace`` or ``png:interlaced``: ``true`` enables Adam7 interlace

PNG writes 8-bit and 16-bit integer images.

TIFF output accepts:

- ``tiff:compression``: ``none``, ``lzw``, ``packbits``, ``zip``, ``deflate``,
  or ``adobe_deflate``
- ``compression`` or ``oiio:Compression``: legacy compression spelling
- ``tiff:predictor``: ``none``/``1``, ``horizontal``/``2``, or ``float``/``3``
- ``tiff:tiled``: ``true``/``false``
- ``tiff:tileWidth`` and ``tiff:tileLength`` or ``tiff:tileHeight``
- ``tiff:rowsPerStrip`` for striped output
- ``tiff:bigTiff`` or ``tiff:bigtiff``: force BigTIFF output
- ``oiio:UnassociatedAlpha``: mark the extra alpha sample as unassociated

TIFF writes 8-bit, 16-bit, and 32-bit float output. ``tiff:rowsPerStrip`` is
not valid when tiled output is enabled. TIFF predictors require LZW or Deflate
compression; the floating-point predictor is valid only for 32-bit float
output.

OpenEXR output accepts:

- ``openexr:compression``: any compression name accepted by OpenEXR, such as
  ``zip``, ``zips``, ``piz``, ``rle``, ``dwaa``, or ``dwab``
- ``compression`` or ``oiio:Compression``: legacy compression spelling
- ``openexr:tiled``: ``true``/``false``
- ``openexr:tileWidth`` and ``openexr:tileHeight`` or ``openexr:tileLength``
- ``openexr:dwaCompressionLevel`` for DWAA/DWAB
- ``openexr:lineOrder``: ``increasing_y``, ``decreasing_y``, or ``random_y``
- ``openexr:attribute:string:<name>``: write one string header attribute

OpenEXR writes half and 32-bit float output.

Example output attributes:

.. code-block:: bat

   RawGL.exe ^
     --out OutColor output.tif ^
     --out_bits 16 ^
     --out_attr tiff:compression zip ^
     --out_attr tiff:tiled true ^
     --out_attr tiff:tileWidth 256 ^
     --out_attr tiff:tileLength 256

.. code-block:: python

   rawgl.io.save_image(
       image,
       "output.exr",
       bits=16,
       attributes={
           "openexr:compression": "zip",
           "openexr:tiled": "true",
           "openexr:tileWidth": "64",
           "openexr:tileHeight": "64",
       },
   )

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
