# RawGL
Command line image processing tool using OpenGL GLSL shaders.
 
## Features

**Image import/export use OpenimageIO**\
Support all image file formats that support by OpenimageIO for import:
* Camera RAWs (oiio compiled with libraw plugin)
* OpenEXR
* TIFF
* PNG
* JPEG
* JPEG-2000 (oiio compiled with openjpeg plugin)
* etc.\
Export for this moment limited to JPG, PNG, TIFF, TGA, HDR, EXR

**On image import and export allow define OpenimageIO reading/writing options like:**
* oiio:ColorSpace ACES
* oiio:RawColor 1
* oiio:UnassociatedAlpha
* Compression jpeg:100
* jpeg:subsampling
* openexr:dwaCompressionLevel 1.0
* png:compressionLevel 6
* raw:ColorSpace ProPhoto-linear
* raw:Demosaic AHD-Mod
* etc.\
More about possible OpenimageIO options read there:
https://openimageio.readthedocs.io/en/latest/builtinplugins.html
https://openimageio.readthedocs.io/en/latest/stdmetadata.html#sec-metadata-color
https://openimageio.readthedocs.io/en/latest/oiiotool.html

**For this moment support vertex, fragment and compute shaders in text and binary SPIR-V form**
* two files: shader.vert shader.frag
* single file: shader.vertfrag
* binary: shader.vert_spv shader.frag_spv\

*For prototyping or in-house use text-based shaders are easier to manage, adapt and use without any loss of speed. But in case of possible distribution, some can prefer SPIR-V binary format shaders. They do not provide too much security and can be decompiled, but decompiling them can violate the license, and can be a useful choice if you have not planned to distribute your shaders as open source.*

As a tool for image processing in mind **RawGL** for this moment supports (hardcoded) single quad and only isometric camera.

**RawGL support multi-passes shaders**.\
That allows the run process from input images, export results as files to disk as well as pass this result to second and other shader passes.\

*For example, it is possible to create multiple shaders to open camera RAW images, do a custom demosaic in the first shader, apply color correction and/or some image processings like denoise or sharpen in the next shader, and export results to disk, in same moment whithout leaving GPU VRAM pass this results to another shader or shaders that will compile normals or separate specular components from photometric stereo capture inputs. Or import spherical panoramas, decompose them into linear projections, and pass these projections to image processing pass for preprocessing and export to disk.*

**RawGL** itself **is not a batch tool** and runs as one thread, but using **windows CMD batch, PowerShell, or Python scripts** it is possible to run **RawGL** in parallel as many as can allow your system and GPU memory. **On multi-GPU systems, it is possible to clone RawGL binary and define GPU affinity to run a specific copy on a specific GPU and use batch scripts run multi-threaded on Multi-GPUs.**\
*Native support for GPU affinity as a RawGL option is in the TODO list, but only if I found a usable way to do this. In that case, it will be possible to define GPU# in CLI. Before that only used Nvidia Control Panel and use specific RawGL binary.**

Dependencies
------------

### Required dependencies -- RawGL will not build without these
* boost
  * boost-program-options
  * boost-log
* openimageio
* glfw (included in repo)
* glad (included in repo)
* miniply (from my fork https://github.com/ssh4net/miniply)

### Optional dependencies -- features may be disabled if not found
* If you want support for camera "RAW" formats:
  * openimageio libraw plugin
* If you want support for jpeg 2000 images:
  * openimageio openjpeg plugin
  
Building and Downloads
======================
Please use **MS Visual Studio 2022**. For install dependencies easier to use **VCPKG**.
Due to some bugs in some moments **vcpkg ports** can be broken, in that case, please use vcpkg reports.

Download Binaries
-----------------
Compiling RawGL in MS VisualStudio is dumb and simple, but for better controling users feedback and bug reports, binaries will be available only through my Patreon:
https://www.patreon.com/3DScan and maybe later through Gumroad.
Premium examples and advanced tutorials as well as tailored support are available from Patreon too.
Feel free to follow me there.

Known bugs and limitations
==========================
* Windows registry required to change Video Driver delays from default settings (30~60 sec). Without this RawGL can silently drop processing or just crashing
Please use gpu_delays.reg from tools/registry folder to change the required registry values. And restart the system before using RawGL.
* No support for atomic counters and buffers yet (highest in priority in TODO list)
* Compute shaders are less tested and probably missed a lot of features
* No support for uniform arrays

Documentation
=============

**RawGL.exe**
|    Options       |        Definition        |
| -------------    | ------------------------ |
| -h [ --help ]    | Show help message        |
| -v [ --version ] | Show program version     |
| |
| -V [ --verbosity ] arg | Log level (selection & above will be shown): |
| | 0 - fatal error only |
| |      1 - errors only |
| |    2 - warnings only |
| |             **3 - info (default)** |
| |            4 - debug |
| |            5 - trace |
| |
| -P [ --pass_vertfrag ] arg | New pass using vertex & fragment shaders (in GLSL or SPIR-V format): |
| | --pass_vertfrag s.vert s.frag |
| | --pass_vertfrag s.vertfrag |
| | (sources separated with macros RAWGL_VERTEX_SHADER, RAWGL_FRAGMENT_SHADER) |
| | --pass_vertfrag s.vert_spv s.frag_spv |
| |  (SPIR-V binary shaders) |
| |
| --bg_color arg | Optional. Define background color (OpenGL clear color) in RGBA |
| | --bg_color 0.5 0.5 0.5 1.0 |
| | default background color: RGBA(0.0, 0.0, 0.0, 0.0) |
| | --bg_color 0.5 - will be parsed as RGBA(0.5, 0.0, 0.0, 1.0) |
| | --bg_color 0.5 0.5 - as RGBA(0.5, 0.5, 0.0, 1.0) | 
| | at least one arg is mandatory, more than 4 will end with error. |
| |
| -M [ --pass_mesh ] arg | Use default quad or external Mesh from file |
| | --pass_mesh quad |
| | --pass_mesh mesh tris true rend tr path\to\file.ply |
| | **tris:** |
| | **true (default)** - inform PLY parser that mesh is triangles only (faster loading on big meshes) |
| | false - arbitrary meshes. |
| | In both cases mesh will be triangulated in render time. |
| | **rend:** |
| | **tr (default)** - GL_TRIANGLES: render as polygons |
| | ln - GL_LINES: render as lines |
| | pt - GL_POINTS - render as a point cloud |
| |
| -C [ --pass_comp ] arg | New pass using a compute shader: |
| |  --pass_comp s.comp |
| |
| -S [ --pass_size ] arg | Output size of this pass (default 512x512px): |
| |  --pass_size X [Y] |
| |  X and Y can also reference the size of an input texture from any pass: |
| |  --pass_size Texture0::0 [Texture1::1] |
| |
| -W [ --pass_workgroupsize ] arg | Number of threads per work group in compute shader on each axis:
| |  --pass_workgroupsize X [Y] |
| |  **Must be equal to the 'local_size' layout constant inside compute shader.** |
| |
| -i [ --in ] arg | Uniform pass index, name & value (numeric or texture path) |
| | (e.g.: --in Texture0 BasicTex.png). |
| |  as output from #-pass: --in outTexture::0 **<- Changed in this version!** |
| |  **Texture filtering and sampling:** |
| |  **min - Texture minification function:** |
| |  **l - GL_LINEAR (default)** |
| |  n - GL_NEAREST |
| |  ll - GL_LINEAR_MIPMAP_LINEAR |
| |  ln - GL_LINEAR_MIPMAP_NEAREST |
| |  nl - GL_NEAREST_MIPMAP_LINEAR |
| |  nn - GL_NEAREST_MIPMAP_NEAREST |
| |  **mag - Texture magnification function:** |
| |  **l - GL_LINEAR (default)** |
| |  n -GL_NEAREST |
| |  **wrps - Texture wrap s-axis:** |
| |  **ce - GL_CLAMP_TO_EDGE (default)** |
| |  r -GL_REPEAT |
| |  cb - GL_CLAMP_TO_BORDER |
| |  mr - GL_MIRRORED_REPEAT) |
| |  mce - GL_MIRROR_CLAMP_TO_EDGE |
| |  **wrpt - Texture wrap t-axis:** |
| |  **ce - GL_CLAMP_TO_EDGE (default)** |
| |  r - GL_REPEAT |
| |  cb - GL_CLAMP_TO_BORDER | 
| |  mr - GL_MIRRORED_REPEAT) |
| |  mce - GL_MIRROR_CLAMP_TO_EDGE |
| | **Uniform can be float, integer, boolean, vec/ivec/bvec 2/3/4,** |
| | **matrices 2x2, 2x3, ... and 4x4** |  
| | --in UniformBoolName True |
| | --in UniformFloatName 0.1524654 |
| | --in UniformVec3 0.15 0.25 0.165 |
| | --in UniformMatx 0.1 0.2 0.3  0.4 0.5 0.6  0.7 0.8 0.9 |
| |
| -t [ --in_attr ] arg | OpenImageIO/plugin attribute value |
| | (e.g.: --in_attr oiio:colorspace sRGB). |
| | **Some used defaults** |
| | oiio:ColorSpace Linear |
| | raw:colorSpace  raw |
| | raw:demosaic AAHD |
| | raw:user_flip -1 |
| | raw:use_camera_wb 0 |
| |
| -o [ --out ] arg | Shader output channel pass, name & path to save the processed image |
| | (e.g.: --out OutColor output.jpg) |
| |  Must have a file name and extension, the extension will determine the ability to save the file with the necessary options. |
| | The file will be recognized as: |
| | BMP:      \*.bmp |
| | PNG:      \*.png |
| | JPEG:     \*.jpg, \*.jpe, \*.jpeg, \*.jif, \*.jfif, \*.jfi |
| | Targa:    \*.tga, \*.tpic |
| | OpenEXR:  \*.exr |
| | HDR/RGBE: \*.hdr |
| | TIFF:     \*.tif, \*.tiff, \*.tx, \*.env, \*.sm, \*.vsm |
| |
| -f [ --out_format ] arg | Output framebuffer format (default rgba32f): |
| | r8, rg8, rgb8, rgba8, |
| | r16, rg16, rgb16, rgba16, |
| | r16f, rg16f, rgb16f, rgba16f, |
| | r32f, rg32f, rgb32f, rgba32f |
| | If format not support this bit depth OpenImageIO automatically convert to supported bit depth. |
| |
| -r [ --out_attr ] arg | OpenImageIO/plugin attribute value |
| | (e.g.: --out_attr oiio:colorspace sRGB). |
| |
| -n [ --out_channels ] arg | # of channels in output image |
| |
| -a [ --out_alpha_channel ] arg | Alpha channel index hint for output image (-1 = off, 0-3 = RGBA). |
| |
| -b [ --out_bits ] arg |  # of bits per output image channel |
| | (depends on file format): |
| |  BMP:      8 |
| | PNG:      8, 16 |
| | JPEG:     8 |
| | Targa:    8, 16 |
| | OpenEXR:  16, 32 (half & float) |
| | HDR/RGBE: 32 |
| | TIFF:     8, 16, 32 float |

Examples
-------------

    RawGL.exe" -V 3 ^
    -P empty.vert EmptyLUT.frag ^
    --pass_size 512 ^
    --in img_size 512 ^
    --in lut_size 8 ^
    --out EmptyLUT EmptyLUT.png ^
    --out_format rgb16 ^
    --out_channels 3 ^
    --out_bits 16 ^
    --out_attr oiio:ColorSpace linear ^
    --out_attr oiio:RawColor 1 ^
    --out_attr oiio:nchannels 3 ^
    --out_attr oiio:UnassociatedAlpha 2 ^
    --out_attr png:compressionLevel 0

This command run **EmptyLUT shade**r with **Image Buffer** size **512x512px** and **generate EmptyLUT.png** GPULut texture 8x8x8 size.
If you want 1024x1024px GPULut size with 16x16x16 3D Lut you can change pass size to 1024, set Uniform img_size to 1024 and Uniform lut_size to 16.
Technically you can use any size of Image buffer and img/lut sizes. For example, this can be 1024x1024 buffer size but 512x512 and 8x8x8 LUT that will make 2x px upscaled image.


    RawGL.exe" ^
    -V 5 ^
    -P shaders\empty.vert shaders\pass1.frag ^
    --pass_size 1024 ^
    --in InSample inputs\EmptyPresetLUT.png ^
    --out OutSample outputs\pass1.tif ^
    --out_format rgb32f --out_channels 3 --out_bits 32 ^
    --out_attr oiio:ColorSpace linear ^
    --out_attr oiio:RawColor 1 ^
    --out_attr oiio:nchannels 3 ^
    --out_attr oiio:UnassociatedAlpha 2 ^
    --out_attr tiff:compression ZIP ^
    -P shaders\empty.vert shaders\pass2.frag ^
    --in InSample2 OutSample::0 ^
    --out OutSample2 outputs\pass2.tif ^
    --out_format rgb32f --out_channels 3 --out_bits 32 ^
    --out_attr oiio:ColorSpace linear ^
    --out_attr oiio:RawColor 1 ^
    --out_attr oiio:nchannels 3 ^
    --out_attr oiio:UnassociatedAlpha 2 ^
    --out_attr tiff:compression ZIP ^
    -P shaders\empty.vert shaders\pass3.frag ^
    --in _LOD 5 ^
    --in InSample3 OutSample2::1 min ll ^
    --out OutSample3 outputs\pass3.tif ^
    --out_format rgb32f --out_channels 3 --out_bits 32 ^
    --out_attr oiio:ColorSpace linear ^
    --out_attr oiio:RawColor 1 ^
    --out_attr oiio:nchannels 3 ^
    --out_attr oiio:UnassociatedAlpha 2 ^
    --out_attr tiff:compression ZIP
   
In this example RawGL will load image **EmptyPresetLUT.png** as **InSample** Uniform and process it in first **pass1** shader. Results of this pass will be saved as 32bit float **pass1.tif**. Pass #2 will use **Output** from pass #1 as **InSample2** Uniform using **OutSample 0** directive and results from this pass will be saved as **pass2.tif**. Pass #3 will use **Output** from pass #2 as **InSample3** Uniform using **OutSample2 1** directive, compute mip-maps (**min ll** directive to make mimp-maps using linear interpolation) and after shader pass save output as pass3.tif.

    RawGL.exe ^
    -V 5 ^
    -P color.vert color.frag ^
    --pass_size 2048 2048 ^
    -M mesh tris true rend pt Shell.ply ^
    --bg_color 0.1 0.4 0.6 ^
    --in inTexture "s:\3D\MATCAPS\coldsteel.png" ^
    --out outColor render.jpg ^
    --out_format rgba16 ^
    --out_channels 4 ^
    --out_bits 16 ^
    --out_attr oiio:ColorSpace linear ^
    --out_attr oiio:RawColor 1 ^
    --out_attr oiio:nchannels 4 ^
    --out_attr compression jpeg:100

More examples available in **Examples** folder. And some more examples can be found in **Tests** folder. 

License
-------

Copyright © 2022 Erium Vladlen.

RawGL is licensed under the GNU General Public License, Version 3.
Individual files may have a different, but compatible license.
