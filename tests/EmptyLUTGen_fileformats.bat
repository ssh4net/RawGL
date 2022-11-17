"..\bin\RawGL\RawGL.exe" ^
--verbosity 5 ^
-P shaders\empty.vert shaders\EmptyLUT.frag ^
--pass_size 8192 ^
--in img_size 8192 ^
--in lut_size 16 ^
--out EmptyLUT outputs\EmptyLUT.jp2 ^
--out_format rgb16 ^
--out_channels 3 ^
--out_bits 16 ^
--out_attr oiio:ColorSpace linear ^
--out_attr oiio:Compression jpeg:100 ^
--out_attr oiio:RawColor 1 ^
--out_attr oiio:nchannels 3 ^
--out_attr oiio:UnassociatedAlpha 2 ^
--out_attr png:compressionLevel 0

pause