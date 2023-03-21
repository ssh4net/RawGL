#include "mesh_io.h"
#include "miniply.h"

#include <stdio.h>

TriMesh::~TriMesh() {
    delete[] pos;
    delete[] normal;
    delete[] uv;
    delete[] color;
    delete[] indices;
};

bool TriMesh::all_indices_valid() const {
    bool checkTerminator = topology != Topology::Soup && hasTerminator && (terminator < 0 || terminator >= int(numVerts));
    for (uint32_t i = 0; i < numIndices; i++) {
        if (checkTerminator && indices[i] == terminator) {
            continue;
        }
        if (indices[i] < 0 || uint32_t(indices[i]) >= numVerts) {
            return false;
        }
    }
    return true;
};


TriMesh* parse_file_with_miniply(const char* filename, bool assumeTriangles)
{
    miniply::PLYReader reader(filename);
    if (!reader.valid()) {
        return nullptr;
    }

    uint32_t faceIdxs[3];
    if (assumeTriangles) {
        miniply::PLYElement* faceElem = reader.get_element(reader.find_element(miniply::kPLYFaceElement));
        if (faceElem == nullptr) {
            return nullptr;
        }
        assumeTriangles = faceElem->convert_list_to_fixed_size(faceElem->find_property("vertex_indices"), 3, faceIdxs);
    }

    uint32_t propIdxs[4];
    bool gotVerts = false, gotFaces = false;

    TriMesh* trimesh = new TriMesh();
    while (reader.has_element() && (!gotVerts || !gotFaces)) {
        if (reader.element_is(miniply::kPLYVertexElement)) {
            if (!reader.load_element() || !reader.find_pos(propIdxs)) {
                break;
            }
            trimesh->numVerts = reader.num_rows();
            trimesh->pos = new float[trimesh->numVerts * 3];
            reader.extract_properties(propIdxs, 3, miniply::PLYPropertyType::Float, trimesh->pos);
            if (reader.find_normal(propIdxs)) {
                trimesh->normal = new float[trimesh->numVerts * 3];
                reader.extract_properties(propIdxs, 3, miniply::PLYPropertyType::Float, trimesh->normal);
            }
            if (reader.find_texcoord(propIdxs)) {
                trimesh->uv = new float[trimesh->numVerts * 2];
                reader.extract_properties(propIdxs, 2, miniply::PLYPropertyType::Float, trimesh->uv);
            }
            if (reader.find_color_rgba(propIdxs)) {
                trimesh->color = new unsigned char[trimesh->numVerts * 4];
                reader.extract_properties(propIdxs, 4, miniply::PLYPropertyType::UChar, trimesh->color);
            }
            gotVerts = true;
        }
        else if (!gotFaces && reader.element_is(miniply::kPLYFaceElement)) {
            if (!reader.load_element()) {
                break;
            }
            if (assumeTriangles) {
                trimesh->numIndices = reader.num_rows() * 3;
                trimesh->indices = new uint32_t[trimesh->numIndices];
                reader.extract_properties(faceIdxs, 3, miniply::PLYPropertyType::Int, trimesh->indices);
            }
            else {
                uint32_t propIdx;
                if (!reader.find_indices(&propIdx)) {
                    break;
                }
                bool polys = reader.requires_triangulation(propIdx);
                if (polys && !gotVerts) {
                    fprintf(stderr, "Error: face data needing triangulation found before vertex data.\n");
                    break;
                }
                if (polys) {
                    trimesh->numIndices = reader.num_triangles(propIdx) * 3;
                    trimesh->indices = new uint32_t[trimesh->numIndices];
                    reader.extract_triangles(propIdx, trimesh->pos, trimesh->numVerts, miniply::PLYPropertyType::Int, trimesh->indices);
                }
                else {
                    trimesh->numIndices = reader.num_rows() * 3;
                    trimesh->indices = new uint32_t[trimesh->numIndices];
                    reader.extract_list_property(propIdx, miniply::PLYPropertyType::Int, trimesh->indices);
                }
            }
            gotFaces = true;
        }
        else if (!gotFaces && reader.element_is("tristrips")) {
            if (!reader.load_element()) {
                fprintf(stderr, "Error: failed to load tri strips.\n");
                break;
            }
            uint32_t propIdx = reader.element()->find_property("vertex_indices");
            if (propIdx == miniply::kInvalidIndex) {
                fprintf(stderr, "Error: couldn't find 'vertex_indices' property for the 'tristrips' element.\n");
                break;
            }

            trimesh->numIndices = reader.sum_of_list_counts(propIdx);
            trimesh->indices = new uint32_t[trimesh->numIndices];
            trimesh->topology = Topology::Strip;
            trimesh->hasTerminator = true;
            trimesh->terminator = -1;
            reader.extract_list_property(propIdx, miniply::PLYPropertyType::Int, trimesh->indices);

            gotFaces = true;
        }
        reader.next_element();
    }

    if (!gotVerts || !gotFaces || !trimesh->all_indices_valid()) {
        delete trimesh;
        return nullptr;
    }

    return trimesh;
}


bool has_extension(const char* filename, const char* ext)
{
    int j = int(strlen(ext));
    int i = int(strlen(filename)) - j;
    if (i <= 0 || filename[i - 1] != '.') {
        return false;
    }
    return strcmp(filename + i, ext) == 0;
}
