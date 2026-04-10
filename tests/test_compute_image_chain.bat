"..\bin\RawGL\RawGL.exe" ^
--verbosity 5 ^
--pass_comp shaders\image_chain_source.comp ^
--pass_size 1 1 ^
--pass_workgroupsize 1 1 ^
--pass_comp shaders\image_chain_consume.comp ^
--pass_size 1 1 ^
--pass_workgroupsize 1 1 ^
--in u_mid0 o_mid0::0 ^
--out o_out0 outputs\compute_image_chain.exr ^
--out_format rgba32f ^
--out_channels 4 ^
--out_alpha_channel 3 ^
--out_bits 32

if not exist outputs\compute_image_chain.exr exit /b 1

pause
