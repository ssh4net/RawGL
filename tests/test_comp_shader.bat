"..\bin\RawGL\RawGL.exe" ^
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

pause