// SPDX-License-Identifier: Apache-2.0

#pragma once

#ifndef MESH_IO_H
#    define MESH_IO_H

#    include <cstdint>
#    include <cstring>

enum class Topology {
    Soup,
    Strip,
    Fan,
};

struct TriMesh {
    float* pos           = nullptr;
    float* normal        = nullptr;
    float* uv            = nullptr;
    unsigned char* color = nullptr;
    uint32_t numVerts    = 0;

    uint32_t* indices   = nullptr;
    uint32_t numIndices = 0;

    Topology topology  = Topology::Soup;
    bool hasTerminator = false;
    int terminator     = -1;

    ~TriMesh();

    bool all_indices_valid() const;
};

TriMesh*
parse_file_with_miniply(const char* filename, bool assumeTriangles);
TriMesh*
parse_mesh_file(const char* filename, bool assumeTriangles);
bool
has_extension(const char* filename, const char* ext);

#endif
