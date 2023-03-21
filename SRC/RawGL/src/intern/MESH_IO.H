#pragma once

#ifndef MESH_IO_H
#define MESH_IO_H

#include <cstdint>
#include <cstring>

enum class Topology {
    Soup,
    Strip,
    Fan,
};

//
// TriMesh type
//

// It's a triangle mesh, so any faces with more than three verts will
// get triangulated.
//
// The structure can hold individual triangles, triangle strips or triangle
// fans (pick one). If it's strips or fans, you can use an optional
// terminator value to indicate where one strip/fan ends and a new one begins.

struct TriMesh {
    float* pos = nullptr;
    float* normal = nullptr;
    float* uv = nullptr;
    unsigned char* color = nullptr;
    uint32_t numVerts = 0;

    uint32_t* indices = nullptr;
    uint32_t numIndices = 0;

    Topology topology = Topology::Soup;
    bool hasTerminator = false;
    int terminator = -1;

    ~TriMesh();

    bool all_indices_valid() const;
};

TriMesh* parse_file_with_miniply(const char* filename, bool assumeTriangles);
bool has_extension(const char* filename, const char* ext);

#endif // MESH_IO_H
