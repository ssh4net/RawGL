"..\..\bin\RawGL\RawGL.exe" ^
--verbosity 3 ^
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

pause