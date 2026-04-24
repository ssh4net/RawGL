// SPDX-License-Identifier: Apache-2.0

#include "mesh_io.h"
#include <rawgl/rawgl_core.h>
#include <stdio.h>
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <string>
#include <vector>

#if !defined(RAWGL_DISABLE_MINIPLY)
#    include "miniply.h"
#endif

#if defined(RAWGL_HAS_RAPIDOBJ)
#    include <rapidobj/rapidobj.hpp>
#endif

namespace {

static bool
has_extension_case_insensitive(const char* filename, const char* ext)
{
    int j = int(strlen(ext));
    int i = int(strlen(filename)) - j;
    if (i <= 0 || filename[i - 1] != '.') {
        return false;
    }

    for (int k = 0; k < j; ++k) {
        char a = filename[i + k];
        char b = ext[k];
        if (a >= 'A' && a <= 'Z') {
            a = static_cast<char>(a - 'A' + 'a');
        }
        if (b >= 'A' && b <= 'Z') {
            b = static_cast<char>(b - 'A' + 'a');
        }
        if (a != b) {
            return false;
        }
    }

    return true;
}

struct ObjMaterialName {
    std::string name;
    uint32_t id = 0u;
};

static uint32_t
resolve_obj_material_name_id(std::vector<ObjMaterialName>& materialNames, const std::string& name)
{
    if (name.empty()) {
        return 0u;
    }

    for (const ObjMaterialName& materialName : materialNames) {
        if (materialName.name == name) {
            return materialName.id;
        }
    }

    const uint32_t id = static_cast<uint32_t>(materialNames.size() + 1u);
    materialNames.push_back(ObjMaterialName { name, id });
    return id;
}

static std::string
trim_ascii_copy(const std::string& value)
{
    size_t begin = 0u;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1u])) != 0) {
        --end;
    }

    return value.substr(begin, end - begin);
}

static bool
find_obj_line_payload(const std::string& line, const char* keyword, size_t& payloadOffset)
{
    size_t cursor = 0u;
    while (cursor < line.size() && std::isspace(static_cast<unsigned char>(line[cursor])) != 0) {
        ++cursor;
    }

    const size_t keywordLength = std::strlen(keyword);
    if (cursor + keywordLength > line.size() || line.compare(cursor, keywordLength, keyword) != 0) {
        return false;
    }

    const size_t afterKeyword = cursor + keywordLength;
    if (afterKeyword < line.size()
        && std::isspace(static_cast<unsigned char>(line[afterKeyword])) == 0) {
        return false;
    }

    payloadOffset = afterKeyword;
    while (payloadOffset < line.size()
           && std::isspace(static_cast<unsigned char>(line[payloadOffset])) != 0) {
        ++payloadOffset;
    }

    return true;
}

static size_t
parse_obj_float_values(const std::string& line, const size_t payloadOffset, float* values, const size_t maxValues)
{
    size_t parsedCount = 0u;
    const char* cursor = line.c_str() + payloadOffset;
    while (parsedCount < maxValues) {
        while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)) != 0) {
            ++cursor;
        }
        if (*cursor == '\0' || *cursor == '#') {
            break;
        }

        char* end = nullptr;
        const float value = std::strtof(cursor, &end);
        if (end == cursor) {
            break;
        }

        values[parsedCount] = value;
        ++parsedCount;
        cursor = end;
    }

    return parsedCount;
}

static size_t
count_obj_face_vertices(const std::string& line, const size_t payloadOffset)
{
    size_t count = 0u;
    bool inToken = false;
    for (size_t i = payloadOffset; i < line.size(); ++i) {
        if (line[i] == '#') {
            break;
        }

        const bool isSpace = std::isspace(static_cast<unsigned char>(line[i])) != 0;
        if (isSpace) {
            inToken = false;
            continue;
        }
        if (!inToken) {
            ++count;
            inToken = true;
        }
    }

    return count;
}

#if defined(RAWGL_HAS_RAPIDOBJ)
struct ObjVertexKey {
    int positionIndex = -1;
    int texcoordIndex = -1;
    int normalIndex = -1;
    uint32_t materialIndex = 0u;

    bool
    operator<(const ObjVertexKey& other) const noexcept
    {
        if (positionIndex != other.positionIndex) {
            return positionIndex < other.positionIndex;
        }
        if (texcoordIndex != other.texcoordIndex) {
            return texcoordIndex < other.texcoordIndex;
        }
        if (normalIndex != other.normalIndex) {
            return normalIndex < other.normalIndex;
        }
        return materialIndex < other.materialIndex;
    }
};

struct ObjMeshBuildData {
    std::vector<float> positions;
    std::vector<float> texcoords;
    std::vector<float> normals;
    std::vector<unsigned char> colors;
    std::vector<uint32_t> materialIds;
    std::vector<uint32_t> indices;
    std::map<ObjVertexKey, uint32_t> uniqueVertices;
};

static bool
read_obj_material_ids_from_usemtl(const char* filename, std::vector<uint32_t>& materialIds)
{
    std::ifstream stream(filename);
    if (!stream) {
        fprintf(stderr, "OBJ mesh load failed: can't open file.\n");
        return false;
    }

    std::vector<ObjMaterialName> materialNames;
    std::string currentMaterialName;
    std::string line;
    while (std::getline(stream, line)) {
        size_t payloadOffset = 0u;
        if (find_obj_line_payload(line, "usemtl", payloadOffset)) {
            currentMaterialName = trim_ascii_copy(line.substr(payloadOffset));
            continue;
        }
        if (find_obj_line_payload(line, "f", payloadOffset)) {
            materialIds.push_back(resolve_obj_material_name_id(materialNames, currentMaterialName));
        }
    }

    return true;
}

static bool
append_obj_vertex(const rapidobj::Attributes& attributes,
                  const rapidobj::Index& index,
                  const uint32_t materialIndex,
                  ObjMeshBuildData& buildData)
{
    if (index.position_index < 0) {
        return false;
    }

    const size_t positionOffset = static_cast<size_t>(index.position_index) * 3u;
    if (positionOffset + 2u >= attributes.positions.size()) {
        return false;
    }

    const ObjVertexKey key { index.position_index, index.texcoord_index, index.normal_index, materialIndex };
    const std::map<ObjVertexKey, uint32_t>::const_iterator existing = buildData.uniqueVertices.find(key);
    if (existing != buildData.uniqueVertices.end()) {
        buildData.indices.push_back(existing->second);
        return true;
    }

    const uint32_t vertexIndex = static_cast<uint32_t>(buildData.positions.size() / 3u);
    buildData.uniqueVertices.insert({ key, vertexIndex });

    buildData.positions.push_back(attributes.positions[positionOffset + 0u]);
    buildData.positions.push_back(attributes.positions[positionOffset + 1u]);
    buildData.positions.push_back(attributes.positions[positionOffset + 2u]);

    if (index.texcoord_index >= 0) {
        const size_t texcoordOffset = static_cast<size_t>(index.texcoord_index) * 2u;
        if (texcoordOffset + 1u >= attributes.texcoords.size()) {
            return false;
        }
        buildData.texcoords.push_back(attributes.texcoords[texcoordOffset + 0u]);
        buildData.texcoords.push_back(attributes.texcoords[texcoordOffset + 1u]);
    } else {
        buildData.texcoords.push_back(0.0f);
        buildData.texcoords.push_back(0.0f);
    }

    if (index.normal_index >= 0) {
        const size_t normalOffset = static_cast<size_t>(index.normal_index) * 3u;
        if (normalOffset + 2u >= attributes.normals.size()) {
            return false;
        }
        buildData.normals.push_back(attributes.normals[normalOffset + 0u]);
        buildData.normals.push_back(attributes.normals[normalOffset + 1u]);
        buildData.normals.push_back(attributes.normals[normalOffset + 2u]);
    } else {
        buildData.normals.push_back(0.0f);
        buildData.normals.push_back(0.0f);
        buildData.normals.push_back(1.0f);
    }

    const size_t colorOffset = static_cast<size_t>(index.position_index) * 3u;
    if (colorOffset + 2u < attributes.colors.size()) {
        for (size_t channel = 0u; channel < 3u; ++channel) {
            const float value = std::clamp(attributes.colors[colorOffset + channel], 0.0f, 1.0f);
            buildData.colors.push_back(static_cast<unsigned char>(value * 255.0f + 0.5f));
        }
        buildData.colors.push_back(255u);
    } else {
        buildData.colors.push_back(255u);
        buildData.colors.push_back(255u);
        buildData.colors.push_back(255u);
        buildData.colors.push_back(255u);
    }

    buildData.materialIds.push_back(materialIndex);
    buildData.indices.push_back(vertexIndex);
    return true;
}

static TriMesh*
build_trimesh_from_obj(const rapidobj::Result& result,
                       const std::vector<uint32_t>& sourceMaterialIds,
                       bool assumeTriangles)
{
    ObjMeshBuildData buildData;

    size_t sourceIndexCount = 0u;
    size_t outputIndexCount = 0u;
    for (const rapidobj::Shape& shape : result.shapes) {
        size_t shapeIndexOffset = 0u;
        for (const uint8_t faceVertexCount : shape.mesh.num_face_vertices) {
            if (faceVertexCount < 3u) {
                fprintf(stderr, "OBJ mesh contains a face with fewer than three vertices.\n");
                return nullptr;
            }
            if (assumeTriangles && faceVertexCount != 3u) {
                fprintf(stderr, "OBJ mesh contains non-triangle faces. Use tris false to triangulate.\n");
                return nullptr;
            }
            shapeIndexOffset += faceVertexCount;
            outputIndexCount += static_cast<size_t>(faceVertexCount - 2u) * 3u;
        }
        if (shapeIndexOffset != shape.mesh.indices.size()) {
            fprintf(stderr, "OBJ mesh has inconsistent face index data.\n");
            return nullptr;
        }
        sourceIndexCount += shapeIndexOffset;
    }

    buildData.positions.reserve(sourceIndexCount * 3u);
    buildData.texcoords.reserve(sourceIndexCount * 2u);
    buildData.normals.reserve(sourceIndexCount * 3u);
    buildData.colors.reserve(sourceIndexCount * 4u);
    buildData.materialIds.reserve(sourceIndexCount);
    buildData.indices.reserve(outputIndexCount);

    size_t globalFaceIndex = 0u;
    for (const rapidobj::Shape& shape : result.shapes) {
        size_t shapeIndexOffset = 0u;
        for (size_t faceIndex = 0u; faceIndex < shape.mesh.num_face_vertices.size(); ++faceIndex) {
            const uint8_t faceVertexCount = shape.mesh.num_face_vertices[faceIndex];
            const rapidobj::Index* faceIndices = shape.mesh.indices.data() + shapeIndexOffset;
            const uint32_t materialIndex =
                globalFaceIndex < sourceMaterialIds.size() ? sourceMaterialIds[globalFaceIndex] : 0u;
            for (uint8_t faceVertex = 1u; faceVertex + 1u < faceVertexCount; ++faceVertex) {
                if (!append_obj_vertex(result.attributes, faceIndices[0u], materialIndex, buildData)
                    || !append_obj_vertex(result.attributes, faceIndices[faceVertex], materialIndex, buildData)
                    || !append_obj_vertex(result.attributes, faceIndices[faceVertex + 1u], materialIndex, buildData)) {
                    fprintf(stderr, "OBJ mesh contains invalid vertex attributes.\n");
                    return nullptr;
                }
            }
            shapeIndexOffset += faceVertexCount;
            ++globalFaceIndex;
        }
    }

    if (buildData.indices.size() != outputIndexCount) {
        fprintf(stderr, "OBJ mesh triangulation produced an inconsistent index count.\n");
        return nullptr;
    }

    if (buildData.positions.empty() || buildData.indices.empty()) {
        return nullptr;
    }
    if (buildData.positions.size() / 3u > std::numeric_limits<uint32_t>::max()
        || buildData.indices.size() > std::numeric_limits<uint32_t>::max()) {
        return nullptr;
    }

    TriMesh* triMesh = new TriMesh();
    triMesh->numVerts = static_cast<uint32_t>(buildData.positions.size() / 3u);
    triMesh->numIndices = static_cast<uint32_t>(buildData.indices.size());
    triMesh->topology = Topology::Soup;
    triMesh->hasTerminator = false;
    triMesh->terminator = -1;

    triMesh->pos = new float[buildData.positions.size()];
    std::memcpy(triMesh->pos, buildData.positions.data(), buildData.positions.size() * sizeof(float));

    triMesh->uv = new float[buildData.texcoords.size()];
    std::memcpy(triMesh->uv, buildData.texcoords.data(), buildData.texcoords.size() * sizeof(float));

    triMesh->normal = new float[buildData.normals.size()];
    std::memcpy(triMesh->normal, buildData.normals.data(), buildData.normals.size() * sizeof(float));

    triMesh->color = new unsigned char[buildData.colors.size()];
    std::memcpy(triMesh->color, buildData.colors.data(), buildData.colors.size() * sizeof(unsigned char));

    triMesh->materialId = new uint32_t[buildData.materialIds.size()];
    std::memcpy(triMesh->materialId, buildData.materialIds.data(), buildData.materialIds.size() * sizeof(uint32_t));

    triMesh->indices = new uint32_t[buildData.indices.size()];
    std::memcpy(triMesh->indices, buildData.indices.data(), buildData.indices.size() * sizeof(uint32_t));

    if (!triMesh->all_indices_valid()) {
        delete triMesh;
        return nullptr;
    }

    return triMesh;
}

static TriMesh*
parse_file_with_rapidobj(const char* filename, bool assumeTriangles)
{
    std::vector<uint32_t> materialIds;
    if (!read_obj_material_ids_from_usemtl(filename, materialIds)) {
        return nullptr;
    }

    std::ifstream stream(filename);
    if (!stream) {
        fprintf(stderr, "OBJ mesh load failed: can't open file.\n");
        return nullptr;
    }

    rapidobj::Result result = rapidobj::ParseStream(stream, rapidobj::MaterialLibrary::Ignore());
    if (result.error) {
        fprintf(stderr, "OBJ mesh load failed: %s\n", result.error.code.message().c_str());
        return nullptr;
    }

    return build_trimesh_from_obj(result, materialIds, assumeTriangles);
}
#endif

}  // namespace

TriMesh::~TriMesh()
{
    delete[] pos;
    delete[] normal;
    delete[] uv;
    delete[] color;
    delete[] materialId;
    delete[] indices;
};

bool
TriMesh::all_indices_valid() const
{
    bool checkTerminator = topology != Topology::Soup && hasTerminator
                           && (terminator < 0 || terminator >= int(numVerts));
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

TriMesh*
parse_file_with_miniply(const char* filename, bool assumeTriangles)
{
#if defined(RAWGL_DISABLE_MINIPLY)
    (void)filename;
    (void)assumeTriangles;
    fprintf(stderr, "PLY mesh support is disabled in this build.\n");
    return nullptr;
#else
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
            trimesh->pos      = new float[trimesh->numVerts * 3];
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
        } else if (!gotFaces && reader.element_is(miniply::kPLYFaceElement)) {
            if (!reader.load_element()) {
                break;
            }
            if (assumeTriangles) {
                trimesh->numIndices = reader.num_rows() * 3;
                trimesh->indices    = new uint32_t[trimesh->numIndices];
                reader.extract_properties(faceIdxs, 3, miniply::PLYPropertyType::Int, trimesh->indices);
            } else {
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
                    trimesh->indices    = new uint32_t[trimesh->numIndices];
                    reader.extract_triangles(propIdx, trimesh->pos, trimesh->numVerts, miniply::PLYPropertyType::Int,
                                             trimesh->indices);
                } else {
                    trimesh->numIndices = reader.num_rows() * 3;
                    trimesh->indices    = new uint32_t[trimesh->numIndices];
                    reader.extract_list_property(propIdx, miniply::PLYPropertyType::Int, trimesh->indices);
                }
            }
            gotFaces = true;
        } else if (!gotFaces && reader.element_is("tristrips")) {
            if (!reader.load_element()) {
                fprintf(stderr, "Error: failed to load tri strips.\n");
                break;
            }
            uint32_t propIdx = reader.element()->find_property("vertex_indices");
            if (propIdx == miniply::kInvalidIndex) {
                fprintf(stderr, "Error: couldn't find 'vertex_indices' property for the 'tristrips' element.\n");
                break;
            }

            trimesh->numIndices    = reader.sum_of_list_counts(propIdx);
            trimesh->indices       = new uint32_t[trimesh->numIndices];
            trimesh->topology      = Topology::Strip;
            trimesh->hasTerminator = true;
            trimesh->terminator    = -1;
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
#endif
}

TriMesh*
parse_mesh_file(const char* filename, bool assumeTriangles)
{
#if defined(RAWGL_HAS_RAPIDOBJ)
    if (has_extension_case_insensitive(filename, "obj")) {
        return parse_file_with_rapidobj(filename, assumeTriangles);
    }
#endif

    return parse_file_with_miniply(filename, assumeTriangles);
}


bool
has_extension(const char* filename, const char* ext)
{
    int j = int(strlen(ext));
    int i = int(strlen(filename)) - j;
    if (i <= 0 || filename[i - 1] != '.') {
        return false;
    }
    return strcmp(filename + i, ext) == 0;
}

namespace rawgl {

MeshInspectionResult
InspectMeshFile(const MeshInspectionRequest& request)
{
    MeshInspectionResult result;
    result.path = request.path;

    if (!has_extension_case_insensitive(request.path.c_str(), "obj")) {
        result.errorMessage = "mesh inspection currently supports OBJ files only";
        return result;
    }

    std::ifstream stream(request.path);
    if (!stream) {
        result.errorMessage = "can't open mesh file";
        return result;
    }

    std::vector<ObjMaterialName> materialNames;
    std::vector<size_t> materialFaceCounts(1u, 0u);
    uint32_t currentMaterialId = 0u;
    std::string currentGroupName;
    size_t activeGroupIndex = std::numeric_limits<size_t>::max();

    std::string line;
    while (std::getline(stream, line)) {
        size_t payloadOffset = 0u;
        if (find_obj_line_payload(line, "v", payloadOffset)) {
            float values[3] = {};
            if (parse_obj_float_values(line, payloadOffset, values, 3u) == 3u) {
                if (!result.hasBounds) {
                    result.boundsMin = { values[0], values[1], values[2] };
                    result.boundsMax = result.boundsMin;
                    result.hasBounds = true;
                } else {
                    for (size_t axis = 0u; axis < 3u; ++axis) {
                        result.boundsMin[axis] = std::min(result.boundsMin[axis], values[axis]);
                        result.boundsMax[axis] = std::max(result.boundsMax[axis], values[axis]);
                    }
                }
            }
            ++result.vertexCount;
            continue;
        }

        if (find_obj_line_payload(line, "vt", payloadOffset)) {
            float values[2] = {};
            if (parse_obj_float_values(line, payloadOffset, values, 2u) == 2u) {
                if (!result.hasUvRange) {
                    result.uvMin = { values[0], values[1] };
                    result.uvMax = result.uvMin;
                    result.hasUvRange = true;
                } else {
                    for (size_t axis = 0u; axis < 2u; ++axis) {
                        result.uvMin[axis] = std::min(result.uvMin[axis], values[axis]);
                        result.uvMax[axis] = std::max(result.uvMax[axis], values[axis]);
                    }
                }
            }
            ++result.texcoordCount;
            continue;
        }

        if (find_obj_line_payload(line, "vn", payloadOffset)) {
            ++result.normalCount;
            continue;
        }

        if (find_obj_line_payload(line, "usemtl", payloadOffset)) {
            const std::string materialName = trim_ascii_copy(line.substr(payloadOffset));
            currentMaterialId = resolve_obj_material_name_id(materialNames, materialName);
            if (currentMaterialId >= materialFaceCounts.size()) {
                materialFaceCounts.resize(static_cast<size_t>(currentMaterialId) + 1u, 0u);
            }
            continue;
        }

        if (find_obj_line_payload(line, "g", payloadOffset) || find_obj_line_payload(line, "o", payloadOffset)) {
            currentGroupName = trim_ascii_copy(line.substr(payloadOffset));
            activeGroupIndex = std::numeric_limits<size_t>::max();
            continue;
        }

        if (!find_obj_line_payload(line, "f", payloadOffset)) {
            continue;
        }

        const size_t faceVertexCount = count_obj_face_vertices(line, payloadOffset);
        if (faceVertexCount == 3u) {
            ++result.triangleFaceCount;
        } else if (faceVertexCount == 4u) {
            ++result.quadFaceCount;
        } else if (faceVertexCount > 4u) {
            ++result.ngonFaceCount;
        }
        if (faceVertexCount >= 3u) {
            result.generatedTriangleCount += faceVertexCount - 2u;
        }

        if (currentMaterialId >= materialFaceCounts.size()) {
            materialFaceCounts.resize(static_cast<size_t>(currentMaterialId) + 1u, 0u);
        }
        ++materialFaceCounts[currentMaterialId];

        if (activeGroupIndex == std::numeric_limits<size_t>::max()
            || result.groups[activeGroupIndex].name != currentGroupName) {
            MeshGroupInfo groupInfo;
            groupInfo.name = currentGroupName;
            groupInfo.firstFaceIndex = result.faceCount;
            result.groups.push_back(groupInfo);
            activeGroupIndex = result.groups.size() - 1u;
        }
        ++result.groups[activeGroupIndex].faceCount;
        ++result.faceCount;
    }

    if (!materialFaceCounts.empty() && materialFaceCounts[0u] > 0u) {
        MeshMaterialInfo materialInfo;
        materialInfo.id = 0u;
        materialInfo.faceCount = materialFaceCounts[0u];
        result.materials.push_back(materialInfo);
    }

    for (const ObjMaterialName& materialName : materialNames) {
        MeshMaterialInfo materialInfo;
        materialInfo.id = materialName.id;
        materialInfo.name = materialName.name;
        const size_t faceCountIndex = static_cast<size_t>(materialName.id);
        if (faceCountIndex < materialFaceCounts.size()) {
            materialInfo.faceCount = materialFaceCounts[faceCountIndex];
        }
        result.materials.push_back(materialInfo);
    }

    result.success = true;
    return result;
}

}  // namespace rawgl
