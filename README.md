# RawGL
Command line image processing tool using OpenGL GLSL shaders.
 
### Features


**Image import/export use OpenimageIO**\
Support all image file formats that support OpenimageIO:
* Camera RAWs (oiio compiled with libraw plugin)
* OpenEXR
* TIFF
* PNG
* JPEG
* JPEG-2000 (oiio compiled with openjpeg plugin)
* etc.

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

**For this moment support vertex, fragment and compute shaders in text and binary SPIR-V form**\
* two files: shader.vert shader.frag
* single file: shader.vertfrag
* binary: shader.vert_spv shader.frag_spv\

*For prototyping or in-house use text-based shaders are easier to manage, adapt and use without any loss of speed. But in case of possible distribution, some can prefer SPIR-V binary format shaders. They do not provide too much security and can be decompiled, but decompiling them can violate the license, and can be a useful choice if you have not planned to distribute your shaders as open source.*

As a tool for image processing in mind **RawGL** for this moment supports (hardcoded) single quad and only isometric camera.

**RawGL support multi-passes shaders**.\
That allows the run process from input images, export results as files to disk as well as pass this result to second and other shader passes.\

*For example, it is possible to create multiple shaders to open camera RAW images, do a custom demosaic in the first shader, apply color correction and/or some image processings like denoise or sharpen in the next shader, and export results to disk, in same moment whithout leaving GPU VRAM pass this results to another shader or shaders that will compile normals or separate specular components from photometric stereo capture inputs. Or import spherical panoramas, decompose them into linear projections, and pass these projections to image processing pass for preprocessing and export to disk.*
