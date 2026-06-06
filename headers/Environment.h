#pragma once

#include "Mesh.h"
#include "ModelLoader.h"
#include "Shader.h"
#include "Texture2D.h"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

struct Material {
    glm::vec3 baseColor{1.0f};
    glm::vec3 emissive{0.0f};
    float roughness{0.75f};
    float checkerStrength{0.0f};
    float fogAmount{1.0f};
    std::shared_ptr<Texture2D> texture;
    float opacity{1.0f};
};

struct PointLight {
    glm::vec3 position{0.0f};
    glm::vec3 color{1.0f};
    float intensity{1.0f};
    float radius{12.0f};
};

struct Bounds {
    glm::vec3 center{0.0f};
    glm::vec3 halfExtent{0.5f};
};

enum class GameplaySurface {
    Static,
    MovingCandidate,
    Hazard,
    Decoration
};

struct SceneObject {
    std::string name;
    MeshType mesh{MeshType::Cube};
    Material material;
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
    bool lava{false};
    bool collidable{true};
    GameplaySurface surface{GameplaySurface::Static};
    Bounds localBounds{};
};

struct ImportedLevelPart {
    std::string name;
    Mesh mesh;
    Material material;
    Bounds sourceBounds{};
};

class Environment {
public:
    void create(const std::string& levelDirectory = "assets/Mundos/FreezeezyPeak", bool loadImportedTextures = true);
    void render(const Shader& sceneShader, const Shader& lavaShader, float timeSeconds, const glm::vec3& cameraPosition) const;

    const std::vector<PointLight>& lights() const { return m_lights; }
    const std::vector<Bounds>& collisionPreview() const { return m_collisionPreview; }
    const glm::vec3& recommendedSpawnPoint() const { return m_recommendedSpawn; }
    const glm::vec3& worldMin() const { return m_worldMin; }
    const glm::vec3& worldMax() const { return m_worldMax; }
    const std::string& levelSource() const { return m_levelDirectory; }

private:
    void createMaterials();
    void createMeshes();
    void buildCastle();
    bool loadReferenceLevel();
    void renderReferenceLevel(const Shader& sceneShader) const;
    std::shared_ptr<Texture2D> textureForPath(const std::string& path);

    void addObject(const SceneObject& object);
    void addBlockWall(const glm::vec3& start, int columns, int rows, const glm::vec3& blockSize, const Material& material, const std::string& prefix);
    void addBridge(const glm::vec3& start, int segments, float spacing, const Material& material, const std::string& prefix);
    void addStaircase(const glm::vec3& start, int steps, float direction, const Material& material, const std::string& prefix);
    void addTorch(const glm::vec3& position, const std::string& name);
    void addColumnPair(float z, float xDistance, const std::string& prefix);

    glm::mat4 modelMatrix(const SceneObject& object) const;
    Bounds worldBounds(const SceneObject& object) const;
    Bounds transformedLevelBounds(const glm::vec3& minBounds, const glm::vec3& maxBounds) const;
    bool shouldRenderBounds(const Bounds& bounds, const glm::vec3& cameraPosition, float padding) const;

    std::vector<SceneObject> m_objects;
    std::vector<ImportedLevelPart> m_levelParts;
    std::vector<PointLight> m_lights;
    std::vector<Bounds> m_collisionPreview;
    std::vector<std::shared_ptr<Texture2D>> m_loadedTextures;
    std::string m_levelDirectory;
    bool m_loadImportedTextures{true};
    glm::mat4 m_levelTransform{1.0f};
    glm::vec3 m_levelMin{0.0f};
    glm::vec3 m_levelMax{0.0f};
    glm::vec3 m_worldMin{0.0f};
    glm::vec3 m_worldMax{0.0f};
    glm::vec3 m_recommendedSpawn{0.0f, 6.0f, 0.0f};
    bool m_hasReferenceLevel{false};

    Mesh m_cube;
    Mesh m_plane;
    Mesh m_cylinder;
    Mesh m_cone;
    Mesh m_ramp;

    Material m_stone;
    Material m_darkStone;
    Material m_trim;
    Material m_block;
    Material m_bridge;
    Material m_lava;
    Material m_flame;
    Material m_metal;
    Material m_volcanic;
};
