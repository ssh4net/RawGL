"..\bin\RawGL\RawGL.exe" ^
--verbosity 5 ^
--pass_comp shaders\atomic_counter.comp ^
--pass_size 1 1 ^
--pass_workgroupsize 1 1 ^
--atomic cntr counter0 5 ^
--out o_out0 outputs\atomic_counter.exr ^
--out_format rgba32f ^
--out_channels 4 ^
--out_alpha_channel 3 ^
--out_bits 16

if not exist outputs\atomic_counter.exr exit /b 1

pause
