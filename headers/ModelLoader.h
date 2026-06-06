#pragma once

#include "Mesh.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

struct LoadedMesh {
    std::string name;
    Mesh mesh;
    unsigned int materialIndex{0};
    glm::vec3 minBounds{0.0f};
    glm::vec3 maxBounds{0.0f};
    struct CollisionBox {
        glm::vec3 minBounds{0.0f};
        glm::vec3 maxBounds{0.0f};
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        glm::vec3 a{0.0f};
        glm::vec3 b{0.0f};
        glm::vec3 c{0.0f};
    };
    std::vector<CollisionBox> collisionBoxes;
};

struct LoadedMaterial {
    std::string name;
    glm::vec3 diffuseColor{1.0f};
    float opacity{1.0f};
    std::string diffuseTexturePath;
    std::vector<std::uint8_t> embeddedTextureData;
    int embeddedTextureWidth{0};
    int embeddedTextureHeight{0};
    bool embeddedTextureCompressed{false};
};

struct LoadedModel {
    std::vector<LoadedMesh> meshes;
    std::vector<LoadedMaterial> materials;
    glm::vec3 minBounds{0.0f};
    glm::vec3 maxBounds{0.0f};
    std::string sourcePath;
};

class ModelLoader {
public:
    static LoadedModel loadModel(const std::string& path);
};
