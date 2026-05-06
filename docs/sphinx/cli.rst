..
  SPDX-License-Identifier: Apache-2.0

CLI
===

Use the command-line interface (CLI) when your workflow starts and ends with
files.

It is pass-oriented and scriptable, and it maps naturally onto Windows batch
files and shell scripts.

See :doc:`examples` for a grouped view of the single-pass and multi-pass
workflow shapes that the CLI and Python layers both support.

Its strengths are:

- direct file-oriented processing
- reproducible batch scripts
- explicit pass ordering
- easy integration into shell and Windows batch workflows

It is not intended to expose low-level GL command choreography to the user.

Fragment workflow example
-------------------------

This is the same style used in the repository smoke tests:

.. code-block:: bat

   RawGL.exe ^
     --verbosity 5 ^
     -P shaders\empty.vert shaders\pass1.frag ^
     --pass_size 1024 ^
     --in InSample inputs\EmptyPresetLUT.png ^
     --out OutSample outputs\pass1.tif ^
     --out_format rgb32f ^
     --out_channels 3 ^
     --out_bits 32 ^
     --out_tiff_compression deflate ^
     --out_tiff_predictor float

This is the normal pattern:

- declare one pass
- provide named inputs
- declare outputs and output format
- let RawGL handle the actual rendering/execution steps

Multi-pass example
------------------

Pass outputs can be referenced by later passes:

.. code-block:: bat

   RawGL.exe ^
     --verbosity 5 ^
     -P shaders\empty.vert shaders\pass1.frag ^
     --pass_size 1024 ^
     --in InSample inputs\EmptyPresetLUT.png ^
     --out OutSample outputs\pass1.tif ^
     -P shaders\empty.vert shaders\pass2.frag ^
     --in InSample2 OutSample::0 ^
     --out OutSample2 outputs\pass2.tif

Pass-to-pass references are one of the main strengths of the CLI path when you
still want file-oriented scripting but need multi-stage processing on the GPU.

Native IO controls
------------------

Use named codec options for common native formats. These options map to the
same typed C++ and Python codec controls used by ``rawgl_io``.

.. code-block:: bat

   RawGL.exe ^
     --pass_vertfrag shaders\empty.vert shaders\pass1.frag ^
     --pass_size 1024 ^
     --in InSample inputs\sky.jpg ^
     --in_backend native_only ^
     --in_jpeg_color_transform rgb ^
     --out OutSample outputs\pass1.tif ^
     --out_format rgba32f ^
     --out_channels 4 ^
     --out_alpha_channel 3 ^
     --out_bits 16 ^
     --out_tiff_layout tiled ^
     --out_tiff_tile_size 256 256 ^
     --out_tiff_compression deflate ^
     --out_tiff_predictor horizontal

The current named option groups are:

- input backend, JPEG color transform, PNG transparency expansion, TIFF
  directory index, and OpenEXR channel selection
- JPEG quality, progressive mode, Huffman optimization, and chroma subsampling
- PNG compression level and interlace mode
- TIFF compression, predictor, strips/tiled layout, tile size, rows per strip,
  BigTIFF, alpha association, and codec-specific quality/level switches
- OpenEXR compression, scanline/tiled layout, tile size, line order, and DWA
  level

``--in_attr`` and ``--out_attr`` remain available for compatibility and
backend-specific escape hatches. Prefer named options when RawGL exposes the
setting directly.

Compute example
---------------

.. code-block:: bat

   RawGL.exe ^
     --verbosity 5 ^
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

This path is useful for image-style compute workflows, histogram/statistics
passes, and intermediate GPU-only processing steps.

IO notes
--------

Use the CLI when file inputs and outputs are the natural boundary.

If you need NumPy arrays or explicit embedding, use the Python or C++ APIs
instead.
