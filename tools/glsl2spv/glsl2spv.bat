@glslangValidator -G100 %1.vert -o %1.vert_spv
@glslangValidator -G100 %1.frag -o %1.frag_spv

@rem glslangValidator -G100 %1.vertfrag -o %1.vert_spv -S vert --D RAWGL_VERTEX_SHADER
@rem glslangValidator -G100 %1.vertfrag -o %1.frag_spv -S frag --D RAWGL_FRAGMENT_SHADER
