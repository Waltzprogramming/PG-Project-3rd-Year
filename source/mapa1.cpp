#include "Mapa1.h"

#include "Mesh.h"
#include "ModelLoader.h"
#include "Shader.h"
#include "Texture2D.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
constexpr float MapScale = 0.0100f;
const glm::vec3 MapCenter(1336.45f, 0.0f, -200.0f);
constexpr float MinimumWalkableNormal = 0.35f;
constexpr float MinimumWalkableHeight = -1.08f;
constexpr float TriangleMargin = 0.012f;
constexpr float LandingMargin = 0.09f;
constexpr float StepHeightWithoutJump = 0.12f;
constexpr int StartingLives = 3;
constexpr int TotalCoins = 10;
constexpr float DeathHeight = -4.0f;
constexpr float MoveSpeed2D = 1.20f;
constexpr float MoveSpeed3D = 1.28f;
constexpr float JumpSpeed = 4.60f;
constexpr float Gravity = 9.20f;
constexpr float CameraBaseHeight = -0.92f;
constexpr float CameraVerticalTracking = 0.62f;
constexpr float CameraMaximumHeight = 5.25f;
constexpr float CameraSmoothing = 8.0f;

struct TerrainTriangle {
    glm::vec3 a{0.0f};
    glm::vec3 b{0.0f};
    glm::vec3 c{0.0f};
    float minX{0.0f};
    float maxX{0.0f};
    float minZ{0.0f};
    float maxZ{0.0f};
    const std::string* name{nullptr};
};

enum class PlatformShape {
    Rectangle,
    Ellipse
};

struct ExtraPlatform {
    PlatformShape shape{PlatformShape::Rectangle};
    float minX{0.0f};
    float maxX{0.0f};
    float minZ{0.0f};
    float maxZ{0.0f};
    float height{0.0f};
    float thickness{0.24f};
    glm::vec4 color{1.0f};
    const char* name{nullptr};
    bool visible{true};
    bool projectIn2D{false};
};

struct Coin {
    glm::vec3 position{0.0f};
    bool collected{false};
    float phase{0.0f};
    bool projectIn2D{false};
};

struct Star {
    glm::vec3 position{0.0f};
    bool active{false};
    bool projectIn2D{false};
};

struct MapMaterial {
    glm::vec4 color{1.0f};
    std::shared_ptr<Texture2D> texture;
};

struct WorldModel {
    LoadedModel model;
    std::vector<MapMaterial> materials;
};

std::string resolveAssetPath(const std::string& path) {
    const std::filesystem::path requested(path);
    const std::filesystem::path candidates[] = {
        requested,
        std::filesystem::path("..") / ".." / requested
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate.lexically_normal().string();
        }
    }
    return path;
}

Vertex makeVertex(const glm::vec3& position, const glm::vec2& uv) {
    return {position, {0.0f, 0.0f, 1.0f}, uv, glm::vec4(1.0f)};
}

Mesh createPlayerMesh() {
    const std::vector<Vertex> vertices = {
        makeVertex({0.42f, 1.15f, 0.0f}, {1.0f, 1.0f}),
        makeVertex({0.42f, 0.00f, 0.0f}, {1.0f, 0.0f}),
        makeVertex({-0.42f, 0.00f, 0.0f}, {0.0f, 0.0f}),
        makeVertex({-0.42f, 1.15f, 0.0f}, {0.0f, 1.0f})
    };
    const std::vector<unsigned int> indices = {0, 1, 2, 0, 2, 3};

    Mesh mesh;
    mesh.upload(vertices, indices);
    return mesh;
}

Mesh createSkyboxMesh() {
    constexpr float margin = 0.001f;
    constexpr float u0 = 0.00f + margin;
    constexpr float u1 = 0.25f - margin;
    constexpr float u2 = 0.25f + margin;
    constexpr float u3 = 0.50f - margin;
    constexpr float u4 = 0.50f + margin;
    constexpr float u5 = 0.75f - margin;
    constexpr float u6 = 0.75f + margin;
    constexpr float u7 = 1.00f - margin;
    constexpr float vBottom0 = 0.00f + margin;
    constexpr float vBottom1 = (1.0f / 3.0f) - margin;
    constexpr float vMiddle0 = (1.0f / 3.0f) + margin;
    constexpr float vMiddle1 = (2.0f / 3.0f) - margin;
    constexpr float vTop0 = (2.0f / 3.0f) + margin;
    constexpr float vTop1 = 1.00f - margin;

    const std::vector<Vertex> vertices = {
        makeVertex({-1.0f, 1.0f, -1.0f}, {u2, vMiddle1}),
        makeVertex({1.0f, 1.0f, -1.0f}, {u3, vMiddle1}),
        makeVertex({1.0f, -1.0f, -1.0f}, {u3, vMiddle0}),
        makeVertex({-1.0f, -1.0f, -1.0f}, {u2, vMiddle0}),
        makeVertex({1.0f, 1.0f, -1.0f}, {u4, vMiddle1}),
        makeVertex({1.0f, 1.0f, 1.0f}, {u5, vMiddle1}),
        makeVertex({1.0f, -1.0f, 1.0f}, {u5, vMiddle0}),
        makeVertex({1.0f, -1.0f, -1.0f}, {u4, vMiddle0}),
        makeVertex({1.0f, 1.0f, 1.0f}, {u6, vMiddle1}),
        makeVertex({-1.0f, 1.0f, 1.0f}, {u7, vMiddle1}),
        makeVertex({-1.0f, -1.0f, 1.0f}, {u7, vMiddle0}),
        makeVertex({1.0f, -1.0f, 1.0f}, {u6, vMiddle0}),
        makeVertex({-1.0f, 1.0f, 1.0f}, {u0, vMiddle1}),
        makeVertex({-1.0f, 1.0f, -1.0f}, {u1, vMiddle1}),
        makeVertex({-1.0f, -1.0f, -1.0f}, {u1, vMiddle0}),
        makeVertex({-1.0f, -1.0f, 1.0f}, {u0, vMiddle0}),
        makeVertex({-1.0f, 1.0f, 1.0f}, {u2, vTop1}),
        makeVertex({1.0f, 1.0f, 1.0f}, {u3, vTop1}),
        makeVertex({1.0f, 1.0f, -1.0f}, {u3, vTop0}),
        makeVertex({-1.0f, 1.0f, -1.0f}, {u2, vTop0}),
        makeVertex({-1.0f, -1.0f, -1.0f}, {u2, vBottom1}),
        makeVertex({1.0f, -1.0f, -1.0f}, {u3, vBottom1}),
        makeVertex({1.0f, -1.0f, 1.0f}, {u3, vBottom0}),
        makeVertex({-1.0f, -1.0f, 1.0f}, {u2, vBottom0})
    };
    const std::vector<unsigned int> indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        8, 9, 10, 10, 11, 8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };

    Mesh mesh;
    mesh.upload(vertices, indices);
    return mesh;
}

Mesh createStarMesh() {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    constexpr int points = 10;
    constexpr float frontZ = 0.08f;
    constexpr float backZ = -0.08f;

    vertices.push_back({{0.0f, 0.0f, frontZ}, {0.0f, 0.0f, 1.0f}, {0.5f, 0.5f}, glm::vec4(1.0f)});
    vertices.push_back({{0.0f, 0.0f, backZ}, {0.0f, 0.0f, -1.0f}, {0.5f, 0.5f}, glm::vec4(1.0f)});
    for (int i = 0; i < points; ++i) {
        const float radius = (i % 2 == 0) ? 0.72f : 0.33f;
        const float angle = glm::half_pi<float>() + glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(points);
        const float x = std::cos(angle) * radius;
        const float y = std::sin(angle) * radius;
        vertices.push_back({{x, y, frontZ}, {0.0f, 0.0f, 1.0f}, {(x / 1.6f) + 0.5f, (y / 1.6f) + 0.5f}, glm::vec4(1.0f)});
        vertices.push_back({{x, y, backZ}, {0.0f, 0.0f, -1.0f}, {(x / 1.6f) + 0.5f, (y / 1.6f) + 0.5f}, glm::vec4(1.0f)});
    }

    for (int i = 0; i < points; ++i) {
        const unsigned int frontA = 2u + static_cast<unsigned int>(i) * 2u;
        const unsigned int backA = frontA + 1u;
        const unsigned int frontB = 2u + static_cast<unsigned int>((i + 1) % points) * 2u;
        const unsigned int backB = frontB + 1u;
        indices.insert(indices.end(), {0u, frontA, frontB});
        indices.insert(indices.end(), {1u, backB, backA});
        indices.insert(indices.end(), {frontA, backA, backB, frontA, backB, frontB});
    }

    Mesh mesh;
    mesh.upload(vertices, indices);
    return mesh;
}

bool containsText(const std::string& text, const char* pattern) {
    return text.find(pattern) != std::string::npos;
}

bool isWalkableMesh(const std::string& name) {
    if (containsText(name, "Wave") ||
        containsText(name, "SeaSide") ||
        containsText(name, "IslandMat") ||
        containsText(name, "Cloud") ||
        containsText(name, "Shadow") ||
        containsText(name, "GatePole")) {
        return false;
    }

    return containsText(name, "Lawn") ||
        containsText(name, "Grass") ||
        containsText(name, "Flower") ||
        containsText(name, "Road") ||
        containsText(name, "SandMat") ||
        containsText(name, "Bridge") ||
        containsText(name, "Rock") ||
        containsText(name, "WallBlock");
}

glm::vec3 transformMapPoint(const glm::vec3& point) {
    return {
        (point.x + MapCenter.x) * MapScale,
        -1.0f + point.y * MapScale,
        (point.z + MapCenter.z) * MapScale
    };
}

bool heightInTriangle(const TerrainTriangle& triangle, float x, float z, float& height) {
    if (x < triangle.minX - TriangleMargin || x > triangle.maxX + TriangleMargin ||
        z < triangle.minZ - TriangleMargin || z > triangle.maxZ + TriangleMargin) {
        return false;
    }

    const float denominator = (triangle.b.z - triangle.c.z) * (triangle.a.x - triangle.c.x) +
        (triangle.c.x - triangle.b.x) * (triangle.a.z - triangle.c.z);
    if (std::fabs(denominator) < 0.000001f) {
        return false;
    }

    const float weightA = ((triangle.b.z - triangle.c.z) * (x - triangle.c.x) +
        (triangle.c.x - triangle.b.x) * (z - triangle.c.z)) / denominator;
    const float weightB = ((triangle.c.z - triangle.a.z) * (x - triangle.c.x) +
        (triangle.a.x - triangle.c.x) * (z - triangle.c.z)) / denominator;
    const float weightC = 1.0f - weightA - weightB;

    if (weightA < -TriangleMargin || weightB < -TriangleMargin || weightC < -TriangleMargin) {
        return false;
    }

    height = weightA * triangle.a.y + weightB * triangle.b.y + weightC * triangle.c.y;
    return true;
}

bool pointInPlatform(const ExtraPlatform& platform, float x, float z, bool projectDepth) {
    if (platform.shape == PlatformShape::Rectangle) {
        return x >= platform.minX && x <= platform.maxX &&
            (projectDepth || (z >= platform.minZ && z <= platform.maxZ));
    }

    const float centerX = (platform.minX + platform.maxX) * 0.5f;
    const float centerZ = (platform.minZ + platform.maxZ) * 0.5f;
    const float radiusX = (platform.maxX - platform.minX) * 0.5f;
    const float radiusZ = (platform.maxZ - platform.minZ) * 0.5f;
    const float dx = (x - centerX) / radiusX;
    const float dz = (z - centerZ) / radiusZ;
    return dx * dx + dz * dz <= 1.0f;
}
}

struct Mapa1::Impl {
    Shader shader;
    std::vector<WorldModel> worldModels;
    std::unordered_map<std::string, std::shared_ptr<Texture2D>> textureCache;
    std::array<Texture2D, 5> playerTextures;
    Texture2D skyboxTexture;
    Texture2D coinIconTexture;
    Texture2D platformSideTexture;
    Texture2D platformTopTexture;
    Mesh playerMesh;
    Mesh skyboxMesh;
    Mesh pipeBodyMesh;
    Mesh pipeRimMesh;
    Mesh pipeOpeningMesh;
    Mesh platformMesh;
    Mesh coinMesh;
    Mesh starMesh;
    std::vector<TerrainTriangle> terrainTriangles;
    std::vector<ExtraPlatform> extraPlatforms = {
        {PlatformShape::Ellipse, 0.60f, 1.40f, -0.40f, 0.40f, 0.18f, 0.24f, {0.36f, 0.72f, 0.32f, 1.0f}, "tuberia", false, false},
        {PlatformShape::Rectangle, -10.50f, -9.15f, 4.00f, 5.35f, -0.18f, 0.24f, {0.30f, 0.72f, 0.42f, 1.0f}, "oeste_01", true, false},
        {PlatformShape::Rectangle, -8.85f, -7.35f, 4.65f, 6.15f, 0.30f, 0.26f, {0.34f, 0.78f, 0.46f, 1.0f}, "oeste_02", true, false},
        {PlatformShape::Rectangle, -3.55f, -2.20f, -6.30f, -4.95f, -0.16f, 0.24f, {0.32f, 0.70f, 0.40f, 1.0f}, "sur_01", true, false},
        {PlatformShape::Rectangle, -1.90f, -0.40f, -5.70f, -4.20f, 0.34f, 0.26f, {0.38f, 0.80f, 0.48f, 1.0f}, "sur_02", true, false},
        {PlatformShape::Rectangle, 5.00f, 6.35f, 4.40f, 5.75f, -0.14f, 0.24f, {0.30f, 0.72f, 0.42f, 1.0f}, "norte_01", true, false},
        {PlatformShape::Rectangle, 6.70f, 8.20f, 5.00f, 6.50f, 0.38f, 0.26f, {0.38f, 0.82f, 0.50f, 1.0f}, "norte_02", true, false},
        {PlatformShape::Rectangle, 11.30f, 12.85f, -0.75f, 0.80f, 0.22f, 0.28f, {0.42f, 0.72f, 0.84f, 1.0f}, "perspectiva_entrada", true, false},
        {PlatformShape::Rectangle, 13.15f, 14.30f, -4.50f, -3.35f, 0.44f, 0.24f, {0.28f, 0.64f, 0.92f, 1.0f}, "perspectiva_01", true, true},
        {PlatformShape::Rectangle, 14.60f, 15.75f, 3.15f, 4.30f, 0.64f, 0.24f, {0.30f, 0.72f, 0.98f, 1.0f}, "perspectiva_02", true, true},
        {PlatformShape::Rectangle, 16.05f, 17.20f, -3.90f, -2.75f, 0.82f, 0.24f, {0.36f, 0.78f, 1.00f, 1.0f}, "perspectiva_03", true, true},
        {PlatformShape::Rectangle, 17.50f, 19.10f, 3.20f, 4.80f, 0.98f, 0.30f, {0.46f, 0.84f, 1.00f, 1.0f}, "perspectiva_medalla", true, true}
    };
    std::vector<Coin> coins;
    Star star;

    bool initialized{false};
    bool mode3D{false};
    bool tabPressed{false};
    float posX{0.0f};
    float posY{0.0f};
    float posZ{0.0f};
    float velocityY{0.0f};
    bool grounded{false};
    float playerAngle{0.0f};
    float cameraOffsetY{0.0f};
    int lives{StartingLives};
    float messageTime{0.0f};
    std::string statusMessage;
    float titleTime{0.0f};
    int currentFrame{0};
    float animationTime{0.0f};
    bool wasMoving{false};
    int collectedCoins{0};
    int coinMessageCount{0};
    float coinMessageUntil{0.0f};
    float starMessageUntil{0.0f};
    bool completed{false};

    std::shared_ptr<Texture2D> textureFor(const std::string& path) {
        if (path.empty()) {
            return {};
        }

        const std::string resolved = resolveAssetPath(path);
        auto found = textureCache.find(resolved);
        if (found != textureCache.end()) {
            return found->second;
        }

        auto texture = std::make_shared<Texture2D>();
        if (!texture->loadFromFile(resolved)) {
            std::cerr << "Mapa 1 texture could not be loaded: " << resolved << std::endl;
            return {};
        }
        textureCache.emplace(resolved, texture);
        return texture;
    }

    bool loadWorldModel(const std::string& path) {
        WorldModel world;
        world.model = ModelLoader::loadModel(resolveAssetPath(path));
        if (world.model.meshes.empty()) {
            return false;
        }

        world.materials.reserve(world.model.materials.size());
        for (const LoadedMaterial& material : world.model.materials) {
            MapMaterial runtimeMaterial;
            runtimeMaterial.color = glm::vec4(material.diffuseColor, material.opacity);
            runtimeMaterial.texture = textureFor(material.diffuseTexturePath);
            world.materials.push_back(std::move(runtimeMaterial));
        }

        worldModels.push_back(std::move(world));
        return true;
    }

    std::string meshName(const WorldModel& model, const LoadedMesh& mesh) const {
        std::string name = mesh.name;
        if (mesh.materialIndex < model.model.materials.size()) {
            name += "__";
            name += model.model.materials[mesh.materialIndex].name;
        }
        return name;
    }

    void buildTerrainCollisions() {
        terrainTriangles.clear();
        terrainTriangles.reserve(50000);
        glm::vec3 terrainMin(100000.0f);
        glm::vec3 terrainMax(-100000.0f);

        for (const WorldModel& model : worldModels) {
            for (const LoadedMesh& mesh : model.model.meshes) {
                const std::string name = meshName(model, mesh);
                if (!isWalkableMesh(name)) {
                    continue;
                }

                for (const LoadedMesh::CollisionBox& source : mesh.collisionBoxes) {
                    const glm::vec3 a = transformMapPoint(source.a);
                    const glm::vec3 b = transformMapPoint(source.b);
                    const glm::vec3 c = transformMapPoint(source.c);
                    const glm::vec3 normal = glm::cross(b - a, c - a);
                    const float normalLength = glm::length(normal);
                    if (normalLength < 0.000001f) {
                        continue;
                    }

                    const float normalY = std::fabs(normal.y / normalLength);
                    const float minY = std::min({a.y, b.y, c.y});
                    const float maxY = std::max({a.y, b.y, c.y});
                    if (normalY < MinimumWalkableNormal || maxY < MinimumWalkableHeight) {
                        continue;
                    }

                    const float minX = std::min({a.x, b.x, c.x});
                    const float maxX = std::max({a.x, b.x, c.x});
                    const float minZ = std::min({a.z, b.z, c.z});
                    const float maxZ = std::max({a.z, b.z, c.z});
                    if (maxX - minX < 0.002f || maxZ - minZ < 0.002f) {
                        continue;
                    }

                    terrainTriangles.push_back({a, b, c, minX, maxX, minZ, maxZ, &mesh.name});
                    terrainMin = glm::min(terrainMin, glm::min(glm::min(a, b), c));
                    terrainMax = glm::max(terrainMax, glm::max(glm::max(a, b), c));
                }
            }
        }

        std::cout << "Mapa 1 ready. Walkable terrain triangles: " << terrainTriangles.size() << std::endl;
        std::cout << "Mapa 1 terrain bounds: X[" << terrainMin.x << ", " << terrainMax.x
            << "] Z[" << terrainMin.z << ", " << terrainMax.z << "]" << std::endl;
        std::cout << "Mapa 1 parkour platforms: " << extraPlatforms.size() - 1 << std::endl;
        std::cout << "Mapa 1 controls: A/D and W jump in 2D, WASD and Space jump in 3D, Tab switches view, Esc returns to menu." << std::endl;
    }

    glm::vec3 platformTopCenter(size_t index) const {
        const ExtraPlatform& platform = extraPlatforms[index];
        return {
            (platform.minX + platform.maxX) * 0.5f,
            platform.height,
            (platform.minZ + platform.maxZ) * 0.5f
        };
    }

    bool terrainHeight(float x, float z, float& height) const {
        bool found = false;
        height = -100.0f;
        for (const TerrainTriangle& triangle : terrainTriangles) {
            float triangleHeight = 0.0f;
            if (heightInTriangle(triangle, x, z, triangleHeight) && triangleHeight >= height) {
                found = true;
                height = triangleHeight;
            }
        }
        return found;
    }

    bool findTerrainCoinPosition(const glm::vec2& anchor, glm::vec3& position) const {
        constexpr float step = 1.20f;
        constexpr int maxRing = 7;
        for (int ring = 0; ring <= maxRing; ++ring) {
            for (int xStep = -ring; xStep <= ring; ++xStep) {
                for (int zStep = -ring; zStep <= ring; ++zStep) {
                    if (ring > 0 && std::abs(xStep) != ring && std::abs(zStep) != ring) {
                        continue;
                    }

                    const float x = anchor.x + static_cast<float>(xStep) * step;
                    const float z = anchor.y + static_cast<float>(zStep) * step;
                    float height = -100.0f;
                    if (terrainHeight(x, z, height)) {
                        position = {x, height + 0.72f, z};
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void resetMission() {
        coins.clear();
        const std::array<size_t, 4> coinPlatforms = {0, 2, 4, 6};
        for (size_t index : coinPlatforms) {
            Coin coin;
            coin.position = platformTopCenter(index) + glm::vec3(0.0f, 0.72f, 0.0f);
            coin.phase = static_cast<float>(coins.size()) * 0.73f;
            coins.push_back(coin);
        }

        const std::array<glm::vec2, 6> terrainAnchors = {
            glm::vec2(-18.0f, -7.0f),
            glm::vec2(-14.0f, 8.0f),
            glm::vec2(-5.0f, 10.0f),
            glm::vec2(3.0f, -9.0f),
            glm::vec2(12.0f, 8.0f),
            glm::vec2(21.0f, -6.0f)
        };
        for (const glm::vec2& anchor : terrainAnchors) {
            Coin coin;
            if (!findTerrainCoinPosition(anchor, coin.position)) {
                std::cerr << "Mapa 1 could not place a terrain coin near X:" << anchor.x << " Z:" << anchor.y << "." << std::endl;
                continue;
            }
            coin.phase = static_cast<float>(coins.size()) * 0.73f;
            coins.push_back(coin);
        }

        star = {};
        star.position = platformTopCenter(extraPlatforms.size() - 1) + glm::vec3(0.0f, 0.95f, 0.0f);
        star.projectIn2D = true;
        collectedCoins = 0;
        coinMessageCount = 0;
        coinMessageUntil = 0.0f;
        starMessageUntil = 0.0f;
        completed = false;
    }

    bool validateParkourMission() const {
        if (coins.size() != TotalCoins) {
            std::cerr << "Mapa 1 mission expected " << TotalCoins << " coins but created " << coins.size() << "." << std::endl;
            return false;
        }

        const float maximumJumpHeight = (JumpSpeed * JumpSpeed) / (2.0f * Gravity);
        const float maximumProjectedJumpDistance = MoveSpeed2D * (2.0f * JumpSpeed / Gravity);
        float maximumPerspectiveRise = 0.0f;
        float maximumProjectedGap = 0.0f;
        float maximumPhysicalGap = 0.0f;
        for (size_t i = 8; i < extraPlatforms.size(); ++i) {
            const ExtraPlatform& previous = extraPlatforms[i - 1];
            const ExtraPlatform& current = extraPlatforms[i];
            maximumPerspectiveRise = std::max(maximumPerspectiveRise, current.height - previous.height);
            maximumProjectedGap = std::max(maximumProjectedGap, current.minX - previous.maxX);
            maximumPhysicalGap = std::max(maximumPhysicalGap, glm::length(glm::vec2(
                (current.minX + current.maxX - previous.minX - previous.maxX) * 0.5f,
                (current.minZ + current.maxZ - previous.minZ - previous.maxZ) * 0.5f)));
        }

        if (maximumPerspectiveRise > maximumJumpHeight - 0.08f ||
            maximumProjectedGap > maximumProjectedJumpDistance - 0.08f) {
            std::cerr << "Mapa 1 perspective route is unreachable after projection." << std::endl;
            return false;
        }

        if (maximumPhysicalGap <= maximumProjectedJumpDistance + 1.0f) {
            std::cerr << "Mapa 1 perspective route is not separated enough in 3D." << std::endl;
            return false;
        }

        glm::vec2 coinMin(std::numeric_limits<float>::max());
        glm::vec2 coinMax(std::numeric_limits<float>::lowest());
        for (const Coin& coin : coins) {
            coinMin = glm::min(coinMin, glm::vec2(coin.position.x, coin.position.z));
            coinMax = glm::max(coinMax, glm::vec2(coin.position.x, coin.position.z));
        }
        const glm::vec2 coinSpread = coinMax - coinMin;
        if (coinSpread.x < 24.0f || coinSpread.y < 12.0f) {
            std::cerr << "Mapa 1 coins are not scattered widely enough across the map." << std::endl;
            return false;
        }

        std::cout << "Mapa 1 mission coins: " << coins.size()
            << " | jump height: " << maximumJumpHeight
            << " | projected bridge gap: " << maximumProjectedGap
            << " | physical bridge gap: " << maximumPhysicalGap
            << " | coin spread: X " << coinSpread.x << " Z " << coinSpread.y << std::endl;
        return true;
    }

    bool touchesCollectible(const glm::vec3& position, float horizontalRadius, float verticalRadius, bool projectIn2D) const {
        const glm::vec3 playerCenter(posX, posY + 0.58f, posZ);
        return std::abs(playerCenter.x - position.x) <= horizontalRadius &&
            std::abs(playerCenter.y - position.y) <= verticalRadius &&
            ((projectIn2D && !mode3D) || std::abs(playerCenter.z - position.z) <= horizontalRadius);
    }

    void updateMission(float now) {
        if (completed) {
            return;
        }

        for (Coin& coin : coins) {
            if (!coin.collected && touchesCollectible(coin.position, 0.56f, 0.72f, coin.projectIn2D)) {
                coin.collected = true;
                collectedCoins = std::min(collectedCoins + 1, TotalCoins);
                coinMessageCount = collectedCoins;
                coinMessageUntil = now + 2.6f;
                showStatus("MONEDA " + std::to_string(collectedCoins) + "/" + std::to_string(TotalCoins));

                if (collectedCoins >= TotalCoins) {
                    star.active = true;
                    starMessageUntil = now + 4.0f;
                    showStatus("Todas las monedas listas - recoge la estrella");
                }
                break;
            }
        }

        if (star.active && touchesCollectible(star.position, 0.82f, 0.98f, star.projectIn2D)) {
            star.active = false;
            completed = true;
            showStatus("NIVEL COMPLETADO");
        }
    }

    bool groundHeight(float x, float z, float& height, const char** zone = nullptr) const {
        bool found = false;
        height = -100.0f;

        for (const TerrainTriangle& triangle : terrainTriangles) {
            float triangleHeight = 0.0f;
            if (heightInTriangle(triangle, x, z, triangleHeight) && triangleHeight >= height) {
                found = true;
                height = triangleHeight;
                if (zone != nullptr) {
                    *zone = triangle.name != nullptr ? triangle.name->c_str() : "mapa";
                }
            }
        }

        for (const ExtraPlatform& platform : extraPlatforms) {
            if (pointInPlatform(platform, x, z, platform.projectIn2D && !mode3D) && platform.height >= height) {
                found = true;
                height = platform.height;
                if (zone != nullptr) {
                    *zone = platform.name;
                }
            }
        }

        if (!found && zone != nullptr) {
            *zone = "hueco";
        }
        return found;
    }

    bool landingHeight(float x, float z, float maximumHeight, float& height) const {
        bool found = false;
        height = -100.0f;

        for (const TerrainTriangle& triangle : terrainTriangles) {
            float triangleHeight = 0.0f;
            if (heightInTriangle(triangle, x, z, triangleHeight) &&
                triangleHeight <= maximumHeight &&
                triangleHeight >= height) {
                found = true;
                height = triangleHeight;
            }
        }

        for (const ExtraPlatform& platform : extraPlatforms) {
            if (pointInPlatform(platform, x, z, platform.projectIn2D && !mode3D) &&
                platform.height <= maximumHeight &&
                platform.height >= height) {
                found = true;
                height = platform.height;
            }
        }
        return found;
    }

    bool movementAllowed(float currentX, float currentZ, float nextX, float nextZ) const {
        float destinationHeight = -100.0f;
        if (!groundHeight(nextX, nextZ, destinationHeight)) {
            return true;
        }

        float currentHeight = -100.0f;
        if (!landingHeight(currentX, currentZ, posY + LandingMargin, currentHeight)) {
            currentHeight = posY;
        }

        const bool tallStep = destinationHeight - currentHeight > StepHeightWithoutJump;
        const bool notHighEnough = posY + LandingMargin < destinationHeight;
        return !(tallStep && notHighEnough);
    }

    void showStatus(const std::string& message) {
        statusMessage = message;
        messageTime = 2.5f;
        std::cout << message << std::endl;
    }

    void resetPlayer(bool resetMode) {
        if (resetMode) {
            mode3D = false;
        }

        posX = 0.0f;
        posZ = 0.0f;
        velocityY = 0.0f;
        cameraOffsetY = 0.0f;
        playerAngle = mode3D ? 90.0f : 0.0f;

        float initialHeight = -1.0f;
        posY = groundHeight(posX, posZ, initialHeight) ? initialHeight : 0.0f;
        grounded = true;
    }

    void loseLife() {
        --lives;
        if (lives <= 0) {
            lives = StartingLives;
            resetPlayer(true);
            resetMission();
            showStatus("GAME OVER - se reinicio el mapa. Vidas: 3");
            return;
        }

        resetPlayer(false);
        showStatus("MORISTE - vidas restantes: " + std::to_string(lives));
    }

    bool isMoving(GLFWwindow* window) const {
        if (!mode3D) {
            return glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
        }

        return glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
    }

    void updateTitle(GLFWwindow* window, float deltaTime) {
        titleTime += deltaTime;
        if (titleTime < 0.5f && messageTime <= 0.0f) {
            return;
        }

        std::ostringstream title;
        title << std::fixed << std::setprecision(2);
        if (messageTime > 0.0f && !statusMessage.empty()) {
            title << statusMessage << " | ";
        }
        title << (mode3D ? "Paper Mario 3D" : "Paper Mario 2D")
            << " | Vidas:" << lives
            << " | Monedas:" << collectedCoins << "/" << TotalCoins
            << " | X:" << posX
            << " Y:" << posY;
        if (mode3D) {
            title << " Z:" << posZ;
        }
        glfwSetWindowTitle(window, title.str().c_str());
        titleTime = 0.0f;
    }

    void update(GLFWwindow* window, float deltaTime) {
        const float dt = std::clamp(deltaTime, 0.0f, 0.05f);
        if (messageTime > 0.0f) {
            messageTime = std::max(0.0f, messageTime - dt);
            if (messageTime <= 0.0f) {
                statusMessage.clear();
            }
        }
        updateTitle(window, dt);
        if (completed) {
            return;
        }

        const bool tabDown = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
        if (tabDown && !tabPressed) {
            mode3D = !mode3D;
            if (!mode3D) {
                posZ = 0.0f;
                playerAngle = 0.0f;
            }
            std::cout << "Mapa 1 view changed to " << (mode3D ? "3D." : "2D.") << std::endl;
        }
        tabPressed = tabDown;

        const bool movingNow = isMoving(window);
        if (movingNow) {
            animationTime += dt;
            if (animationTime >= 0.10f) {
                animationTime = 0.0f;
                currentFrame = (currentFrame + 1) % static_cast<int>(playerTextures.size());
            }
        } else {
            currentFrame = 0;
            animationTime = 0.0f;
        }
        wasMoving = movingNow;

        float nextX = posX;
        float nextZ = posZ;
        if (!mode3D) {
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                nextX -= MoveSpeed2D * dt;
                playerAngle = 180.0f;
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                nextX += MoveSpeed2D * dt;
                playerAngle = 0.0f;
            }
        } else {
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                nextX += MoveSpeed3D * dt;
                playerAngle = 90.0f;
            }
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                nextX -= MoveSpeed3D * dt;
                playerAngle = 270.0f;
            }
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                nextZ -= MoveSpeed3D * dt;
                playerAngle = 90.0f;
            }
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                nextZ += MoveSpeed3D * dt;
                playerAngle = 270.0f;
            }
        }

        if (movementAllowed(posX, posZ, nextX, posZ)) {
            posX = nextX;
        }
        if (movementAllowed(posX, posZ, posX, nextZ)) {
            posZ = nextZ;
        }

        const bool jumpDown = (!mode3D && glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) ||
            (mode3D && glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
        if (jumpDown && grounded) {
            velocityY = JumpSpeed;
            grounded = false;
        }

        velocityY -= Gravity * dt;
        posY += velocityY * dt;

        float floorHeight = -1.0f;
        const bool hasFloor = landingHeight(posX, posZ, posY + LandingMargin, floorHeight);
        if (hasFloor && velocityY <= 0.0f && posY <= floorHeight + LandingMargin) {
            posY = floorHeight;
            velocityY = 0.0f;
            grounded = true;
        } else {
            grounded = false;
        }

        if (posY <= DeathHeight) {
            loseLife();
        }

        updateMission(static_cast<float>(glfwGetTime()));

        const float targetCameraY = std::clamp(
            (posY - CameraBaseHeight) * CameraVerticalTracking,
            0.0f,
            CameraMaximumHeight);
        const float cameraBlend = std::clamp(dt * CameraSmoothing, 0.0f, 1.0f);
        cameraOffsetY += (targetCameraY - cameraOffsetY) * cameraBlend;
    }

    void drawSkybox(const glm::mat4& view, const glm::mat4& projection) {
        glDepthMask(GL_FALSE);
        shader.setMat4("view", glm::mat4(glm::mat3(view)));
        shader.setMat4("projection", projection);
        shader.setMat4("model", glm::scale(glm::mat4(1.0f), glm::vec3(60.0f)));
        shader.setInt("modoRender", 0);
        shader.setInt("tex0", 0);
        skyboxTexture.bind();
        skyboxMesh.draw();
        glDepthMask(GL_TRUE);
        shader.setMat4("view", view);
    }

    void drawColoredMesh(const Mesh& mesh, const glm::mat4& model, const glm::vec4& color) {
        shader.setMat4("model", model);
        shader.setInt("modoRender", 2);
        shader.setVec4("colorMaterial", color);
        mesh.draw();
    }

    void drawTexturedMesh(const Mesh& mesh, const glm::mat4& model, const Texture2D& texture, const glm::vec4& fallbackColor) {
        if (!texture.valid()) {
            drawColoredMesh(mesh, model, fallbackColor);
            return;
        }

        shader.setMat4("model", model);
        shader.setInt("modoRender", 0);
        shader.setInt("tex0", 0);
        texture.bind();
        mesh.draw();
    }

    float renderedDepth(float depth, bool projectIn2D) const {
        return projectIn2D && !mode3D ? posZ : depth;
    }

    void drawPipe() {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), {1.0f - posX, -0.44f, -posZ});
        model = glm::scale(model, {0.28f, 0.95f, 0.28f});
        drawColoredMesh(pipeBodyMesh, model, {0.36f, 0.72f, 0.32f, 1.0f});

        model = glm::translate(glm::mat4(1.0f), {1.0f - posX, 0.03f, -posZ});
        model = glm::scale(model, {0.40f, 0.22f, 0.40f});
        drawColoredMesh(pipeRimMesh, model, {0.48f, 0.82f, 0.38f, 1.0f});

        model = glm::translate(glm::mat4(1.0f), {1.0f - posX, 0.15f, -posZ});
        model = glm::scale(model, {0.30f, 0.025f, 0.30f});
        drawColoredMesh(pipeOpeningMesh, model, {0.02f, 0.10f, 0.08f, 1.0f});
    }

    void drawParkourPlatforms() {
        for (const ExtraPlatform& platform : extraPlatforms) {
            if (!platform.visible) {
                continue;
            }

            const float width = platform.maxX - platform.minX;
            const float depth = platform.maxZ - platform.minZ;
            const float centerX = (platform.minX + platform.maxX) * 0.5f - posX;
            const float centerZ = renderedDepth((platform.minZ + platform.maxZ) * 0.5f, platform.projectIn2D) - posZ;

            glm::mat4 model = glm::translate(glm::mat4(1.0f), {centerX, platform.height - platform.thickness * 0.5f, centerZ});
            model = glm::scale(model, {width, platform.thickness, depth});
            drawTexturedMesh(platformMesh, model, platformSideTexture, platform.color);

            model = glm::translate(glm::mat4(1.0f), {centerX, platform.height - platform.thickness + 0.028f, centerZ});
            model = glm::scale(model, {width * 1.035f, 0.055f, depth * 1.035f});
            drawColoredMesh(platformMesh, model, platform.color);

            model = glm::translate(glm::mat4(1.0f), {centerX, platform.height + 0.014f, centerZ});
            model = glm::scale(model, {width * 0.96f, 0.028f, depth * 0.96f});
            drawTexturedMesh(platformMesh, model, platformTopTexture, {0.48f, 0.82f, 0.42f, 1.0f});
        }
    }

    void drawMissionItems(float now) {
        for (const Coin& coin : coins) {
            if (coin.collected) {
                continue;
            }

            const float bob = std::sin(now * 2.4f + coin.phase) * 0.10f;
            glm::mat4 model = glm::translate(
                glm::mat4(1.0f),
                {coin.position.x - posX, coin.position.y + bob, renderedDepth(coin.position.z, coin.projectIn2D) - posZ});
            model = glm::rotate(model, now * 5.2f + coin.phase, {0.0f, 1.0f, 0.0f});
            model = glm::rotate(model, glm::half_pi<float>(), {1.0f, 0.0f, 0.0f});
            drawColoredMesh(coinMesh, model, {1.0f, 0.74f, 0.08f, 1.0f});
        }

        if (star.active) {
            const float bob = std::sin(now * 2.8f) * 0.14f;
            glm::mat4 model = glm::translate(
                glm::mat4(1.0f),
                {star.position.x - posX, star.position.y + bob, renderedDepth(star.position.z, star.projectIn2D) - posZ});
            model = glm::rotate(model, now * 2.8f, {0.0f, 1.0f, 0.0f});
            model = glm::scale(model, glm::vec3(0.88f));
            drawColoredMesh(starMesh, model, {1.0f, 0.86f, 0.12f, 1.0f});
        }
    }

    void drawWorld() {
        glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), {-posX, -1.0f, -posZ});
        modelMatrix = glm::scale(modelMatrix, glm::vec3(MapScale));
        modelMatrix = glm::translate(modelMatrix, MapCenter);
        shader.setMat4("model", modelMatrix);

        for (const WorldModel& world : worldModels) {
            for (const LoadedMesh& mesh : world.model.meshes) {
                const MapMaterial* material = mesh.materialIndex < world.materials.size()
                    ? &world.materials[mesh.materialIndex]
                    : nullptr;

                if (material != nullptr && material->texture != nullptr && material->texture->valid()) {
                    shader.setInt("modoRender", 0);
                    shader.setInt("tex0", 0);
                    material->texture->bind();
                } else {
                    shader.setInt("modoRender", 2);
                    shader.setVec4("colorMaterial", material != nullptr ? material->color : glm::vec4(1.0f));
                }
                mesh.mesh.draw();
            }
        }
    }

    void drawPlayer() {
        const float step = wasMoving ? std::sin(static_cast<float>(glfwGetTime()) * 14.0f) : 0.0f;
        const float bounce = wasMoving ? std::fabs(step) * 0.035f : 0.0f;
        const float tilt = wasMoving ? step * 4.0f : 0.0f;

        glm::mat4 model = glm::translate(glm::mat4(1.0f), {0.0f, posY + bounce, 0.22f});
        model = glm::rotate(model, glm::radians(playerAngle), {0.0f, 1.0f, 0.0f});
        model = glm::rotate(model, glm::radians(tilt), {0.0f, 0.0f, 1.0f});
        shader.setMat4("model", model);
        shader.setInt("modoRender", 0);
        shader.setInt("tex0", 0);
        playerTextures[static_cast<size_t>(currentFrame)].bind();
        playerMesh.draw();
    }

    bool initialize() {
        if (initialized) {
            return true;
        }

        if (!shader.load(resolveAssetPath("shaders/mapa1.vert"), resolveAssetPath("shaders/mapa1.frag"))) {
            return false;
        }

        const std::array<std::string, 5> playerPaths = {
            "assets/mapa1/player/vergil_idle.png",
            "assets/mapa1/player/vergil_walk1.png",
            "assets/mapa1/player/vergil_walk2.png",
            "assets/mapa1/player/vergil_walk3.png",
            "assets/mapa1/player/vergil_walk4.png"
        };
        for (size_t i = 0; i < playerPaths.size(); ++i) {
            if (!playerTextures[i].loadFromFile(resolveAssetPath(playerPaths[i]))) {
                std::cerr << "Mapa 1 player texture could not be loaded: " << playerPaths[i] << std::endl;
                return false;
            }
        }

        if (!skyboxTexture.loadFromFile(resolveAssetPath("assets/mapa1/skybox/R.jpg"))) {
            std::cerr << "Mapa 1 skybox could not be loaded." << std::endl;
            return false;
        }
        coinIconTexture.loadFromFile(resolveAssetPath("assets/images/coin_spin.png"));
        if (!platformSideTexture.loadFromFile(resolveAssetPath("assets/mapa1/world1/W1WallBlock_DM_alb.png")) ||
            !platformTopTexture.loadFromFile(resolveAssetPath("assets/mapa1/world1/Grass01_YD_alb.png"))) {
            std::cerr << "Mapa 1 parkour textures could not be loaded." << std::endl;
            return false;
        }
        skyboxTexture.bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        const std::string worldBase = "assets/mapa1/world1/";
        const bool loaded =
            loadWorldModel(worldBase + "CourseSelectW1.dae") &&
            loadWorldModel(worldBase + "CourseSelectDecorationW1A.dae") &&
            loadWorldModel(worldBase + "CourseSelectWaveW1.dae") &&
            loadWorldModel(worldBase + "CourseSelectWallW1.dae");
        if (!loaded) {
            std::cerr << "Mapa 1 world models could not be loaded." << std::endl;
            return false;
        }

        playerMesh = createPlayerMesh();
        skyboxMesh = createSkyboxMesh();
        pipeBodyMesh = Mesh::cylinder(8);
        pipeRimMesh = Mesh::cylinder(8);
        pipeOpeningMesh = Mesh::cylinder(8);
        platformMesh = Mesh::cube();
        coinMesh = Mesh::cylinder(32, 0.14f, 0.42f);
        starMesh = createStarMesh();
        buildTerrainCollisions();
        if (terrainTriangles.empty()) {
            std::cerr << "Mapa 1 did not generate walkable terrain." << std::endl;
            return false;
        }

        lives = StartingLives;
        statusMessage.clear();
        messageTime = 0.0f;
        titleTime = 0.0f;
        currentFrame = 0;
        animationTime = 0.0f;
        wasMoving = false;
        tabPressed = false;
        resetPlayer(true);
        resetMission();
        if (!validateParkourMission()) {
            return false;
        }
        initialized = true;
        return true;
    }

    void render(GLFWwindow* window, float deltaTime) {
        update(window, deltaTime);

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
        const glm::mat4 projection = glm::perspective(glm::radians(mode3D ? 60.0f : 45.0f), aspect, 0.1f, 100.0f);

        glm::mat4 view(1.0f);
        if (!mode3D) {
            view = glm::translate(view, {0.0f, -cameraOffsetY, -3.0f});
        } else {
            const glm::vec3 cameraPosition(-3.0f, 1.0f + cameraOffsetY, 0.0f);
            const glm::vec3 cameraTarget(0.0f, 0.5f + cameraOffsetY, 0.0f);
            view = glm::lookAt(cameraPosition, cameraTarget, {0.0f, 1.0f, 0.0f});
        }

        glEnable(GL_DEPTH_TEST);
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        shader.setMat4("view", view);
        shader.setMat4("projection", projection);
        drawSkybox(view, projection);
        drawPipe();
        drawWorld();
        drawParkourPlatforms();
        drawMissionItems(static_cast<float>(glfwGetTime()));
        drawPlayer();
    }

    void shutdown() {
        terrainTriangles.clear();
        worldModels.clear();
        textureCache.clear();
        for (Texture2D& texture : playerTextures) {
            texture = Texture2D{};
        }
        skyboxTexture = Texture2D{};
        coinIconTexture = Texture2D{};
        platformSideTexture = Texture2D{};
        platformTopTexture = Texture2D{};
        playerMesh = Mesh{};
        skyboxMesh = Mesh{};
        pipeBodyMesh = Mesh{};
        pipeRimMesh = Mesh{};
        pipeOpeningMesh = Mesh{};
        platformMesh = Mesh{};
        coinMesh = Mesh{};
        starMesh = Mesh{};
        coins.clear();
        star = {};
        shader = Shader{};
        initialized = false;
    }
};

Mapa1::Mapa1()
    : m_impl(std::make_unique<Impl>()) {
}

Mapa1::~Mapa1() = default;

bool Mapa1::initialize() {
    return m_impl->initialize();
}

void Mapa1::render(GLFWwindow* window, float deltaTime) {
    m_impl->render(window, deltaTime);
}

void Mapa1::shutdown() {
    m_impl->shutdown();
}

int Mapa1::collectedCount() const {
    return m_impl->collectedCoins;
}

int Mapa1::messageCount() const {
    return m_impl->coinMessageCount;
}

bool Mapa1::showCoinMessage(float timeSeconds) const {
    return timeSeconds <= m_impl->coinMessageUntil;
}

bool Mapa1::showStarMessage(float timeSeconds) const {
    return timeSeconds <= m_impl->starMessageUntil;
}

bool Mapa1::levelComplete() const {
    return m_impl->completed;
}

const Texture2D& Mapa1::coinIconTexture() const {
    return m_impl->coinIconTexture;
}
