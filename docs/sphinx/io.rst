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

Codec capability query
----------------------

Use ``rawgl::io::GetImageIoCapabilities()`` when an application needs to decide
which file formats and writer controls are available in the current RawGL
build. The query reports native read/write routing, OpenImageIO fallback
availability, supported native component types, native read/write option names,
native compression modes, unavailable optional compression modes, and library
version details.

.. code-block:: cpp

   const rawgl::io::ImageIoCapabilities capabilities =
       rawgl::io::GetImageIoCapabilities();

   for (const rawgl::io::ImageCodecCapabilities& codec : capabilities.codecs) {
       if (codec.name == "tiff" && codec.nativeWrite) {
           // codec.nativeWriteCompressionModes contains the libtiff codecs
           // configured in this dependency build.
       }
   }

Python exposes the same information through ``rawgl.io.capabilities()``:

.. code-block:: python

   caps = rawgl.io.capabilities()
   for codec in caps.codecs:
       print(codec.name, codec.native_read_options, codec.native_write_options)

Native codec options
--------------------

Use typed options for direct C++ and Python loads/saves. String attributes
remain supported for CLI options, legacy scripts, and backend-specific escape
hatches. If typed options and string attributes set the same native option, the
typed option wins.

Typed load example:

.. code-block:: cpp

   rawgl::io::ImageLoadRequest request;
   request.path = "input.jpg";
   request.codecOptions.hasBackendPolicy = true;
   request.codecOptions.backendPolicy = rawgl::io::ImageLoadBackendPolicy::NativeOnly;
   request.codecOptions.hasJpeg = true;
   request.codecOptions.jpeg.hasColorTransform = true;
   request.codecOptions.jpeg.colorTransform = rawgl::io::JpegLoadColorTransform::Rgb;

   const rawgl::io::ImageLoadResult result = rawgl::io::LoadImageFile(request);

.. code-block:: python

   jpeg = rawgl.io.JpegLoadOptions()
   jpeg.has_color_transform = True
   jpeg.color_transform = rawgl.io.JpegLoadColorTransform.rgb

   codec = rawgl.io.ImageCodecLoadOptions()
   codec.has_backend_policy = True
   codec.backend_policy = rawgl.io.ImageLoadBackendPolicy.native_only
   codec.has_jpeg = True
   codec.jpeg = jpeg

   image = rawgl.io.load_image("input.jpg", codec_options=codec)

Typed save example:

.. code-block:: cpp

   rawgl::io::ImageSaveRequest request;
   request.path = "output.tif";
   request.bits = 16;
   request.image = image;
   request.codecOptions.hasTiff = true;
   request.codecOptions.tiff.hasCompression = true;
   request.codecOptions.tiff.compression = rawgl::io::TiffCompressionMode::Deflate;
   request.codecOptions.tiff.hasLayout = true;
   request.codecOptions.tiff.layout = rawgl::io::TiffStorageLayout::Tiled;
   request.codecOptions.tiff.hasTileWidth = true;
   request.codecOptions.tiff.tileWidth = 256;
   request.codecOptions.tiff.hasTileHeight = true;
   request.codecOptions.tiff.tileHeight = 256;

   const rawgl::io::ImageSaveResult result = rawgl::io::SaveImageFile(request);

.. code-block:: python

   exr = rawgl.io.OpenExrSaveOptions()
   exr.has_compression = True
   exr.compression = rawgl.io.OpenExrCompressionMode.zip
   exr.has_layout = True
   exr.layout = rawgl.io.OpenExrStorageLayout.tiled
   exr.has_tile_width = True
   exr.tile_width = 64
   exr.has_tile_height = True
   exr.tile_height = 64

   codec = rawgl.io.ImageCodecSaveOptions()
   codec.has_openexr = True
   codec.openexr = exr

   rawgl.io.save_image(image, "output.exr", bits=16, codec_options=codec)

Compatibility load attribute keys are still accepted:

- ``rawgl:load_backend`` or ``rawgl:decode_backend``: ``auto``, ``native`` /
  ``native_only``, or ``openimageio`` / ``openimageio_only``
- ``jpeg:color_transform``: ``auto``, ``rgb``, or ``grayscale``
- ``png:expand_transparency``: ``true`` expands ``tRNS`` chunks to alpha
- ``tiff:directory_index`` / ``tiff:directoryIndex`` / ``tiff:subimage``:
  zero-based TIFF directory
- ``openexr:channel_selection`` / ``openexr:channelSelection``: ``auto``,
  ``luminance``, ``rgb``, ``rgba``, or ``all``

Compatibility save attribute keys are still accepted:

JPEG output accepts:

- ``jpeg:quality`` or ``jpg:quality``: integer quality from ``1`` to ``100``
- ``jpeg:progressive`` or ``jpg:progressive``: ``true``/``false``
- ``jpeg:optimize`` or ``jpg:optimize``: optimize Huffman coding
- ``jpeg:subsampling`` or ``jpg:subsampling``: ``default``, ``444``/``4:4:4``,
  ``422``/``4:2:2``, ``420``/``4:2:0``, ``440``/``4:4:0``, or
  ``411``/``4:1:1``
- ``oiio:Compression`` with ``jpeg:N`` or ``jpg:N``: legacy quality spelling

JPEG writes grayscale or RGB data. Alpha channels are omitted. Chroma
subsampling is valid only for RGB output. Invalid quality, boolean, or
subsampling values fail the write instead of being clamped.

PNG output accepts:

- ``png:compressionLevel``: integer from ``0`` to ``9``
- ``png:compression_level``: snake_case spelling for compression level
- ``png:interlace`` or ``png:interlaced``: ``true`` enables Adam7 interlace

PNG writes 8-bit and 16-bit integer images. Invalid compression and interlace
values fail the write instead of being clamped.

TIFF output accepts:

- ``tiff:compression``: ``none``, ``lzw``, ``packbits``, ``zip``, ``deflate``,
  ``adobe_deflate``, ``jpeg``, ``lzma``, ``zstd``, ``webp``, ``jxl``,
  ``jxl_dng``, or ``lerc`` when the deployed libtiff build provides that codec
- ``compression`` or ``oiio:Compression``: legacy compression spelling
- ``tiff:predictor``: ``none``/``1``, ``horizontal``/``2``, or ``float``/``3``
- ``tiff:layout``: ``strips`` or ``tiled``. ``tiff:tiled`` is still accepted.
- ``tiff:tileWidth``/``tiff:tile_width`` and
  ``tiff:tileLength``/``tiff:tile_length`` or
  ``tiff:tileHeight``/``tiff:tile_height``
- ``tiff:rowsPerStrip`` or ``tiff:rows_per_strip`` for striped output
- ``tiff:bigTiff``, ``tiff:bigtiff``, or ``tiff:big_tiff``: force BigTIFF
  output
- ``tiff:jpegQuality``/``tiff:jpeg_quality``: JPEG-in-TIFF quality from ``1``
  to ``100``
- ``tiff:zipLevel``/``tiff:zip_level`` or
  ``tiff:deflateLevel``/``tiff:deflate_level``: Deflate level from ``1`` to
  ``9``
- ``tiff:zstdLevel``/``tiff:zstd_level``: ZSTD level from ``1`` to ``22``
- ``tiff:lzmaPreset``/``tiff:lzma_preset``: LZMA preset from ``0`` to ``9``
- ``tiff:webpLevel``/``tiff:webp_level``: WebP level from ``0`` to ``100``
- ``tiff:webpLossless``/``tiff:webp_lossless`` and
  ``tiff:webpLosslessExact``/``tiff:webp_lossless_exact`` for WebP-in-TIFF
- ``oiio:UnassociatedAlpha``: mark the extra alpha sample as unassociated

TIFF writes 8-bit, 16-bit, and 32-bit float output. ``tiff:rowsPerStrip`` is
not valid when tiled output is enabled. TIFF predictors require LZW or Deflate
compression; the floating-point predictor is valid only for 32-bit float
output. Codec-specific quality and level options must match the selected
compression mode; RawGL reports a write error instead of silently ignoring a
mismatched option.

OpenEXR output accepts:

- ``openexr:compression``: any compression name accepted by OpenEXR, such as
  ``zip``, ``zips``, ``piz``, ``rle``, ``dwaa``, or ``dwab``
- ``compression`` or ``oiio:Compression``: legacy compression spelling
- ``openexr:layout``: ``scanlines`` or ``tiled``. ``openexr:tiled`` is still
  accepted.
- ``openexr:tileWidth``/``openexr:tile_width`` and
  ``openexr:tileHeight``/``openexr:tile_height`` or
  ``openexr:tileLength``/``openexr:tile_length``
- ``openexr:dwaCompressionLevel``/``openexr:dwa_compression_level`` for
  DWAA/DWAB
- ``openexr:lineOrder``/``openexr:line_order``: ``increasing_y``,
  ``decreasing_y``, or ``random_y``
- ``openexr:attribute:string:<name>``: write one string header attribute

OpenEXR writes half and 32-bit float output. Tile dimensions are valid only for
tiled output. DWA compression level is valid only with DWAA or DWAB
compression.

Example output attributes:

.. code-block:: bat

   RawGL.exe ^
     --out OutColor output.tif ^
     --out_bits 16 ^
     --out_attr tiff:compression zip ^
     --out_attr tiff:zip_level 6 ^
     --out_attr tiff:layout tiled ^
     --out_attr tiff:tile_width 256 ^
     --out_attr tiff:tile_height 256

.. code-block:: python

   rawgl.io.save_image(
       image,
       "output.exr",
       bits=16,
       attributes={
           "openexr:compression": "zip",
           "openexr:layout": "tiled",
           "openexr:tile_width": "64",
           "openexr:tile_height": "64",
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
- ``GetImageIoCapabilities()``
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

``rawgl::io`` also owns file-backed metadata readback and explicit
source-to-target metadata transfer.

Use metadata readback when:

- you need EXIF, XMP, IPTC, ICC, BMFF, JUMBF, or PNG text fields from a file
- the metadata inspection step should stay separate from workflow execution
- you want the file boundary to remain explicit

Current support includes preview reads, typed metadata documents for
inspection, and transfer into already-written JPEG, TIFF, PNG, and EXR targets.
Transfers default to rendered-image safety, which keeps general descriptive,
capture, GPS, IPTC, and portable XMP metadata while filtering source-specific
RAW processing data, source ICC profiles, opaque MakerNotes, and non-C2PA JUMBF
payloads. Use ``MetadataTransferSafety::CompatibleFile`` only for compatible
metadata repackaging or recompression where source camera/color metadata still
matches the target pixels.

Python exposes the same path through:

- ``rawgl.io.read_metadata(...)``
- ``rawgl.io.read_metadata_document(...)``
- ``rawgl.io.transfer_image_metadata(...)``
- ``rawgl.MetadataReadRequest``
- ``rawgl.IoRuntime.read_metadata_file(...)``

Use the preview path when you want printable metadata entries.

Use the typed document path when you want a RawGL-owned typed representation of
the metadata families that were read.

Use ``TransferImageMetadataFile(...)`` or Python ``save_image(...,
source_metadata=document)`` when a generated target should inherit metadata
from a source document. The transfer path uses OpenMeta internally, but OpenMeta
types do not appear in RawGL public headers.

``metadata_safety`` can be set to
``rawgl.MetadataTransferSafety.compatible_file`` for a true compatible file
transfer; otherwise rendered-image safety is used. Advanced callers may pass an
exact target image to ``transfer_image_metadata(...)`` when the in-memory
payload matches the encoded target layout; otherwise RawGL inspects the written
target file before applying metadata.

When to prefer IoRuntime
------------------------

Prefer ``IoRuntime`` when:

- the workflow is file-oriented
- you want file inputs/outputs but still want the public C++ façade
- you want the file translation boundary to stay explicit

Prefer pure ``Session`` / ``PreparedWorkflow`` when:

- your host application already owns the image data in memory
- you want the smallest execution dependency surface
