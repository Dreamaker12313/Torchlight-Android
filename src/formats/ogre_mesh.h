// OGRE's binary mesh (`.mesh`, MeshSerializer v1.40 as every Torchlight mesh is, either byte order): the
// submeshes' raw material names, their indices and the vertex positions. What collision reads of a mesh;
// nothing of skinning, levels of detail or animation. Byte span in, struct out.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tl::formats {

/// One vertex set: `positions` holds x, y, z per vertex, in the file's float values.
struct OgreMeshVertices {
    bool present = false;
    uint32_t count = 0;
    std::vector<float> positions;
};

struct OgreMeshSubmesh {
    /// The material name as the file spells it (what `MeshSerializerListener::processMaterialName` sees).
    std::string material;
    bool uses_shared_vertices = true;
    bool indices_32bit = false;
    std::vector<uint32_t> indices;
    /// Its own vertices when it does not use the shared ones.
    OgreMeshVertices vertices;
};

struct OgreMesh {
    bool ok = false;
    std::string error;
    std::string version;  ///< `[MeshSerializer_v1.40]`
    bool big_endian = false;
    OgreMeshVertices shared;
    std::vector<OgreMeshSubmesh> submeshes;
};

OgreMesh parse_ogre_mesh(const uint8_t* data, size_t size);

}  // namespace tl::formats
