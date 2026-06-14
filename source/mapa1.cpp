#include "Mapa1.h"

#include "AudioPlayer.h"
#include "Mesh.h"
#include "ModelLoader.h"
#include "Shader.h"
#include "Texture2D.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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
    constexpr int PlayerMaximumHealth = 3;
    constexpr int EnemyMaximumHealth = 3;
    constexpr int TotalCoins = 10;
    constexpr float DeathHeight = -4.0f;
    constexpr float MoveSpeed2D = 1.20f;
    constexpr float MoveSpeed3D = 1.28f;
    constexpr float JumpSpeed = 6.25f;
    constexpr float Gravity = 9.20f;
    constexpr float EnemyDetectionRange = 9.50f;
    constexpr float EnemyAttackRange = 4.60f;
    constexpr float EnemyStopDistance = 1.10f;
    constexpr float EnemyMoveSpeed = 0.82f;
    constexpr float EnemyMaximumStepHeight = 0.24f;
    constexpr float EnemyShotCooldown = 2.80f;
    constexpr float EnemyShotSpeed = 3.20f;
    constexpr float PlayerShotSpeed = 5.40f;
    constexpr float ChargedPlayerShotSpeed = 6.20f;
    constexpr float ProjectileLifetime = 4.20f;
    constexpr float PlayerInvulnerabilityTime = 1.25f;
    constexpr float PlayerAttackCooldown = 0.38f;
    constexpr float PlayerChargeTime = 1.65f;
    constexpr float ParryWindow = 0.50f;
    constexpr float ParryEffectTime = 0.34f;
    constexpr float ProjectileSwordLength = 0.72f;
    constexpr float PlayerSpriteHeight = 1.15f;
    constexpr float CameraPlayerCenterOffset = PlayerSpriteHeight * 0.5f;
    constexpr float PlayerRunFramesPerSecond = 12.0f;
    constexpr float PlayerEntranceFramesPerSecond = 11.0f;
    constexpr float PlayerDeathFramesPerSecond = 7.0f;
    constexpr float PlayerTransitionFramesPerSecond = 16.0f;
    constexpr int EnemySpawnCount = 11;
    constexpr int SpectralGemRequirement = 5;
    constexpr int DamageParryGemRequirement = 3;
    constexpr int MaximumStoredGems = 99;
    constexpr float GemPickupRadius = 0.62f;
    constexpr float GemPickupVerticalRange = 0.82f;
    constexpr float SpectralAnchorPromptRange = 0.95f;
    constexpr float SpectralAnchorVerticalRange = 1.35f;
    constexpr float VanPromptRange = 3.25f;
    constexpr float VanPromptVerticalRange = 4.80f;
    constexpr float VanRenderRangeX = 30.0f;
    constexpr float VanRenderRangeZ = 30.0f;
    constexpr float VanModelDisplaySize = 4.20f;
    constexpr float GemModelDisplaySize = 0.62f;
    constexpr float PlayerSpawnDistanceFromVan = 2.25f;
    constexpr float SpectralStepCooldown = 0.75f;
    constexpr float SpectralHintTime = 3.8f;
    constexpr size_t NormalRouteFirstPlatformIndex = 8;
    constexpr size_t SpectralAnchorPlatformIndex = 11;
    constexpr size_t FirstSpectralIslandIndex = 12;
    constexpr size_t FinalSpectralIslandIndex = 13;
    constexpr size_t VanRouteFirstPlatformIndex = 14;
    constexpr size_t VanIslandPlatformIndex = 18;

    struct TerrainTriangle {
        glm::vec3 a{ 0.0f };
        glm::vec3 b{ 0.0f };
        glm::vec3 c{ 0.0f };
        float minX{ 0.0f };
        float maxX{ 0.0f };
        float minZ{ 0.0f };
        float maxZ{ 0.0f };
        const std::string* name{ nullptr };
    };

    enum class PlatformShape {
        Rectangle,
        Ellipse
    };

    struct ExtraPlatform {
        PlatformShape shape{ PlatformShape::Rectangle };
        float minX{ 0.0f };
        float maxX{ 0.0f };
        float minZ{ 0.0f };
        float maxZ{ 0.0f };
        float height{ 0.0f };
        float thickness{ 0.24f };
        glm::vec4 color{ 1.0f };
        const char* name{ nullptr };
        bool visible{ true };
        bool projectIn2D{ false };
    };

    struct Coin {
        glm::vec3 position{ 0.0f };
        bool collected{ false };
        float phase{ 0.0f };
        bool projectIn2D{ false };
    };

    struct Star {
        glm::vec3 position{ 0.0f };
        bool active{ false };
        bool projectIn2D{ false };
    };

    struct SpectralAnchor {
        glm::vec3 origin{ 0.0f };
        glm::vec3 target{ 0.0f };
        bool projectIn2D{ true };
        float phase{ 0.0f };
    };

    struct DroppedGem {
        glm::vec3 position{ 0.0f };
        float phase{ 0.0f };
        bool projectIn2D{ true };
    };

    enum class EnemyState {
        Idle,
        Chase,
        Attack,
        Hurt,
        Dying
    };

    struct DemonEnemy {
        glm::vec3 position{ 0.0f };
        glm::vec3 spawnPosition{ 0.0f };
        EnemyState state{ EnemyState::Idle };
        float shotCooldown{ 0.0f };
        float animationTime{ 0.0f };
        float hurtTime{ 0.0f };
        int health{ EnemyMaximumHealth };
        bool facingLeft{ false };
        bool alive{ true };
    };

    struct Projectile {
        glm::vec3 position{ 0.0f };
        glm::vec3 velocity{ 0.0f };
        float lifetime{ ProjectileLifetime };
        bool fromPlayer{ false };
        int damage{ 1 };
        bool charged{ false };
    };

    struct AtlasFrame {
        int x{ 0 };
        int y{ 0 };
        int width{ 0 };
        int height{ 0 };
    };

    struct MapMaterial {
        glm::vec4 color{ 1.0f };
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
        return { position, {0.0f, 0.0f, 1.0f}, uv, glm::vec4(1.0f) };
    }

    Mesh createPlayerMesh() {
        const std::vector<Vertex> vertices = {
            makeVertex({0.42f, 1.15f, 0.0f}, {1.0f, 1.0f}),
            makeVertex({0.42f, 0.00f, 0.0f}, {1.0f, 0.0f}),
            makeVertex({-0.42f, 0.00f, 0.0f}, {0.0f, 0.0f}),
            makeVertex({-0.42f, 1.15f, 0.0f}, {0.0f, 1.0f})
        };
        const std::vector<unsigned int> indices = { 0, 1, 2, 0, 2, 3 };

        Mesh mesh;
        mesh.upload(vertices, indices);
        return mesh;
    }

    Mesh createSpriteMesh(const AtlasFrame& frame, float width, float height, int atlasWidth, int atlasHeight) {
        constexpr float inset = 0.18f;
        const float u0 = (static_cast<float>(frame.x) + inset) / static_cast<float>(atlasWidth);
        const float u1 = (static_cast<float>(frame.x + frame.width) - inset) / static_cast<float>(atlasWidth);
        const float vTop = 1.0f - (static_cast<float>(frame.y) + inset) / static_cast<float>(atlasHeight);
        const float vBottom = 1.0f - (static_cast<float>(frame.y + frame.height) - inset) / static_cast<float>(atlasHeight);
        const float halfWidth = width * 0.5f;
        const std::vector<Vertex> vertices = {
            makeVertex({halfWidth, height, 0.0f}, {u1, vTop}),
            makeVertex({halfWidth, 0.0f, 0.0f}, {u1, vBottom}),
            makeVertex({-halfWidth, 0.0f, 0.0f}, {u0, vBottom}),
            makeVertex({-halfWidth, height, 0.0f}, {u0, vTop})
        };
        const std::vector<unsigned int> indices = { 0, 1, 2, 0, 2, 3 };

        Mesh mesh;
        mesh.upload(vertices, indices);
        return mesh;
    }

    Mesh createPlayerSpriteMesh(const AtlasFrame& frame, int atlasWidth, int atlasHeight) {
        const float aspect = static_cast<float>(frame.width) / static_cast<float>(std::max(frame.height, 1));
        return createSpriteMesh(frame, PlayerSpriteHeight * aspect, PlayerSpriteHeight, atlasWidth, atlasHeight);
    }

    float animationDuration(const std::vector<Mesh>& frames, float framesPerSecond) {
        if (frames.empty() || framesPerSecond <= 0.0f) {
            return 0.0f;
        }
        return static_cast<float>(frames.size()) / framesPerSecond;
    }

    const Mesh* animationFrame(const std::vector<Mesh>& frames, float time, float framesPerSecond, bool loop) {
        if (frames.empty()) {
            return nullptr;
        }

        size_t frameIndex = static_cast<size_t>(std::max(0.0f, time) * framesPerSecond);
        frameIndex = loop
            ? frameIndex % frames.size()
            : std::min(frameIndex, frames.size() - 1);
        return &frames[frameIndex];
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

        vertices.push_back({ {0.0f, 0.0f, frontZ}, {0.0f, 0.0f, 1.0f}, {0.5f, 0.5f}, glm::vec4(1.0f) });
        vertices.push_back({ {0.0f, 0.0f, backZ}, {0.0f, 0.0f, -1.0f}, {0.5f, 0.5f}, glm::vec4(1.0f) });
        for (int i = 0; i < points; ++i) {
            const float radius = (i % 2 == 0) ? 0.72f : 0.33f;
            const float angle = glm::half_pi<float>() + glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(points);
            const float x = std::cos(angle) * radius;
            const float y = std::sin(angle) * radius;
            vertices.push_back({ {x, y, frontZ}, {0.0f, 0.0f, 1.0f}, {(x / 1.6f) + 0.5f, (y / 1.6f) + 0.5f}, glm::vec4(1.0f) });
            vertices.push_back({ {x, y, backZ}, {0.0f, 0.0f, -1.0f}, {(x / 1.6f) + 0.5f, (y / 1.6f) + 0.5f}, glm::vec4(1.0f) });
        }

        for (int i = 0; i < points; ++i) {
            const unsigned int frontA = 2u + static_cast<unsigned int>(i) * 2u;
            const unsigned int backA = frontA + 1u;
            const unsigned int frontB = 2u + static_cast<unsigned int>((i + 1) % points) * 2u;
            const unsigned int backB = frontB + 1u;
            indices.insert(indices.end(), { 0u, frontA, frontB });
            indices.insert(indices.end(), { 1u, backB, backA });
            indices.insert(indices.end(), { frontA, backA, backB, frontA, backB, frontB });
        }

        Mesh mesh;
        mesh.upload(vertices, indices);
        return mesh;
    }

    Mesh createParryRingMesh() {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        constexpr int segments = 48;
        constexpr float outerRadius = 0.66f;
        constexpr float innerRadius = 0.52f;
        vertices.reserve(static_cast<size_t>((segments + 1) * 2));
        indices.reserve(static_cast<size_t>(segments * 6));

        for (int index = 0; index <= segments; ++index) {
            const float angle = glm::two_pi<float>() * static_cast<float>(index) / static_cast<float>(segments);
            const glm::vec2 direction(std::cos(angle), std::sin(angle));
            vertices.push_back(makeVertex({ direction.x * outerRadius, direction.y * outerRadius, 0.0f }, { 1.0f, 1.0f }));
            vertices.push_back(makeVertex({ direction.x * innerRadius, direction.y * innerRadius, 0.0f }, { 0.0f, 0.0f }));
        }

        for (int index = 0; index < segments; ++index) {
            const unsigned int outer = static_cast<unsigned int>(index * 2);
            const unsigned int inner = outer + 1;
            indices.insert(indices.end(), { outer, inner, outer + 2, inner, inner + 2, outer + 2 });
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

    glm::quat rotationBetweenVectors(glm::vec3 from, glm::vec3 to) {
        from = glm::normalize(from);
        to = glm::normalize(to);
        const float cosine = glm::dot(from, to);
        if (cosine < -0.9999f) {
            glm::vec3 axis = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), from);
            if (glm::dot(axis, axis) < 0.0001f) {
                axis = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), from);
            }
            return glm::angleAxis(glm::pi<float>(), glm::normalize(axis));
        }

        const glm::vec3 axis = glm::cross(from, to);
        const float scale = std::sqrt((1.0f + cosine) * 2.0f);
        const float inverseScale = 1.0f / scale;
        return glm::normalize(glm::quat(
            scale * 0.5f,
            axis.x * inverseScale,
            axis.y * inverseScale,
            axis.z * inverseScale));
    }
}

struct Mapa1::Impl {
    Shader shader;
    AudioPlayer backgroundMusic{ L"mapa1_background_music" };
    AudioPlayer parrySound{ L"mapa1_parry_sound" };
    std::vector<WorldModel> worldModels;
    std::vector<std::string> deferredWorldModelPaths;
    WorldModel vanModel;
    WorldModel gemModel;
    std::unordered_map<std::string, std::shared_ptr<Texture2D>> textureCache;
    Texture2D playerAtlasTexture;
    LoadedModel projectileSwordModel;
    Texture2D skyboxTexture;
    Texture2D coinIconTexture;
    Texture2D platformSideTexture;
    Texture2D platformTopTexture;
    Texture2D enemyTexture;
    Mesh playerIdleMesh;
    Mesh playerGuardGroundMesh;
    Mesh playerGuardAirMesh;
    Mesh skyboxMesh;
    Mesh pipeBodyMesh;
    Mesh pipeRimMesh;
    Mesh pipeOpeningMesh;
    Mesh platformMesh;
    Mesh coinMesh;
    Mesh starMesh;
    Mesh parryRingMesh;
    std::vector<Mesh> playerJumpMeshes;
    std::vector<Mesh> playerRunMeshes;
    std::vector<Mesh> playerEntranceMeshes;
    std::vector<Mesh> playerDeathMeshes;
    std::vector<Mesh> playerTransitionMeshes;
    std::vector<Mesh> enemyIdleMeshes;
    std::vector<Mesh> enemyRunMeshes;
    std::vector<Mesh> enemyAttackMeshes;
    std::vector<Mesh> enemyHurtMeshes;
    std::vector<Mesh> enemyDeathMeshes;
    std::vector<TerrainTriangle> terrainTriangles;
    std::vector<ExtraPlatform> extraPlatforms = {
        {PlatformShape::Ellipse, 0.60f, 1.40f, -0.40f, 0.40f, 0.18f, 0.24f, {0.36f, 0.72f, 0.32f, 1.0f}, "tuberia", false, false},
        {PlatformShape::Rectangle, -10.50f, -9.15f, 4.00f, 5.35f, -0.18f, 0.24f, {0.30f, 0.72f, 0.42f, 1.0f}, "oeste_01", true, false},
        {PlatformShape::Rectangle, -8.85f, -7.35f, 4.65f, 6.15f, 0.30f, 0.26f, {0.34f, 0.78f, 0.46f, 1.0f}, "oeste_02", true, false},
        {PlatformShape::Rectangle, -3.55f, -2.20f, -6.30f, -4.95f, -0.16f, 0.24f, {0.32f, 0.70f, 0.40f, 1.0f}, "sur_01", true, false},
        {PlatformShape::Rectangle, -1.90f, -0.40f, -5.70f, -4.20f, 0.34f, 0.26f, {0.38f, 0.80f, 0.48f, 1.0f}, "sur_02", true, false},
        {PlatformShape::Rectangle, 5.00f, 6.35f, 4.40f, 5.75f, -0.14f, 0.24f, {0.30f, 0.72f, 0.42f, 1.0f}, "norte_01", true, false},
        {PlatformShape::Rectangle, 6.70f, 8.20f, 5.00f, 6.50f, 0.38f, 0.26f, {0.38f, 0.82f, 0.50f, 1.0f}, "norte_02", true, false},
        {PlatformShape::Rectangle, 11.30f, 12.85f, -8.40f, -6.90f, 0.95f, 0.28f, {0.42f, 0.72f, 0.84f, 1.0f}, "isla_entrada", true, true},
        {PlatformShape::Rectangle, 13.05f, 14.25f, 5.75f, 7.05f, 1.45f, 0.24f, {0.28f, 0.64f, 0.92f, 1.0f}, "isla_salto_01", true, true},
        {PlatformShape::Rectangle, 14.95f, 16.15f, -9.35f, -8.05f, 1.95f, 0.24f, {0.30f, 0.72f, 0.98f, 1.0f}, "isla_salto_02", true, true},
        {PlatformShape::Rectangle, 16.75f, 18.05f, 8.10f, 9.45f, 2.45f, 0.24f, {0.36f, 0.78f, 1.00f, 1.0f}, "isla_salto_03", true, true},
        {PlatformShape::Rectangle, 18.70f, 20.20f, -10.50f, -8.90f, 2.90f, 0.28f, {0.46f, 0.84f, 1.00f, 1.0f}, "isla_ancla_01", true, true},
        {PlatformShape::Rectangle, 23.65f, 25.25f, 7.90f, 9.55f, 3.05f, 0.28f, {0.18f, 0.68f, 1.00f, 1.0f}, "isla_espectral_01", true, true},
        {PlatformShape::Rectangle, 31.80f, 34.20f, -12.20f, -10.10f, 3.22f, 0.30f, {0.70f, 0.88f, 1.00f, 1.0f}, "isla_espectral_final", true, true},
        {PlatformShape::Rectangle, -37.00f, -31.90f, -1.40f, 1.60f, 0.42f, 0.22f, {0.36f, 0.64f, 0.42f, 1.0f}, "van_camino_mapa", true, true},
        {PlatformShape::Rectangle, -40.80f, -38.00f, -1.50f, 1.50f, 0.58f, 0.22f, {0.40f, 0.68f, 0.48f, 1.0f}, "van_camino_01", true, true},
        {PlatformShape::Rectangle, -44.60f, -41.80f, -1.50f, 1.50f, 0.76f, 0.22f, {0.44f, 0.72f, 0.54f, 1.0f}, "van_camino_02", true, true},
        {PlatformShape::Rectangle, -48.40f, -45.60f, -1.50f, 1.50f, 0.94f, 0.22f, {0.48f, 0.76f, 0.60f, 1.0f}, "van_camino_03", true, true},
        {PlatformShape::Rectangle, -56.25f, -49.25f, -3.60f, 3.60f, 1.10f, 0.30f, {0.58f, 0.78f, 0.62f, 1.0f}, "isla_camioneta", true, true}
    };
    std::vector<Coin> coins;
    Star star;
    std::vector<SpectralAnchor> spectralAnchors;
    std::vector<DemonEnemy> enemies;
    std::vector<Projectile> projectiles;
    std::vector<DroppedGem> droppedGems;

    bool initialized{ false };
    bool vanModelLoaded{ false };
    bool vanModelLoadAttempted{ false };
    bool gemModelLoaded{ false };
    bool backgroundMusicOpen{ false };
    bool backgroundMusicPlaying{ false };
    bool parrySoundOpen{ false };
    bool mode3D{ false };
    bool tabPressed{ false };
    bool mouseAttackPressed{ false };
    bool chargingPlayerAttack{ false };
    bool parryPressed{ false };
    bool interactPressed{ false };
    bool shopTogglePressed{ false };
    bool resetEnemiesRequested{ false };
    bool clearProjectilesRequested{ false };
    float posX{ 0.0f };
    float posY{ 0.0f };
    float posZ{ 0.0f };
    float velocityY{ 0.0f };
    bool grounded{ false };
    float playerAngle{ 0.0f };
    float cameraOffsetY{ 0.0f };
    int lives{ StartingLives };
    int playerHealth{ PlayerMaximumHealth };
    float messageTime{ 0.0f };
    std::string statusMessage;
    float titleTime{ 0.0f };
    int currentFrame{ 0 };
    float animationTime{ 0.0f };
    float playerEntranceTime{ 0.0f };
    float playerTransitionTime{ 0.0f };
    float playerDeathTime{ 0.0f };
    float playerGuardUntil{ 0.0f };
    bool wasMoving{ false };
    bool playerEntrancePlaying{ false };
    bool playerTransitionPlaying{ false };
    bool playerDeathPlaying{ false };
    int collectedCoins{ 0 };
    int coinMessageCount{ 0 };
    float coinMessageUntil{ 0.0f };
    float starMessageUntil{ 0.0f };
    float combatHintUntil{ 0.0f };
    float playerAttackCooldown{ 0.0f };
    float playerChargeTime{ 0.0f };
    float playerInvulnerability{ 0.0f };
    float parryUntil{ 0.0f };
    float parryEffectUntil{ 0.0f };
    float spectralCooldown{ 0.0f };
    float spectralLockedHintUntil{ 0.0f };
    float spectralReadyHintUntil{ 0.0f };
    float spectralUnlockHintUntil{ 0.0f };
    float spectralEffectUntil{ 0.0f };
    glm::vec3 spectralEffectStart{ 0.0f };
    glm::vec3 spectralEffectEnd{ 0.0f };
    glm::vec3 playerAimDirection{ 1.0f, 0.0f, 0.0f };
    int spectralGems{ 0 };
    bool spectralStepUnlocked{ false };
    bool damageParryPurchased{ false };
    bool spectralKeyPressed{ false };
    bool vanShopOpen{ false };
    float vanPromptUntil{ 0.0f };
    float vanShopUntil{ 0.0f };
    bool completed{ false };
    float deferredWorldLoadDelay{ 0.0f };

    void initializeAudio() {
        backgroundMusicOpen = backgroundMusic.open(resolveAssetPath("assets/audio/devil-never-cry-compatible.mp3"));
        if (backgroundMusicOpen) {
            backgroundMusicPlaying = backgroundMusic.playLoop();
        }
        else {
            std::cerr << "Mapa 1 background music could not be started." << std::endl;
        }

        parrySoundOpen = parrySound.open(resolveAssetPath("assets/audio/royal-guard.mp3"));
        if (!parrySoundOpen) {
            std::cerr << "Mapa 1 parry sound could not be loaded." << std::endl;
        }
    }

    void shutdownAudio() {
        if (backgroundMusicOpen) {
            backgroundMusic.close();
        }
        if (parrySoundOpen) {
            parrySound.close();
        }
        backgroundMusicOpen = false;
        backgroundMusicPlaying = false;
        parrySoundOpen = false;
    }

    std::shared_ptr<Texture2D> textureFor(const std::string& path) {
        if (path.empty() || path.find('*') != std::string::npos) {
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

    std::vector<MapMaterial> runtimeMaterialsFor(const std::vector<LoadedMaterial>& materials) {
        std::vector<MapMaterial> runtimeMaterials;
        runtimeMaterials.reserve(materials.size());
        for (const LoadedMaterial& material : materials) {
            MapMaterial runtimeMaterial;
            runtimeMaterial.color = glm::vec4(material.diffuseColor, material.opacity);
            if (!material.embeddedTextureData.empty()) {
                auto embedded = std::make_shared<Texture2D>();
                bool loaded = false;
                if (material.embeddedTextureCompressed) {
                    loaded = embedded->loadFromMemory(
                        material.embeddedTextureData.data(),
                        static_cast<int>(material.embeddedTextureData.size()),
                        false);
                }
                else if (material.embeddedTextureWidth > 0 && material.embeddedTextureHeight > 0) {
                    embedded->createFromRGBA(
                        material.embeddedTextureWidth,
                        material.embeddedTextureHeight,
                        material.embeddedTextureData.data(),
                        false);
                    loaded = embedded->valid();
                }
                if (loaded) {
                    runtimeMaterial.texture = std::move(embedded);
                }
            }
            else {
                runtimeMaterial.texture = textureFor(material.diffuseTexturePath);
            }
            runtimeMaterials.push_back(std::move(runtimeMaterial));
        }
        return runtimeMaterials;
    }

    bool loadWorldModel(const std::string& path) {
        WorldModel world;
        world.model = ModelLoader::loadModel(resolveAssetPath(path));
        if (world.model.meshes.empty()) {
            return false;
        }

        world.materials = runtimeMaterialsFor(world.model.materials);

        worldModels.push_back(std::move(world));
        return true;
    }

    void streamDeferredWorldModels(float dt) {
        if (deferredWorldModelPaths.empty()) {
            return;
        }

        deferredWorldLoadDelay -= dt;
        if (deferredWorldLoadDelay > 0.0f) {
            return;
        }

        const std::string path = deferredWorldModelPaths.front();
        deferredWorldModelPaths.erase(deferredWorldModelPaths.begin());
        loadWorldModel(path);
        deferredWorldLoadDelay = 0.18f;
    }

    bool loadVanModel() {
        vanModelLoadAttempted = true;
        vanModel = {};
        vanModel.model = ModelLoader::loadModel(resolveAssetPath("assets/mapa1/extra/cabina_telefonica (1).glb"));
        if (vanModel.model.meshes.empty()) {
            std::cerr << "Mapa 1 phone booth model could not be loaded." << std::endl;
            vanModelLoaded = false;
            return false;
        }

        vanModel.materials = runtimeMaterialsFor(vanModel.model.materials);
        vanModelLoaded = true;
        return true;
    }

    bool ensureVanModelLoaded() {
        if (vanModelLoaded) {
            return true;
        }
        if (vanModelLoadAttempted) {
            return false;
        }
        return loadVanModel();
    }

    bool loadGemModel() {
        gemModel = {};
        gemModel.model = ModelLoader::loadModel(resolveAssetPath("assets/mapa1/extra/Untitled.glb"));
        if (gemModel.model.meshes.empty()) {
            std::cerr << "Mapa 1 red gem model could not be loaded." << std::endl;
            gemModelLoaded = false;
            return false;
        }

        gemModel.materials = runtimeMaterialsFor(gemModel.model.materials);
        gemModelLoaded = true;
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
                    const float minY = std::min({ a.y, b.y, c.y });
                    const float maxY = std::max({ a.y, b.y, c.y });
                    if (normalY < MinimumWalkableNormal || maxY < MinimumWalkableHeight) {
                        continue;
                    }

                    const float minX = std::min({ a.x, b.x, c.x });
                    const float maxX = std::max({ a.x, b.x, c.x });
                    const float minZ = std::min({ a.z, b.z, c.z });
                    const float maxZ = std::max({ a.z, b.z, c.z });
                    if (maxX - minX < 0.002f || maxZ - minZ < 0.002f) {
                        continue;
                    }

                    terrainTriangles.push_back({ a, b, c, minX, maxX, minZ, maxZ, &mesh.name });
                    terrainMin = glm::min(terrainMin, glm::min(glm::min(a, b), c));
                    terrainMax = glm::max(terrainMax, glm::max(glm::max(a, b), c));
                }
            }
        }

        std::cout << "Mapa 1 ready. Walkable terrain triangles: " << terrainTriangles.size() << std::endl;
        std::cout << "Mapa 1 terrain bounds: X[" << terrainMin.x << ", " << terrainMax.x
            << "] Z[" << terrainMin.z << ", " << terrainMax.z << "]" << std::endl;
        std::cout << "Mapa 1 parkour platforms: " << extraPlatforms.size() - 1 << std::endl;
        std::cout << "Mapa 1 controls: A/D and W jump in 2D, WASD and Space jump in 3D, mouse aims and shoots in 2D, hold mouse to charge, F parries, E interacts with the phone booth, B opens the remote shop, Q uses purchased Salto Espectral near blue anchors, Tab switches view, Esc returns to menu." << std::endl;
    }

    glm::vec3 platformTopCenter(size_t index) const {
        const ExtraPlatform& platform = extraPlatforms[index];
        return {
            (platform.minX + platform.maxX) * 0.5f,
            platform.height,
            (platform.minZ + platform.maxZ) * 0.5f
        };
    }

    float vanRouteCenterDepth() const {
        return platformTopCenter(VanIslandPlatformIndex).z;
    }

    bool playerNearVanRoute() const {
        const float maximumJumpHeight = (JumpSpeed * JumpSpeed) / (2.0f * Gravity);
        for (size_t i = VanRouteFirstPlatformIndex; i <= VanIslandPlatformIndex; ++i) {
            const ExtraPlatform& platform = extraPlatforms[i];
            const bool insideRoute = pointInPlatform(platform, posX, posZ, platform.projectIn2D && !mode3D);
            const bool closeHeight = posY >= platform.height - 0.55f &&
                posY <= platform.height + maximumJumpHeight + 0.45f;
            if (insideRoute && closeHeight) {
                return true;
            }
        }
        return false;
    }

    glm::vec3 spectralLandingPoint(size_t platformIndex) const {
        glm::vec3 point = platformTopCenter(platformIndex);
        point.y += 0.06f;
        return point;
    }

    glm::vec3 vanWorldPosition() const {
        return platformTopCenter(VanIslandPlatformIndex) + glm::vec3(0.0f, 0.10f, 0.0f);
    }

    glm::vec3 playerSpawnPosition() const {
        const glm::vec3 van = vanWorldPosition();
        return van + glm::vec3(PlayerSpawnDistanceFromVan, -0.10f, 0.0f);
    }

    void rebuildSpectralAnchors() {
        spectralAnchors.clear();
        if (extraPlatforms.size() <= FinalSpectralIslandIndex) {
            return;
        }

        spectralAnchors.push_back({
            platformTopCenter(SpectralAnchorPlatformIndex) + glm::vec3(0.0f, 0.70f, 0.0f),
            spectralLandingPoint(FirstSpectralIslandIndex),
            true,
            0.0f
            });
        spectralAnchors.push_back({
            platformTopCenter(FirstSpectralIslandIndex) + glm::vec3(0.0f, 0.70f, 0.0f),
            spectralLandingPoint(FinalSpectralIslandIndex),
            true,
            1.35f
            });
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
                        position = { x, height + 0.72f, z };
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool findTerrainEnemyPosition(const glm::vec2& anchor, glm::vec3& position) const {
        if (!findTerrainCoinPosition(anchor, position)) {
            return false;
        }

        position.y -= 0.72f;
        return true;
    }

    void resetEnemies() {
        enemies.clear();
        const std::array<glm::vec2, EnemySpawnCount> enemyAnchors = {
            glm::vec2(-27.0f, -7.0f),
            glm::vec2(-20.0f, 5.0f),
            glm::vec2(-14.0f, -2.0f),
            glm::vec2(-8.0f, 6.0f),
            glm::vec2(0.0f, 8.0f),
            glm::vec2(6.0f, -5.0f),
            glm::vec2(11.0f, 6.0f),
            glm::vec2(16.0f, -7.0f),
            glm::vec2(22.0f, 5.0f),
            glm::vec2(28.0f, -3.0f),
            glm::vec2(35.0f, 7.0f)
        };

        for (size_t i = 0; i < enemyAnchors.size(); ++i) {
            DemonEnemy enemy;
            if (!findTerrainEnemyPosition(enemyAnchors[i], enemy.position)) {
                std::cerr << "Mapa 1 could not place an enemy near X:" << enemyAnchors[i].x
                    << " Z:" << enemyAnchors[i].y << "." << std::endl;
                continue;
            }

            enemy.spawnPosition = enemy.position;
            enemy.shotCooldown = 0.55f + static_cast<float>(i) * 0.34f;
            enemy.animationTime = static_cast<float>(i) * 0.17f;
            enemies.push_back(enemy);
        }

        std::cout << "Mapa 1 Thalassa-style enemies: " << enemies.size() << std::endl;
    }

    void resetMission() {
        coins.clear();
        const std::array<size_t, 8> coinPlatforms = {
            0,
            2,
            4,
            6,
            8,
            10,
            FirstSpectralIslandIndex,
            FinalSpectralIslandIndex
        };
        for (size_t index : coinPlatforms) {
            Coin coin;
            coin.position = platformTopCenter(index) + glm::vec3(0.0f, 0.72f, 0.0f);
            coin.projectIn2D = extraPlatforms[index].projectIn2D;
            coin.phase = static_cast<float>(coins.size()) * 0.73f;
            coins.push_back(coin);
        }

        const std::array<glm::vec2, 2> terrainAnchors = {
            glm::vec2(-18.0f, -7.0f),
            glm::vec2(3.0f, -9.0f)
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
        star.position = platformTopCenter(FinalSpectralIslandIndex) + glm::vec3(0.0f, 0.95f, 0.0f);
        star.projectIn2D = true;
        rebuildSpectralAnchors();
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
        if (extraPlatforms.size() <= VanIslandPlatformIndex || spectralAnchors.size() < 2) {
            std::cerr << "Mapa 1 special routes are missing platforms or anchors." << std::endl;
            return false;
        }
        for (size_t i = VanRouteFirstPlatformIndex; i <= VanIslandPlatformIndex; ++i) {
            if (!extraPlatforms[i].visible || !extraPlatforms[i].projectIn2D) {
                std::cerr << "Mapa 1 van route must stay visible and projected so it works in 2D and 3D." << std::endl;
                return false;
            }
        }

        const float maximumJumpHeight = (JumpSpeed * JumpSpeed) / (2.0f * Gravity);
        const float maximumProjectedJumpDistance = MoveSpeed2D * (2.0f * JumpSpeed / Gravity);
        float maximumPerspectiveRise = 0.0f;
        float maximumProjectedGap = 0.0f;
        float maximumPhysicalGap = 0.0f;
        for (size_t i = NormalRouteFirstPlatformIndex; i <= SpectralAnchorPlatformIndex; ++i) {
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

        const ExtraPlatform& anchorIsland = extraPlatforms[SpectralAnchorPlatformIndex];
        const ExtraPlatform& firstSpectralIsland = extraPlatforms[FirstSpectralIslandIndex];
        const ExtraPlatform& finalSpectralIsland = extraPlatforms[FinalSpectralIslandIndex];
        const float firstSpectralGap = firstSpectralIsland.minX - anchorIsland.maxX;
        const float finalSpectralGap = finalSpectralIsland.minX - firstSpectralIsland.maxX;
        if (firstSpectralGap <= maximumProjectedJumpDistance + 1.0f ||
            finalSpectralGap <= maximumProjectedJumpDistance + 1.0f) {
            std::cerr << "Mapa 1 spectral islands must stay out of normal jump range." << std::endl;
            return false;
        }

        auto xRangeGap = [](const ExtraPlatform& a, const ExtraPlatform& b) {
            return std::max(0.0f, std::max(a.minX, b.minX) - std::min(a.maxX, b.maxX));
            };
        auto horizontalPlatformGap = [](const ExtraPlatform& a, const ExtraPlatform& b) {
            const float gapX = std::max(0.0f, std::max(a.minX, b.minX) - std::min(a.maxX, b.maxX));
            const float gapZ = std::max(0.0f, std::max(a.minZ, b.minZ) - std::min(a.maxZ, b.maxZ));
            return glm::length(glm::vec2(gapX, gapZ));
            };

        float maximumVanRouteRise = 0.0f;
        float maximumVanRouteProjectedGap = 0.0f;
        float maximumVanRoutePhysicalGap = 0.0f;
        float minimumVanRouteProjectedGap = std::numeric_limits<float>::max();
        float minimumVanRoutePhysicalGap = std::numeric_limits<float>::max();
        for (size_t i = VanRouteFirstPlatformIndex + 1; i <= VanIslandPlatformIndex; ++i) {
            const ExtraPlatform& previous = extraPlatforms[i - 1];
            const ExtraPlatform& current = extraPlatforms[i];
            const float projectedGap = xRangeGap(previous, current);
            const float physicalGap = horizontalPlatformGap(previous, current);
            maximumVanRouteRise = std::max(maximumVanRouteRise, current.height - previous.height);
            maximumVanRouteProjectedGap = std::max(maximumVanRouteProjectedGap, projectedGap);
            maximumVanRoutePhysicalGap = std::max(maximumVanRoutePhysicalGap, physicalGap);
            minimumVanRouteProjectedGap = std::min(minimumVanRouteProjectedGap, projectedGap);
            minimumVanRoutePhysicalGap = std::min(minimumVanRoutePhysicalGap, physicalGap);
        }

        if (maximumVanRouteRise > maximumJumpHeight - 0.08f ||
            maximumVanRouteProjectedGap > maximumProjectedJumpDistance - 0.08f ||
            maximumVanRoutePhysicalGap > maximumProjectedJumpDistance - 0.08f) {
            std::cerr << "Mapa 1 van route is unreachable after 2D projection." << std::endl;
            return false;
        }

        if (minimumVanRouteProjectedGap < 0.65f || minimumVanRoutePhysicalGap < 0.65f) {
            std::cerr << "Mapa 1 van route platforms are too close together." << std::endl;
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
            << " | spectral gaps: " << firstSpectralGap << ", " << finalSpectralGap
            << " | van route gap: " << minimumVanRouteProjectedGap << "-" << maximumVanRouteProjectedGap
            << "/" << minimumVanRoutePhysicalGap << "-" << maximumVanRoutePhysicalGap
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

    void resetSpectralProgress() {
        spectralGems = 0;
        spectralStepUnlocked = false;
        damageParryPurchased = false;
        spectralKeyPressed = false;
        interactPressed = false;
        shopTogglePressed = false;
        vanShopOpen = false;
        vanPromptUntil = 0.0f;
        vanShopUntil = 0.0f;
        droppedGems.clear();
        spectralCooldown = 0.0f;
        spectralLockedHintUntil = 0.0f;
        spectralReadyHintUntil = 0.0f;
        spectralUnlockHintUntil = 0.0f;
        spectralEffectUntil = 0.0f;
        spectralEffectStart = { 0.0f, 0.0f, 0.0f };
        spectralEffectEnd = { 0.0f, 0.0f, 0.0f };
    }

    void unlockSpectralStep(float now) {
        if (spectralStepUnlocked) {
            return;
        }

        spectralStepUnlocked = true;
        spectralUnlockHintUntil = now + 5.2f;
        spectralLockedHintUntil = 0.0f;
        showStatus("PASO ESPECTRAL COMPRADO - usa Q junto a un ancla azul");
    }

    void registerEnemyDefeat(const glm::vec3& position, float now) {
        droppedGems.push_back({
            position + glm::vec3(0.0f, 0.42f, 0.0f),
            now * 1.73f + static_cast<float>(droppedGems.size()) * 0.91f,
            true
            });
        showStatus("GEMA ROJA LIBERADA - acercate para recogerla");
    }

    void updateDroppedGems() {
        for (size_t index = 0; index < droppedGems.size();) {
            const DroppedGem& gem = droppedGems[index];
            if (!touchesCollectible(gem.position, GemPickupRadius, GemPickupVerticalRange, gem.projectIn2D)) {
                ++index;
                continue;
            }

            spectralGems = std::min(MaximumStoredGems, spectralGems + 1);
            droppedGems.erase(droppedGems.begin() + static_cast<std::ptrdiff_t>(index));
            showStatus("GEMA ROJA OBTENIDA - saldo: " + std::to_string(spectralGems));
        }
    }

    bool purchaseSpectralStep() {
        if (spectralStepUnlocked) {
            showStatus("SALTO ESPECTRAL YA ADQUIRIDO");
            return false;
        }
        if (spectralGems < SpectralGemRequirement) {
            showStatus("NO HAY SUFICIENTES GEMAS ROJAS");
            return false;
        }

        spectralGems -= SpectralGemRequirement;
        unlockSpectralStep(static_cast<float>(glfwGetTime()));
        return true;
    }

    bool purchaseDamageParry() {
        if (damageParryPurchased) {
            showStatus("RETORNO REAL YA ADQUIRIDO");
            return false;
        }
        if (spectralGems < DamageParryGemRequirement) {
            showStatus("NO HAY SUFICIENTES GEMAS ROJAS");
            return false;
        }

        spectralGems -= DamageParryGemRequirement;
        damageParryPurchased = true;
        showStatus("RETORNO REAL COMPRADO - el parry devuelve dano");
        return true;
    }

    bool nearVanShop() const {
        const glm::vec3 van = vanWorldPosition() + glm::vec3(0.0f, 0.55f, 0.0f);
        const glm::vec3 playerCenter(posX, posY + 0.58f, posZ);
        const bool closeX = std::abs(playerCenter.x - van.x) <= VanPromptRange;
        const bool closeY = std::abs(playerCenter.y - van.y) <= VanPromptVerticalRange;
        const bool closeZ = !mode3D || std::abs(playerCenter.z - van.z) <= VanPromptRange;
        return closeX && closeY && closeZ;
    }

    void handleVanShop(GLFWwindow* window, float now) {
        const bool interactDown = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
        const bool toggleDown = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
        const bool nearby = nearVanShop();
        if (nearby) {
            vanPromptUntil = std::max(vanPromptUntil, now + 0.20f);
        }

        if (toggleDown && !shopTogglePressed) {
            vanShopOpen = !vanShopOpen;
            stopChargingPlayerAttack();
            showStatus(vanShopOpen ? "TIENDA REMOTA ABIERTA" : "TIENDA CERRADA");
        }

        if (interactDown && !interactPressed && nearby) {
            vanShopOpen = true;
            stopChargingPlayerAttack();
            showStatus("CONEXION CON LA CABINA ESTABLECIDA");
        }
        interactPressed = interactDown;
        shopTogglePressed = toggleDown;
    }

    int nearestSpectralAnchorIndex() const {
        const glm::vec3 playerCenter(posX, posY + 0.58f, posZ);
        for (size_t index = 0; index < spectralAnchors.size(); ++index) {
            const SpectralAnchor& anchor = spectralAnchors[index];
            const bool closeX = std::abs(playerCenter.x - anchor.origin.x) <= SpectralAnchorPromptRange;
            const bool closeY = std::abs(playerCenter.y - anchor.origin.y) <= SpectralAnchorVerticalRange;
            const bool closeZ = (anchor.projectIn2D && !mode3D) ||
                std::abs(playerCenter.z - anchor.origin.z) <= SpectralAnchorPromptRange;
            if (closeX && closeY && closeZ) {
                return static_cast<int>(index);
            }
        }
        return -1;
    }

    void performSpectralStep(const SpectralAnchor& anchor, float now) {
        stopChargingPlayerAttack();
        spectralCooldown = SpectralStepCooldown;
        spectralReadyHintUntil = 0.0f;
        spectralEffectUntil = now + 0.46f;
        spectralEffectStart = { posX, posY + 0.58f, posZ };
        spectralEffectEnd = anchor.target + glm::vec3(0.0f, 0.58f, 0.0f);

        const float previousX = posX;
        posX = anchor.target.x;
        posY = anchor.target.y;
        posZ = anchor.target.z;
        velocityY = 0.0f;
        grounded = true;
        playerAngle = anchor.target.x >= previousX ? 0.0f : 180.0f;
        startModeTransitionAnimation();
        showStatus("PASO ESPECTRAL");
    }

    void handleSpectralStep(GLFWwindow* window, float now) {
        const bool spectralDown = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
        const int anchorIndex = nearestSpectralAnchorIndex();
        if (anchorIndex >= 0) {
            if (spectralStepUnlocked) {
                spectralReadyHintUntil = std::max(spectralReadyHintUntil, now + 0.20f);
            }
            else {
                spectralLockedHintUntil = std::max(spectralLockedHintUntil, now + 0.20f);
            }
        }

        if (spectralDown && !spectralKeyPressed && anchorIndex >= 0) {
            if (!spectralStepUnlocked) {
                spectralLockedHintUntil = std::max(spectralLockedHintUntil, now + SpectralHintTime);
                showStatus("COMPRA SALTO ESPECTRAL EN LA TIENDA CON 5 GEMAS");
            }
            else if (spectralCooldown <= 0.0f) {
                performSpectralStep(spectralAnchors[static_cast<size_t>(anchorIndex)], now);
            }
        }

        spectralKeyPressed = spectralDown;
    }

    void startEntranceAnimation() {
        playerEntrancePlaying = true;
        playerEntranceTime = 0.0f;
        playerTransitionPlaying = false;
        playerTransitionTime = 0.0f;
        playerDeathPlaying = false;
        playerDeathTime = 0.0f;
        playerGuardUntil = 0.0f;
        animationTime = 0.0f;
    }

    void startModeTransitionAnimation() {
        playerTransitionPlaying = true;
        playerTransitionTime = 0.0f;
        playerEntrancePlaying = false;
        playerGuardUntil = 0.0f;
    }

    void beginPlayerDeath(float now) {
        if (playerDeathPlaying) {
            return;
        }

        playerDeathPlaying = true;
        playerDeathTime = 0.0f;
        playerEntrancePlaying = false;
        playerTransitionPlaying = false;
        playerGuardUntil = 0.0f;
        stopChargingPlayerAttack();
        clearProjectilesRequested = true;
        playerInvulnerability = std::max(playerInvulnerability, animationDuration(playerDeathMeshes, PlayerDeathFramesPerSecond));
        showStatus("DANO LETAL");
        if (mode3D) {
            showCombatRestriction(now);
        }
    }

    void finishPlayerDeath() {
        playerDeathPlaying = false;
        playerDeathTime = 0.0f;
        loseLife();
    }

    void resetPlayer(bool resetMode) {
        if (resetMode) {
            mode3D = false;
        }

        const glm::vec3 spawn = playerSpawnPosition();
        posX = spawn.x;
        posZ = spawn.z;
        velocityY = 0.0f;
        cameraOffsetY = 0.0f;
        playerAngle = 0.0f;

        float initialHeight = spawn.y;
        posY = groundHeight(posX, posZ, initialHeight) ? initialHeight : spawn.y;
        grounded = true;
        animationTime = 0.0f;
        wasMoving = false;
        playerTransitionPlaying = false;
        playerTransitionTime = 0.0f;
        playerDeathPlaying = false;
        playerDeathTime = 0.0f;
        playerGuardUntil = 0.0f;
        if (resetMode) {
            startEntranceAnimation();
        }
    }

    void loseLife() {
        playerInvulnerability = PlayerInvulnerabilityTime;
        clearProjectilesRequested = true;
        playerHealth = PlayerMaximumHealth;
        --lives;
        if (lives <= 0) {
            lives = StartingLives;
            resetPlayer(true);
            resetMission();
            resetSpectralProgress();
            resetEnemiesRequested = true;
            showStatus("GAME OVER - se reinicio el mapa. Vidas: 3");
            return;
        }

        resetPlayer(false);
        showStatus("MORISTE - vidas restantes: " + std::to_string(lives));
    }

    float gameplayDistance(const glm::vec3& from, const glm::vec3& to) const {
        if (!mode3D) {
            return std::abs(to.x - from.x);
        }

        return glm::length(glm::vec2(to.x - from.x, to.z - from.z));
    }

    bool touchesGameplayTarget(const glm::vec3& from, const glm::vec3& to, float horizontalRadius, float verticalRadius) const {
        return gameplayDistance(from, to) <= horizontalRadius &&
            std::abs(from.y - to.y) <= verticalRadius;
    }

    bool tryMoveEnemy(DemonEnemy& enemy, const glm::vec2& step) const {
        if (glm::length(step) < 0.00001f) {
            return false;
        }

        const float nextX = enemy.position.x + step.x;
        const float nextZ = enemy.position.z + step.y;
        float nextHeight = -100.0f;
        if (!terrainHeight(nextX, nextZ, nextHeight) ||
            std::abs(nextHeight - enemy.position.y) > EnemyMaximumStepHeight) {
            return false;
        }

        enemy.position = { nextX, nextHeight, nextZ };
        return true;
    }

    void showCombatRestriction(float now) {
        combatHintUntil = std::max(combatHintUntil, now + 3.8f);
        showStatus("CAMBIA A 2D CON TAB PARA DETENER A LOS ENEMIGOS");
    }

    void spawnEnemyProjectile(const DemonEnemy& enemy, float now) {
        glm::vec3 origin = enemy.position + glm::vec3(0.0f, 0.58f, 0.0f);
        glm::vec3 target(posX, posY + 0.58f, posZ);
        if (!mode3D) {
            target.z = origin.z;
        }

        glm::vec3 direction = target - origin;
        if (glm::length(direction) < 0.0001f) {
            direction = { enemy.facingLeft ? -1.0f : 1.0f, 0.0f, 0.0f };
        }
        else {
            direction = glm::normalize(direction);
        }

        projectiles.push_back({ origin, direction * EnemyShotSpeed, ProjectileLifetime, false });
        if (mode3D) {
            showCombatRestriction(now);
        }
    }

    glm::vec3 mouseAimDirection(GLFWwindow* window) const {
        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        if (width <= 0 || height <= 0) {
            return playerAimDirection;
        }

        double mouseX = 0.0;
        double mouseY = 0.0;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        const glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::translate(glm::mat4(1.0f), { 0.0f, -cameraOffsetY, -3.0f });
        const glm::vec4 viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
        const glm::vec3 nearPoint = glm::unProject(
            glm::vec3(static_cast<float>(mouseX), static_cast<float>(height) - static_cast<float>(mouseY), 0.0f),
            view,
            projection,
            viewport);
        const glm::vec3 farPoint = glm::unProject(
            glm::vec3(static_cast<float>(mouseX), static_cast<float>(height) - static_cast<float>(mouseY), 1.0f),
            view,
            projection,
            viewport);
        const glm::vec3 ray = farPoint - nearPoint;
        const float planeZ = 0.22f;
        const float amount = std::abs(ray.z) > 0.0001f ? (planeZ - nearPoint.z) / ray.z : 0.0f;
        const glm::vec3 cursorPosition = nearPoint + ray * amount;
        glm::vec3 direction = cursorPosition - glm::vec3(0.0f, posY + 0.62f, planeZ);
        direction.z = 0.0f;
        if (glm::length(direction) < 0.05f) {
            return playerAimDirection;
        }
        return glm::normalize(direction);
    }

    void spawnPlayerProjectile(const glm::vec3& direction, bool charged) {
        const float speed = charged ? ChargedPlayerShotSpeed : PlayerShotSpeed;
        projectiles.push_back({
            {posX + direction.x * 0.48f, posY + 0.62f + direction.y * 0.28f, posZ},
            direction * speed,
            ProjectileLifetime,
            true,
            charged ? EnemyMaximumHealth : 1,
            charged
            });
    }

    void damagePlayer(float now) {
        if (playerInvulnerability > 0.0f) {
            return;
        }

        const bool wasMode3D = mode3D;
        playerInvulnerability = PlayerInvulnerabilityTime;
        playerHealth = std::max(0, playerHealth - 1);
        if (playerHealth <= 0) {
            beginPlayerDeath(now);
        }
        else {
            showStatus("DANO RECIBIDO - salud: " + std::to_string(playerHealth) + "/" + std::to_string(PlayerMaximumHealth));
            if (wasMode3D) {
                showCombatRestriction(now);
            }
        }
    }

    void updateEnemies(float dt, float now) {
        const glm::vec3 playerCenter(posX, posY + 0.58f, posZ);
        for (DemonEnemy& enemy : enemies) {
            enemy.animationTime += dt;
            if (!enemy.alive) {
                enemy.state = EnemyState::Dying;
                continue;
            }

            enemy.shotCooldown = std::max(0.0f, enemy.shotCooldown - dt);
            enemy.hurtTime = std::max(0.0f, enemy.hurtTime - dt);
            const glm::vec3 enemyCenter = enemy.position + glm::vec3(0.0f, 0.48f, 0.0f);
            const float distance = gameplayDistance(enemyCenter, playerCenter);
            if (distance > EnemyDetectionRange) {
                enemy.state = EnemyState::Idle;
                continue;
            }

            const glm::vec2 rawDirection(
                posX - enemy.position.x,
                mode3D ? posZ - enemy.position.z : 0.0f);
            if (glm::length(rawDirection) > 0.0001f) {
                const glm::vec2 direction = glm::normalize(rawDirection);
                enemy.facingLeft = direction.x < 0.0f;

                if (distance > EnemyStopDistance) {
                    const glm::vec2 step = direction * EnemyMoveSpeed * dt;
                    if (!tryMoveEnemy(enemy, step)) {
                        tryMoveEnemy(enemy, { step.x, 0.0f });
                        if (mode3D) {
                            tryMoveEnemy(enemy, { 0.0f, step.y });
                        }
                    }
                }
            }

            enemy.state = enemy.hurtTime > 0.0f
                ? EnemyState::Hurt
                : (enemy.shotCooldown > EnemyShotCooldown - 0.55f
                    ? EnemyState::Attack
                    : EnemyState::Chase);
            if (distance <= EnemyAttackRange && enemy.shotCooldown <= 0.0f) {
                enemy.state = EnemyState::Attack;
                enemy.animationTime = 0.0f;
                enemy.shotCooldown = EnemyShotCooldown;
                spawnEnemyProjectile(enemy, now);
            }

            const glm::vec3 movedEnemyCenter = enemy.position + glm::vec3(0.0f, 0.48f, 0.0f);
            if (touchesGameplayTarget(movedEnemyCenter, playerCenter, 0.60f, 0.82f)) {
                damagePlayer(now);
            }
        }
    }

    int remainingEnemyCount() const {
        return static_cast<int>(std::count_if(enemies.begin(), enemies.end(), [](const DemonEnemy& enemy) {
            return enemy.alive;
            }));
    }

    bool parryEnemyProjectile(Projectile& projectile, float now) {
        if (parryUntil <= 0.0f || now > parryUntil) {
            return false;
        }

        parryUntil = 0.0f;
        parryEffectUntil = now + ParryEffectTime;
        playerGuardUntil = now + std::max(ParryEffectTime, 0.42f);
        if (parrySoundOpen) {
            parrySound.playOnce();
        }

        if (damageParryPurchased) {
            glm::vec3 targetDirection(playerAngle == 180.0f ? -1.0f : 1.0f, 0.0f, 0.0f);
            float nearestDistance = std::numeric_limits<float>::max();
            const glm::vec3 playerCenter(posX, posY + 0.58f, posZ);
            for (const DemonEnemy& enemy : enemies) {
                if (!enemy.alive) {
                    continue;
                }
                const glm::vec3 enemyCenter = enemy.position + glm::vec3(0.0f, 0.48f, 0.0f);
                const float distance = gameplayDistance(playerCenter, enemyCenter);
                if (distance < nearestDistance) {
                    nearestDistance = distance;
                    targetDirection = enemyCenter - playerCenter;
                    if (!mode3D) {
                        targetDirection.z = 0.0f;
                    }
                }
            }
            if (glm::length(targetDirection) < 0.001f) {
                targetDirection = { playerAngle == 180.0f ? -1.0f : 1.0f, 0.0f, 0.0f };
            }

            targetDirection = glm::normalize(targetDirection);
            projectile.fromPlayer = true;
            projectile.damage = EnemyMaximumHealth;
            projectile.charged = true;
            projectile.velocity = targetDirection * ChargedPlayerShotSpeed;
            projectile.position = playerCenter + targetDirection * 0.48f;
            projectile.lifetime = 2.8f;
            showStatus("RETORNO REAL - DANO DEVUELTO");
        }
        else {
            showStatus("ROYAL GUARD - PARRY PERFECTO");
        }
        return true;
    }

    void updateProjectiles(float dt, float now) {
        for (size_t i = 0; i < projectiles.size();) {
            Projectile& projectile = projectiles[i];
            projectile.position += projectile.velocity * dt;
            projectile.lifetime -= dt;
            bool remove = projectile.lifetime <= 0.0f;

            float terrain = -100.0f;
            if (!remove && terrainHeight(projectile.position.x, projectile.position.z, terrain) &&
                projectile.position.y <= terrain + 0.08f) {
                remove = true;
            }

            if (!remove && projectile.fromPlayer) {
                for (DemonEnemy& enemy : enemies) {
                    if (!enemy.alive) {
                        continue;
                    }

                    const glm::vec3 enemyCenter = enemy.position + glm::vec3(0.0f, 0.48f, 0.0f);
                    if (touchesGameplayTarget(projectile.position, enemyCenter, 0.58f, 0.72f)) {
                        enemy.health = std::max(0, enemy.health - projectile.damage);
                        enemy.animationTime = 0.0f;
                        if (enemy.health <= 0) {
                            enemy.alive = false;
                            enemy.state = EnemyState::Dying;
                            registerEnemyDefeat(enemy.position, now);
                        }
                        else {
                            enemy.state = EnemyState::Hurt;
                            enemy.hurtTime = 0.36f;
                            showStatus("GOLPE AL ENEMIGO - resistencia: " + std::to_string(enemy.health) + "/" + std::to_string(EnemyMaximumHealth));
                        }
                        remove = true;
                        break;
                    }
                }
            }
            else if (!remove) {
                const glm::vec3 playerCenter(posX, posY + 0.58f, posZ);
                if (touchesGameplayTarget(projectile.position, playerCenter, 0.34f, 0.58f)) {
                    const bool parried = parryEnemyProjectile(projectile, now);
                    if (!parried) {
                        damagePlayer(now);
                    }
                    remove = !parried || !projectile.fromPlayer;
                }
            }

            if (remove) {
                projectiles.erase(projectiles.begin() + static_cast<std::ptrdiff_t>(i));
            }
            else {
                ++i;
            }
        }
    }

    void tryPlayerAttack(const glm::vec3& direction, bool charged, float now) {
        playerAttackCooldown = PlayerAttackCooldown;
        if (mode3D) {
            showCombatRestriction(now);
        }
        else {
            spawnPlayerProjectile(direction, charged);
        }
    }

    void stopChargingPlayerAttack() {
        chargingPlayerAttack = false;
        playerChargeTime = 0.0f;
    }

    void handlePlayerAttack(GLFWwindow* window, float dt, float now) {
        const bool attackDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        if (!mode3D) {
            playerAimDirection = mouseAimDirection(window);
            if (playerAimDirection.x < -0.05f) {
                playerAngle = 180.0f;
            }
            else if (playerAimDirection.x > 0.05f) {
                playerAngle = 0.0f;
            }
        }

        if (mode3D) {
            if (attackDown && !mouseAttackPressed) {
                showCombatRestriction(now);
            }
            stopChargingPlayerAttack();
        }
        else {
            if (attackDown && !mouseAttackPressed && playerAttackCooldown <= 0.0f) {
                chargingPlayerAttack = true;
                playerChargeTime = 0.0f;
            }
            if (attackDown && chargingPlayerAttack) {
                playerChargeTime = std::min(PlayerChargeTime, playerChargeTime + dt);
            }
            if (!attackDown && mouseAttackPressed && chargingPlayerAttack) {
                tryPlayerAttack(playerAimDirection, playerChargeTime >= PlayerChargeTime, now);
                stopChargingPlayerAttack();
            }
        }
        mouseAttackPressed = attackDown;
    }

    void handleParry(GLFWwindow* window, float now) {
        const bool parryDown = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
        if (parryDown && !parryPressed) {
            parryUntil = now + ParryWindow;
        }
        parryPressed = parryDown;
    }

    bool runCombatSmokeTest() {
        if (enemies.size() < 3) {
            std::cerr << "Mapa 1 combat smoke test needs at least 3 enemies." << std::endl;
            return false;
        }

        glm::vec3 nearbyPosition;
        if (!findTerrainEnemyPosition({ 2.0f, 0.0f }, nearbyPosition)) {
            std::cerr << "Mapa 1 combat smoke test could not find nearby terrain." << std::endl;
            return false;
        }
        auto placePlayerForCombatTest = [&]() {
            posX = nearbyPosition.x - 2.0f;
            posZ = nearbyPosition.z;
            float playerTestHeight = nearbyPosition.y;
            posY = groundHeight(posX, posZ, playerTestHeight) ? playerTestHeight : nearbyPosition.y;
            velocityY = 0.0f;
            grounded = true;
            };
        placePlayerForCombatTest();

        mode3D = false;
        projectiles.clear();
        lives = StartingLives;
        playerHealth = PlayerMaximumHealth;
        playerInvulnerability = 0.0f;
        enemies[0].position = nearbyPosition;
        enemies[0].alive = true;
        enemies[0].health = EnemyMaximumHealth;
        enemies[0].shotCooldown = 0.0f;
        updateEnemies(0.0f, 0.0f);
        const bool enemyAttacksIn2D = std::any_of(projectiles.begin(), projectiles.end(), [](const Projectile& projectile) {
            return !projectile.fromPlayer;
            });

        projectiles.clear();
        playerHealth = PlayerMaximumHealth;
        lives = StartingLives;
        playerInvulnerability = 0.0f;
        playerDeathPlaying = false;
        playerDeathTime = 0.0f;
        auto hitPlayer = [&](float now) {
            playerInvulnerability = 0.0f;
            projectiles.push_back({
                {posX, posY + 0.58f, posZ},
                {},
                ProjectileLifetime,
                false,
                1,
                false
                });
            updateProjectiles(0.0f, now);
            };
        hitPlayer(1.0f);
        const int healthAfterFirstHit = playerHealth;
        const int livesAfterFirstHit = lives;
        const bool firstHitLeavesTwoHealth = healthAfterFirstHit == 2 && livesAfterFirstHit == StartingLives;
        hitPlayer(2.0f);
        const int healthAfterSecondHit = playerHealth;
        const int livesAfterSecondHit = lives;
        const bool secondHitLeavesOneHealth = healthAfterSecondHit == 1 && livesAfterSecondHit == StartingLives;
        hitPlayer(3.0f);
        const bool thirdHitStartsDeathAnimation =
            playerHealth == 0 &&
            playerDeathPlaying &&
            lives == StartingLives;
        finishPlayerDeath();
        const bool thirdHitCostsOneLife =
            playerHealth == PlayerMaximumHealth &&
            lives == StartingLives - 1 &&
            !playerDeathPlaying;
        placePlayerForCombatTest();

        projectiles.clear();
        clearProjectilesRequested = false;
        playerHealth = PlayerMaximumHealth;
        parryUntil = 4.50f;
        parryEffectUntil = 0.0f;
        const bool parryActivationStaysQuiet = parryEffectUntil <= 0.0f;
        hitPlayer(4.0f);
        const bool perfectParryShowsEffect = parryEffectUntil > 4.0f;
        const bool parryBlocksDamage =
            playerHealth == PlayerMaximumHealth &&
            parryUntil == 0.0f &&
            parryActivationStaysQuiet &&
            perfectParryShowsEffect;

        projectiles.clear();
        auto hitEnemy = [&](DemonEnemy& enemy, int damage, bool charged, float now) {
            projectiles.push_back({
                enemy.position + glm::vec3(0.0f, 0.48f, 0.0f),
                {},
                ProjectileLifetime,
                true,
                damage,
                charged
                });
            updateProjectiles(0.0f, now);
            };
        enemies[0].position = nearbyPosition;
        enemies[0].alive = true;
        enemies[0].health = EnemyMaximumHealth;
        hitEnemy(enemies[0], 1, false, 5.0f);
        const bool enemySurvivesFirstNormalHit = enemies[0].alive && enemies[0].health == 2;
        hitEnemy(enemies[0], 1, false, 5.1f);
        const bool enemySurvivesSecondNormalHit = enemies[0].alive && enemies[0].health == 1;
        hitEnemy(enemies[0], 1, false, 5.2f);
        const bool thirdNormalHitDefeatsEnemy = !enemies[0].alive;

        enemies[1].position = nearbyPosition;
        enemies[1].alive = true;
        enemies[1].health = EnemyMaximumHealth;
        hitEnemy(enemies[1], EnemyMaximumHealth, true, 5.3f);
        const bool chargedShotDefeatsEnemy = !enemies[1].alive;

        mode3D = true;
        projectiles.clear();
        combatHintUntil = 0.0f;
        playerAttackCooldown = 0.0f;
        tryPlayerAttack({ 1.0f, 0.0f, 0.0f }, false, 6.0f);
        const bool playerAttackBlockedIn3D = projectiles.empty() && combatHintUntil > 6.0f;

        projectiles.clear();
        combatHintUntil = 0.0f;
        enemies[2].position = nearbyPosition;
        enemies[2].alive = true;
        enemies[2].health = EnemyMaximumHealth;
        enemies[2].shotCooldown = 0.0f;
        updateEnemies(0.0f, 7.0f);
        const bool enemyStillAttacksIn3D =
            combatHintUntil > 7.0f &&
            std::any_of(projectiles.begin(), projectiles.end(), [](const Projectile& projectile) {
            return !projectile.fromPlayer;
                });

        mode3D = false;
        const size_t droppedBeforePickup = droppedGems.size();
        const int gemsBeforePickup = spectralGems;
        if (!droppedGems.empty()) {
            posX = droppedGems.front().position.x;
            posY = droppedGems.front().position.y - 0.42f;
            posZ = droppedGems.front().position.z;
            updateDroppedGems();
        }
        const bool defeatedEnemyDropsCollectibleGem =
            droppedBeforePickup > 0 &&
            droppedGems.size() < droppedBeforePickup &&
            spectralGems > gemsBeforePickup;

        spectralGems = SpectralGemRequirement + DamageParryGemRequirement;
        spectralStepUnlocked = false;
        damageParryPurchased = false;
        const bool spectralPurchaseWorks = purchaseSpectralStep() &&
            spectralStepUnlocked &&
            spectralGems == DamageParryGemRequirement;
        const bool parryPurchaseWorks = purchaseDamageParry() &&
            damageParryPurchased &&
            spectralGems == 0;

        enemies[0].position = nearbyPosition;
        enemies[0].alive = true;
        enemies[0].health = EnemyMaximumHealth;
        projectiles.clear();
        placePlayerForCombatTest();
        parryUntil = 9.5f;
        projectiles.push_back({
            {posX, posY + 0.58f, posZ},
            {},
            ProjectileLifetime,
            false,
            1,
            false
            });
        updateProjectiles(0.0f, 9.0f);
        const bool projectileReflected =
            projectiles.size() == 1 &&
            projectiles.front().fromPlayer &&
            projectiles.front().damage == EnemyMaximumHealth;
        if (projectileReflected) {
            projectiles.front().position = enemies[0].position + glm::vec3(0.0f, 0.48f, 0.0f);
            updateProjectiles(0.0f, 9.1f);
        }
        const bool reflectedParryDealsDamage = projectileReflected && !enemies[0].alive;

        vanShopOpen = false;
        vanShopOpen = true;
        const bool remoteShopOpens = vanShopOpen;
        vanShopOpen = false;

        const bool passed =
            enemyAttacksIn2D &&
            firstHitLeavesTwoHealth &&
            secondHitLeavesOneHealth &&
            thirdHitStartsDeathAnimation &&
            thirdHitCostsOneLife &&
            parryBlocksDamage &&
            enemySurvivesFirstNormalHit &&
            enemySurvivesSecondNormalHit &&
            thirdNormalHitDefeatsEnemy &&
            chargedShotDefeatsEnemy &&
            playerAttackBlockedIn3D &&
            enemyStillAttacksIn3D &&
            defeatedEnemyDropsCollectibleGem &&
            spectralPurchaseWorks &&
            parryPurchaseWorks &&
            reflectedParryDealsDamage &&
            remoteShopOpens;
        std::cout << "Mapa 1 combat smoke test: " << (passed ? "PASS" : "FAIL")
            << " | enemy attacks 2D: " << enemyAttacksIn2D
            << " | player health: " << (firstHitLeavesTwoHealth && secondHitLeavesOneHealth && thirdHitStartsDeathAnimation && thirdHitCostsOneLife)
            << " (" << firstHitLeavesTwoHealth
            << "=" << healthAfterFirstHit << "/" << livesAfterFirstHit
            << "," << secondHitLeavesOneHealth
            << "=" << healthAfterSecondHit << "/" << livesAfterSecondHit
            << "," << thirdHitStartsDeathAnimation
            << "," << thirdHitCostsOneLife << ")"
            << " | parry: " << parryBlocksDamage
            << " | perfect-only message: " << (parryActivationStaysQuiet && perfectParryShowsEffect)
            << " | normal damage: " << (enemySurvivesFirstNormalHit && enemySurvivesSecondNormalHit && thirdNormalHitDefeatsEnemy)
            << " | charged shot: " << chargedShotDefeatsEnemy
            << " | player blocked 3D: " << playerAttackBlockedIn3D
            << " | enemy attacks 3D: " << enemyStillAttacksIn3D
            << " | gem pickup: " << defeatedEnemyDropsCollectibleGem
            << " | shop purchases: " << (spectralPurchaseWorks && parryPurchaseWorks)
            << " | reflected damage: " << reflectedParryDealsDamage
            << " | remote shop: " << remoteShopOpens << std::endl;
        return passed;
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
            << " | Salud:" << playerHealth << "/" << PlayerMaximumHealth
            << " | Monedas:" << collectedCoins << "/" << TotalCoins
            << " | Gemas:" << spectralGems
            << " | Salto:" << (spectralStepUnlocked
                ? std::string("LISTO")
                : std::string("NO"))
            << " | Retorno:" << (damageParryPurchased ? "LISTO" : "NO")
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
        const float now = static_cast<float>(glfwGetTime());
        streamDeferredWorldModels(dt);
        if (messageTime > 0.0f) {
            messageTime = std::max(0.0f, messageTime - dt);
            if (messageTime <= 0.0f) {
                statusMessage.clear();
            }
        }
        playerAttackCooldown = std::max(0.0f, playerAttackCooldown - dt);
        playerInvulnerability = std::max(0.0f, playerInvulnerability - dt);
        spectralCooldown = std::max(0.0f, spectralCooldown - dt);
        if (playerEntrancePlaying) {
            playerEntranceTime += dt;
            if (playerEntranceTime >= animationDuration(playerEntranceMeshes, PlayerEntranceFramesPerSecond)) {
                playerEntrancePlaying = false;
                playerEntranceTime = 0.0f;
            }
        }
        if (playerTransitionPlaying) {
            playerTransitionTime += dt;
            if (playerTransitionTime >= animationDuration(playerTransitionMeshes, PlayerTransitionFramesPerSecond)) {
                playerTransitionPlaying = false;
                playerTransitionTime = 0.0f;
            }
        }
        updateTitle(window, dt);
        handleVanShop(window, now);
        if (vanShopOpen) {
            stopChargingPlayerAttack();
            wasMoving = false;
            return;
        }
        if (playerDeathPlaying) {
            playerDeathTime += dt;
            if (playerDeathTime >= animationDuration(playerDeathMeshes, PlayerDeathFramesPerSecond)) {
                finishPlayerDeath();
            }
            return;
        }
        if (completed) {
            return;
        }
        if (playerEntrancePlaying) {
            return;
        }

        const bool tabDown = glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS;
        if (tabDown && !tabPressed) {
            const bool keepVanRouteCentered = playerNearVanRoute();
            mode3D = !mode3D;
            stopChargingPlayerAttack();
            startModeTransitionAnimation();
            if (!mode3D) {
                posZ = keepVanRouteCentered ? vanRouteCenterDepth() : 0.0f;
                playerAngle = 0.0f;
            }
            else {
                if (keepVanRouteCentered) {
                    posZ = vanRouteCenterDepth();
                }
                playerAngle = 90.0f;
            }
            std::cout << "Mapa 1 view changed to " << (mode3D ? "3D." : "2D.") << std::endl;
        }
        tabPressed = tabDown;
        handlePlayerAttack(window, dt, now);
        handleParry(window, now);
        handleSpectralStep(window, now);

        const bool movingNow = isMoving(window);
        if (movingNow || !grounded) {
            animationTime += dt;
        }
        else {
            currentFrame = 0;
            animationTime = 0.0f;
        }
        wasMoving = movingNow && grounded;

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
        }
        else {
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
        }
        else {
            grounded = false;
        }

        if (posY <= DeathHeight) {
            loseLife();
        }

        updateEnemies(dt, now);
        updateProjectiles(dt, now);
        updateDroppedGems();
        if (resetEnemiesRequested) {
            resetEnemies();
            resetEnemiesRequested = false;
        }
        if (clearProjectilesRequested) {
            projectiles.clear();
            clearProjectilesRequested = false;
        }
        updateMission(now);

        cameraOffsetY = posY + CameraPlayerCenterOffset;
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

    glm::mat4 normalizedSwordMatrix(const glm::vec3& position, const glm::vec3& velocity, float length) const {
        const glm::vec3 extents = projectileSwordModel.maxBounds - projectileSwordModel.minBounds;
        const float maximumExtent = std::max({ extents.x, extents.y, extents.z, 0.001f });
        const glm::vec3 center = (projectileSwordModel.minBounds + projectileSwordModel.maxBounds) * 0.5f;

        glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
        if (glm::length(velocity) > 0.0001f) {
            model *= glm::mat4_cast(rotationBetweenVectors(glm::vec3(0.0f, -1.0f, 0.0f), velocity));
        }
        model = glm::scale(model, glm::vec3(length / maximumExtent));
        model = glm::translate(model, -center);
        return model;
    }

    void drawSwordModel(const glm::mat4& model, const glm::vec4& color) {
        for (const LoadedMesh& mesh : projectileSwordModel.meshes) {
            drawColoredMesh(mesh.mesh, model, color);
        }
    }

    float renderedDepth(float depth, bool projectIn2D) const {
        return projectIn2D && !mode3D ? posZ : depth;
    }

    void drawPipe() {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), { 1.0f - posX, -0.44f, -posZ });
        model = glm::scale(model, { 0.28f, 0.95f, 0.28f });
        drawColoredMesh(pipeBodyMesh, model, { 0.36f, 0.72f, 0.32f, 1.0f });

        model = glm::translate(glm::mat4(1.0f), { 1.0f - posX, 0.03f, -posZ });
        model = glm::scale(model, { 0.40f, 0.22f, 0.40f });
        drawColoredMesh(pipeRimMesh, model, { 0.48f, 0.82f, 0.38f, 1.0f });

        model = glm::translate(glm::mat4(1.0f), { 1.0f - posX, 0.15f, -posZ });
        model = glm::scale(model, { 0.30f, 0.025f, 0.30f });
        drawColoredMesh(pipeOpeningMesh, model, { 0.02f, 0.10f, 0.08f, 1.0f });
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

            glm::mat4 model = glm::translate(glm::mat4(1.0f), { centerX, platform.height - platform.thickness * 0.5f, centerZ });
            model = glm::scale(model, { width, platform.thickness, depth });
            drawTexturedMesh(platformMesh, model, platformSideTexture, platform.color);

            model = glm::translate(glm::mat4(1.0f), { centerX, platform.height - platform.thickness + 0.028f, centerZ });
            model = glm::scale(model, { width * 1.035f, 0.055f, depth * 1.035f });
            drawColoredMesh(platformMesh, model, platform.color);

            model = glm::translate(glm::mat4(1.0f), { centerX, platform.height + 0.014f, centerZ });
            model = glm::scale(model, { width * 0.96f, 0.028f, depth * 0.96f });
            drawTexturedMesh(platformMesh, model, platformTopTexture, { 0.48f, 0.82f, 0.42f, 1.0f });
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
                { coin.position.x - posX, coin.position.y + bob, renderedDepth(coin.position.z, coin.projectIn2D) - posZ });
            model = glm::rotate(model, now * 5.2f + coin.phase, { 0.0f, 1.0f, 0.0f });
            model = glm::rotate(model, glm::half_pi<float>(), { 1.0f, 0.0f, 0.0f });
            drawColoredMesh(coinMesh, model, { 1.0f, 0.74f, 0.08f, 1.0f });
        }

        if (star.active) {
            const float bob = std::sin(now * 2.8f) * 0.14f;
            glm::mat4 model = glm::translate(
                glm::mat4(1.0f),
                { star.position.x - posX, star.position.y + bob, renderedDepth(star.position.z, star.projectIn2D) - posZ });
            model = glm::rotate(model, now * 2.8f, { 0.0f, 1.0f, 0.0f });
            model = glm::scale(model, glm::vec3(0.88f));
            drawColoredMesh(starMesh, model, { 1.0f, 0.86f, 0.12f, 1.0f });
        }
    }

    void drawEnemies() {
        for (const DemonEnemy& enemy : enemies) {
            const std::vector<Mesh>* frames = &enemyIdleMeshes;
            float framesPerSecond = 8.0f;
            bool clampLastFrame = false;
            if (!enemy.alive) {
                if (enemy.animationTime > 1.05f) {
                    continue;
                }
                frames = &enemyDeathMeshes;
                framesPerSecond = 9.0f;
                clampLastFrame = true;
            }
            else if (enemy.state == EnemyState::Attack) {
                frames = &enemyAttackMeshes;
                framesPerSecond = 9.0f;
                clampLastFrame = true;
            }
            else if (enemy.state == EnemyState::Hurt) {
                frames = &enemyHurtMeshes;
                framesPerSecond = 9.0f;
                clampLastFrame = true;
            }
            else if (enemy.state == EnemyState::Chase) {
                frames = &enemyRunMeshes;
                framesPerSecond = 9.0f;
            }

            if (frames->empty()) {
                continue;
            }

            size_t frameIndex = static_cast<size_t>(enemy.animationTime * framesPerSecond);
            frameIndex = clampLastFrame
                ? std::min(frameIndex, frames->size() - 1)
                : frameIndex % frames->size();

            const float renderZ = renderedDepth(enemy.position.z, true) - posZ + (!mode3D ? 0.16f : 0.0f);
            glm::mat4 model = glm::translate(
                glm::mat4(1.0f),
                { enemy.position.x - posX, enemy.position.y, renderZ });
            const float angle = mode3D
                ? (enemy.facingLeft ? 270.0f : 90.0f)
                : (enemy.facingLeft ? 180.0f : 0.0f);
            model = glm::rotate(model, glm::radians(angle), { 0.0f, 1.0f, 0.0f });
            drawTexturedMesh((*frames)[frameIndex], model, enemyTexture, { 0.92f, 0.16f, 0.12f, 1.0f });
        }
    }

    void drawDroppedGems(float now) {
        for (const DroppedGem& gem : droppedGems) {
            const float bob = std::sin(now * 3.8f + gem.phase) * 0.09f;
            const float pulse = 1.0f + std::sin(now * 6.4f + gem.phase) * 0.06f;
            const float renderZ = renderedDepth(gem.position.z, gem.projectIn2D) - posZ + (!mode3D ? 0.34f : 0.0f);
            const glm::vec3 renderPosition(gem.position.x - posX, gem.position.y + bob, renderZ);

            if (gemModelLoaded) {
                const glm::vec3 extents = gemModel.model.maxBounds - gemModel.model.minBounds;
                const float maximumExtent = std::max({ extents.x, extents.y, extents.z, 0.001f });
                const float scale = (GemModelDisplaySize / maximumExtent) * pulse;
                const glm::vec3 center = (gemModel.model.minBounds + gemModel.model.maxBounds) * 0.5f;
                glm::mat4 model = glm::translate(glm::mat4(1.0f), renderPosition);
                model = glm::rotate(model, now * 2.6f + gem.phase, { 0.0f, 1.0f, 0.0f });
                model = glm::rotate(model, glm::radians(12.0f), { 0.0f, 0.0f, 1.0f });
                model = glm::scale(model, glm::vec3(scale));
                model = glm::translate(model, -center);
                drawRuntimeModel(gemModel, model, { 0.95f, 0.03f, 0.06f, 1.0f });
            }
            else {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), renderPosition);
                model = glm::rotate(model, now * 2.8f + gem.phase, { 0.0f, 1.0f, 0.0f });
                model = glm::scale(model, glm::vec3(0.34f * pulse));
                drawColoredMesh(starMesh, model, { 0.95f, 0.03f, 0.06f, 1.0f });
            }

            glm::mat4 glow = glm::translate(glm::mat4(1.0f), renderPosition);
            glow = glm::scale(glow, glm::vec3(0.48f * pulse));
            drawColoredMesh(parryRingMesh, glow, { 1.0f, 0.08f, 0.12f, 0.55f });
        }
    }

    void drawProjectiles() {
        for (const Projectile& projectile : projectiles) {
            const float renderZ = renderedDepth(projectile.position.z, true) - posZ + (!mode3D ? 0.28f : 0.0f);
            const float length = ProjectileSwordLength * (projectile.charged ? 1.55f : 1.0f);
            const glm::mat4 model = normalizedSwordMatrix(
                { projectile.position.x - posX, projectile.position.y, renderZ },
                projectile.velocity,
                length);
            drawSwordModel(
                model,
                projectile.fromPlayer
                ? glm::vec4(0.18f, 0.66f, 1.00f, 1.0f)
                : glm::vec4(1.00f, 0.16f, 0.10f, 1.0f));
        }
    }

    void drawSpectralAnchors(float now) {
        for (const SpectralAnchor& anchor : spectralAnchors) {
            const float bob = std::sin(now * 3.0f + anchor.phase) * 0.07f;
            const float pulse = 1.0f + std::sin(now * 5.4f + anchor.phase) * 0.08f;
            const float renderZ = renderedDepth(anchor.origin.z, anchor.projectIn2D) - posZ + (!mode3D ? 0.30f : 0.0f);
            const glm::vec3 renderPosition(anchor.origin.x - posX, anchor.origin.y + bob, renderZ);
            const glm::vec4 anchorColor = spectralStepUnlocked
                ? glm::vec4(0.18f, 0.72f, 1.00f, 1.0f)
                : glm::vec4(0.18f, 0.32f, 0.46f, 1.0f);
            const glm::vec4 ringColor = spectralStepUnlocked
                ? glm::vec4(0.22f, 0.95f, 1.00f, 1.0f)
                : glm::vec4(0.12f, 0.42f, 0.58f, 1.0f);

            glm::mat4 ring = glm::translate(glm::mat4(1.0f), renderPosition);
            ring = glm::scale(ring, glm::vec3(0.70f * pulse));
            drawColoredMesh(parryRingMesh, ring, ringColor);

            const glm::mat4 sword = normalizedSwordMatrix(
                renderPosition + glm::vec3(0.0f, 0.10f, 0.0f),
                glm::vec3(0.0f, -1.0f, 0.0f),
                0.78f * pulse);
            drawSwordModel(sword, anchorColor);
        }
    }

    void drawCombatEffects(float now) {
        if (chargingPlayerAttack && !mode3D) {
            const float ratio = std::clamp(playerChargeTime / PlayerChargeTime, 0.0f, 1.0f);
            const float previewLength = ProjectileSwordLength * (0.42f + ratio * 1.13f);
            const glm::vec3 offset = playerAimDirection * (0.42f + previewLength * 0.34f);
            const glm::mat4 model = normalizedSwordMatrix(
                { offset.x, posY + 0.62f + offset.y, 0.36f },
                playerAimDirection,
                previewLength);
            drawSwordModel(model, { 0.18f, 0.66f, 1.00f, 1.0f });
        }

        if (now <= spectralEffectUntil) {
            const float ratio = std::clamp((spectralEffectUntil - now) / 0.46f, 0.0f, 1.0f);
            const float pulse = 1.0f + (1.0f - ratio) * 0.85f;
            glm::mat4 model = glm::translate(
                glm::mat4(1.0f),
                mode3D ? glm::vec3(-0.08f, posY + 0.58f, 0.0f) : glm::vec3(0.0f, posY + 0.58f, 0.38f));
            if (mode3D) {
                model = glm::rotate(model, glm::half_pi<float>(), { 0.0f, 1.0f, 0.0f });
            }
            model = glm::scale(model, glm::vec3(pulse));
            drawColoredMesh(parryRingMesh, model, { 0.18f, 0.72f, 1.00f, 1.0f });
        }

    }

    void drawWorld() {
        glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), { -posX, -1.0f, -posZ });
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
                }
                else {
                    shader.setInt("modoRender", 2);
                    shader.setVec4("colorMaterial", material != nullptr ? material->color : glm::vec4(1.0f));
                }
                mesh.mesh.draw();
            }
        }
    }

    void drawRuntimeModel(const WorldModel& modelData, const glm::mat4& modelMatrix, const glm::vec4& fallbackColor) {
        shader.setMat4("model", modelMatrix);
        for (const LoadedMesh& mesh : modelData.model.meshes) {
            const MapMaterial* material = mesh.materialIndex < modelData.materials.size()
                ? &modelData.materials[mesh.materialIndex]
                : nullptr;

            if (material != nullptr && material->texture != nullptr && material->texture->valid()) {
                shader.setInt("modoRender", 0);
                shader.setInt("tex0", 0);
                material->texture->bind();
            }
            else {
                shader.setInt("modoRender", 2);
                shader.setVec4("colorMaterial", material != nullptr ? material->color : fallbackColor);
            }
            mesh.mesh.draw();
        }
    }

    void drawVanShop(float now) {
        const glm::vec3 van = vanWorldPosition();
        const float renderZ = renderedDepth(van.z, true) - posZ;
        if (std::abs(van.x - posX) > VanRenderRangeX || (mode3D && std::abs(van.z - posZ) > VanRenderRangeZ)) {
            return;
        }

        const glm::vec3 renderPosition(van.x - posX, van.y, renderZ);
        float vanMarkerHeight = 1.45f;

        if (ensureVanModelLoaded()) {
            const glm::vec3 extents = vanModel.model.maxBounds - vanModel.model.minBounds;
            const float maximumExtent = std::max({ extents.x, extents.y, extents.z, 0.001f });
            const float vanScale = VanModelDisplaySize / maximumExtent;
            const float scaledHeight = extents.y * vanScale;
            const glm::vec3 center = (vanModel.model.minBounds + vanModel.model.maxBounds) * 0.5f;
            const glm::vec3 vanModelPosition = renderPosition + glm::vec3(0.0f, scaledHeight * 0.5f, 0.0f);
            vanMarkerHeight = scaledHeight + 0.35f;
            glm::mat4 model = glm::translate(glm::mat4(1.0f), vanModelPosition);
            model = glm::rotate(model, glm::radians(mode3D ? -25.0f : 0.0f), { 0.0f, 1.0f, 0.0f });
            model = glm::scale(model, glm::vec3(vanScale));
            model = glm::translate(model, -center);
            drawRuntimeModel(vanModel, model, { 0.72f, 0.78f, 0.86f, 1.0f });
        }
        else {
            glm::mat4 body = glm::translate(glm::mat4(1.0f), renderPosition + glm::vec3(0.0f, 0.95f, 0.0f));
            body = glm::scale(body, { 0.72f, 1.90f, 0.72f });
            drawColoredMesh(platformMesh, body, { 0.74f, 0.04f, 0.05f, 1.0f });
        }

        const float pulse = 1.0f + std::sin(now * 4.0f) * 0.06f;
        glm::mat4 marker = glm::translate(glm::mat4(1.0f), renderPosition + glm::vec3(0.0f, vanMarkerHeight, 0.0f));
        marker = glm::scale(marker, glm::vec3(0.48f * pulse));
        drawColoredMesh(parryRingMesh, marker, spectralStepUnlocked
            ? glm::vec4(1.0f, 0.82f, 0.18f, 1.0f)
            : glm::vec4(0.18f, 0.72f, 1.0f, 1.0f));
    }

    void drawPlayer() {
        const float now = static_cast<float>(glfwGetTime());
        const bool airborne = !grounded;
        const bool guardActive = playerGuardUntil > 0.0f && now <= playerGuardUntil;
        const bool lockedAction = playerDeathPlaying || playerEntrancePlaying || playerTransitionPlaying || guardActive;
        const float step = wasMoving && !lockedAction ? std::sin(now * 14.0f) : 0.0f;
        const float bounce = wasMoving && !lockedAction ? std::fabs(step) * 0.035f : 0.0f;
        const float jumpStrength = airborne ? std::clamp(std::abs(velocityY) / JumpSpeed, 0.0f, 1.0f) : 0.0f;
        const float walkTilt = wasMoving && !lockedAction ? step * 4.0f : 0.0f;
        const bool jumpPoseActive = airborne && !lockedAction;
        const float jumpTilt = jumpPoseActive ? (velocityY >= 0.0f ? -10.0f : 8.0f) * jumpStrength : 0.0f;
        const glm::vec3 jumpScale = jumpPoseActive
            ? glm::vec3(1.0f - jumpStrength * 0.06f, 1.0f + jumpStrength * 0.10f, 1.0f)
            : glm::vec3(1.0f);

        const Mesh* selectedMesh = &playerIdleMesh;
        if (playerDeathPlaying) {
            selectedMesh = animationFrame(playerDeathMeshes, playerDeathTime, PlayerDeathFramesPerSecond, false);
        }
        else if (playerEntrancePlaying) {
            selectedMesh = animationFrame(playerEntranceMeshes, playerEntranceTime, PlayerEntranceFramesPerSecond, false);
        }
        else if (playerTransitionPlaying) {
            selectedMesh = animationFrame(playerTransitionMeshes, playerTransitionTime, PlayerTransitionFramesPerSecond, false);
        }
        else if (guardActive) {
            selectedMesh = airborne ? &playerGuardAirMesh : &playerGuardGroundMesh;
        }
        else if (airborne && !playerJumpMeshes.empty()) {
            const size_t jumpIndex = velocityY > JumpSpeed * 0.22f
                ? 0
                : (velocityY > -JumpSpeed * 0.35f ? std::min<size_t>(1, playerJumpMeshes.size() - 1) : playerJumpMeshes.size() - 1);
            selectedMesh = &playerJumpMeshes[jumpIndex];
        }
        else if (wasMoving) {
            selectedMesh = animationFrame(playerRunMeshes, animationTime, PlayerRunFramesPerSecond, true);
        }
        if (selectedMesh == nullptr) {
            selectedMesh = &playerIdleMesh;
        }

        glm::mat4 model = glm::translate(glm::mat4(1.0f), { 0.0f, posY + bounce, 0.22f });
        model = glm::rotate(model, glm::radians(playerAngle), { 0.0f, 1.0f, 0.0f });
        model = glm::rotate(model, glm::radians(walkTilt + jumpTilt), { 0.0f, 0.0f, 1.0f });
        model = glm::scale(model, jumpScale);
        drawTexturedMesh(*selectedMesh, model, playerAtlasTexture, { 1.0f, 1.0f, 1.0f, 1.0f });
    }

    bool initialize(bool enableAudio) {
        if (initialized) {
            return true;
        }

        if (!shader.load(resolveAssetPath("shaders/mapa1.vert"), resolveAssetPath("shaders/mapa1.frag"))) {
            return false;
        }

        if (!playerAtlasTexture.loadFromFile(resolveAssetPath("assets/mapa1/player/vergil_dmc5_spritesheet.png"))) {
            std::cerr << "Mapa 1 player sprite atlas could not be loaded." << std::endl;
            return false;
        }
        playerAtlasTexture.bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        const int playerAtlasWidth = playerAtlasTexture.width();
        const int playerAtlasHeight = playerAtlasTexture.height();
        auto buildPlayerMeshes = [&](const std::vector<AtlasFrame>& frames, std::vector<Mesh>& meshes) {
            meshes.clear();
            meshes.reserve(frames.size());
            for (const AtlasFrame& frame : frames) {
                meshes.push_back(createPlayerSpriteMesh(frame, playerAtlasWidth, playerAtlasHeight));
            }
            };

        playerIdleMesh = createPlayerSpriteMesh({ 641, 547, 42, 62 }, playerAtlasWidth, playerAtlasHeight);
        playerGuardGroundMesh = createPlayerSpriteMesh({ 2, 380, 35, 55 }, playerAtlasWidth, playerAtlasHeight);
        playerGuardAirMesh = createPlayerSpriteMesh({ 58, 380, 37, 49 }, playerAtlasWidth, playerAtlasHeight);
        buildPlayerMeshes({
            {2, 199, 48, 62},
            {62, 202, 54, 54},
            {133, 198, 48, 63}
            }, playerJumpMeshes);
        buildPlayerMeshes({
            {2, 296, 43, 52},
            {55, 296, 46, 52},
            {116, 296, 48, 52},
            {174, 296, 42, 52},
            {226, 296, 54, 52},
            {286, 296, 51, 52},
            {346, 296, 45, 52},
            {407, 296, 50, 52}
            }, playerRunMeshes);
        buildPlayerMeshes({
            {0, 547, 55, 62},
            {55, 547, 62, 62},
            {117, 547, 65, 62},
            {183, 547, 60, 62},
            {243, 547, 67, 62},
            {310, 547, 67, 62},
            {377, 547, 50, 62},
            {419, 547, 38, 62},
            {462, 547, 38, 62},
            {506, 547, 39, 62},
            {551, 547, 39, 62},
            {596, 547, 42, 62},
            {641, 547, 42, 62}
            }, playerEntranceMeshes);
        buildPlayerMeshes({
            {0, 725, 60, 58},
            {66, 725, 58, 58},
            {132, 725, 70, 58},
            {203, 725, 50, 58},
            {253, 725, 70, 58}
            }, playerDeathMeshes);
        buildPlayerMeshes({
            {0, 2030, 72, 58},
            {94, 2030, 44, 58},
            {151, 2030, 47, 58},
            {221, 2030, 35, 58},
            {279, 2030, 38, 58},
            {340, 2030, 38, 58},
            {390, 2030, 43, 58},
            {446, 2030, 38, 58},
            {493, 2030, 38, 58},
            {545, 2030, 38, 58},
            {595, 2030, 36, 58},
            {642, 2030, 38, 58},
            {688, 2030, 42, 58}
            }, playerTransitionMeshes);

        projectileSwordModel = ModelLoader::loadModel(resolveAssetPath("assets/items/vergil_summoned_sword/scene.gltf"));
        if (projectileSwordModel.meshes.empty()) {
            std::cerr << "Mapa 1 summoned sword projectile could not be loaded." << std::endl;
            return false;
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
        if (!enemyTexture.loadFromFile(resolveAssetPath("assets/mapa1/enemies/Demon_Spritesheet.png"))) {
            std::cerr << "Mapa 1 enemy texture could not be loaded." << std::endl;
            return false;
        }
        skyboxTexture.bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        enemyTexture.bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        const std::string worldBase = "assets/mapa1/world1/";
        if (!loadWorldModel(worldBase + "CourseSelectW1.dae")) {
            std::cerr << "Mapa 1 world models could not be loaded." << std::endl;
            return false;
        }
        deferredWorldModelPaths = {
            worldBase + "CourseSelectWallW1.dae",
            worldBase + "CourseSelectWaveW1.dae",
            worldBase + "CourseSelectDecorationW1A.dae"
        };
        deferredWorldLoadDelay = 0.45f;
        vanModelLoadAttempted = false;
        loadGemModel();

        skyboxMesh = createSkyboxMesh();
        pipeBodyMesh = Mesh::cylinder(8);
        pipeRimMesh = Mesh::cylinder(8);
        pipeOpeningMesh = Mesh::cylinder(8);
        platformMesh = Mesh::cube();
        coinMesh = Mesh::cylinder(32, 0.14f, 0.42f);
        starMesh = createStarMesh();
        parryRingMesh = createParryRingMesh();

        const std::array<AtlasFrame, 7> enemyIdleFrames = {
            AtlasFrame{7, 19, 15, 13}, AtlasFrame{39, 19, 15, 13}, AtlasFrame{71, 19, 15, 13},
            AtlasFrame{103, 19, 15, 13}, AtlasFrame{135, 19, 15, 13}, AtlasFrame{167, 19, 15, 13},
            AtlasFrame{199, 19, 15, 13}
        };
        const std::array<AtlasFrame, 7> enemyRunFrames = {
            AtlasFrame{7, 49, 15, 14}, AtlasFrame{39, 49, 15, 14}, AtlasFrame{71, 49, 15, 14},
            AtlasFrame{103, 50, 15, 14}, AtlasFrame{135, 49, 15, 14}, AtlasFrame{167, 49, 15, 14},
            AtlasFrame{199, 49, 15, 14}
        };
        const std::array<AtlasFrame, 6> enemyAttackFrames = {
            AtlasFrame{8, 83, 16, 13}, AtlasFrame{40, 83, 16, 13}, AtlasFrame{71, 83, 16, 13},
            AtlasFrame{103, 83, 16, 13}, AtlasFrame{135, 83, 16, 13}, AtlasFrame{167, 83, 16, 13}
        };
        const std::array<AtlasFrame, 4> enemyHurtFrames = {
            AtlasFrame{8, 115, 15, 13}, AtlasFrame{40, 115, 15, 13},
            AtlasFrame{72, 115, 15, 13}, AtlasFrame{104, 115, 15, 13}
        };
        const std::array<AtlasFrame, 9> enemyDeathFrames = {
            AtlasFrame{8, 147, 15, 13}, AtlasFrame{40, 147, 15, 13}, AtlasFrame{72, 147, 15, 13},
            AtlasFrame{104, 147, 15, 13}, AtlasFrame{135, 147, 15, 13}, AtlasFrame{167, 147, 15, 13},
            AtlasFrame{194, 147, 15, 13}, AtlasFrame{218, 147, 15, 13}, AtlasFrame{239, 147, 15, 13}
        };
        auto buildEnemyMeshes = [](const auto& frames, std::vector<Mesh>& meshes) {
            meshes.clear();
            meshes.reserve(frames.size());
            for (const AtlasFrame& frame : frames) {
                meshes.push_back(createSpriteMesh(frame, 0.88f, 0.82f, 256, 192));
            }
            };
        buildEnemyMeshes(enemyIdleFrames, enemyIdleMeshes);
        buildEnemyMeshes(enemyRunFrames, enemyRunMeshes);
        buildEnemyMeshes(enemyAttackFrames, enemyAttackMeshes);
        buildEnemyMeshes(enemyHurtFrames, enemyHurtMeshes);
        buildEnemyMeshes(enemyDeathFrames, enemyDeathMeshes);

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
        playerEntranceTime = 0.0f;
        playerTransitionTime = 0.0f;
        playerDeathTime = 0.0f;
        playerGuardUntil = 0.0f;
        wasMoving = false;
        playerEntrancePlaying = false;
        playerTransitionPlaying = false;
        playerDeathPlaying = false;
        tabPressed = false;
        mouseAttackPressed = false;
        chargingPlayerAttack = false;
        parryPressed = false;
        resetEnemiesRequested = false;
        clearProjectilesRequested = false;
        combatHintUntil = 0.0f;
        playerAttackCooldown = 0.0f;
        playerChargeTime = 0.0f;
        playerInvulnerability = 0.0f;
        parryUntil = 0.0f;
        parryEffectUntil = 0.0f;
        resetSpectralProgress();
        playerAimDirection = { 1.0f, 0.0f, 0.0f };
        playerHealth = PlayerMaximumHealth;
        projectiles.clear();
        resetPlayer(true);
        resetMission();
        resetEnemies();
        if (enemies.size() != EnemySpawnCount) {
            std::cerr << "Mapa 1 expected " << EnemySpawnCount << " enemies but created " << enemies.size() << "." << std::endl;
            return false;
        }
        if (!validateParkourMission()) {
            return false;
        }
        if (enableAudio) {
            initializeAudio();
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
            view = glm::translate(view, { 0.0f, -cameraOffsetY, -3.0f });
        }
        else {
            const glm::vec3 cameraPosition(-3.0f, cameraOffsetY + 0.55f, 0.0f);
            const glm::vec3 cameraTarget(0.0f, cameraOffsetY, 0.0f);
            view = glm::lookAt(cameraPosition, cameraTarget, { 0.0f, 1.0f, 0.0f });
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
        drawVanShop(static_cast<float>(glfwGetTime()));
        drawMissionItems(static_cast<float>(glfwGetTime()));
        drawSpectralAnchors(static_cast<float>(glfwGetTime()));
        drawEnemies();
        drawDroppedGems(static_cast<float>(glfwGetTime()));
        drawProjectiles();
        drawCombatEffects(static_cast<float>(glfwGetTime()));
        drawPlayer();
    }

    void shutdown() {
        shutdownAudio();
        terrainTriangles.clear();
        worldModels.clear();
        deferredWorldModelPaths.clear();
        vanModel = {};
        vanModelLoaded = false;
        vanModelLoadAttempted = false;
        gemModel = {};
        gemModelLoaded = false;
        deferredWorldLoadDelay = 0.0f;
        textureCache.clear();
        playerAtlasTexture = Texture2D{};
        projectileSwordModel = {};
        skyboxTexture = Texture2D{};
        coinIconTexture = Texture2D{};
        platformSideTexture = Texture2D{};
        platformTopTexture = Texture2D{};
        enemyTexture = Texture2D{};
        playerIdleMesh = Mesh{};
        playerGuardGroundMesh = Mesh{};
        playerGuardAirMesh = Mesh{};
        skyboxMesh = Mesh{};
        pipeBodyMesh = Mesh{};
        pipeRimMesh = Mesh{};
        pipeOpeningMesh = Mesh{};
        platformMesh = Mesh{};
        coinMesh = Mesh{};
        starMesh = Mesh{};
        parryRingMesh = Mesh{};
        playerJumpMeshes.clear();
        playerRunMeshes.clear();
        playerEntranceMeshes.clear();
        playerDeathMeshes.clear();
        playerTransitionMeshes.clear();
        enemyIdleMeshes.clear();
        enemyRunMeshes.clear();
        enemyAttackMeshes.clear();
        enemyHurtMeshes.clear();
        enemyDeathMeshes.clear();
        coins.clear();
        star = {};
        enemies.clear();
        projectiles.clear();
        droppedGems.clear();
        shader = Shader{};
        initialized = false;
    }
};

Mapa1::Mapa1()
    : m_impl(std::make_unique<Impl>()) {
}

Mapa1::~Mapa1() = default;

bool Mapa1::initialize(bool enableAudio) {
    return m_impl->initialize(enableAudio);
}

bool Mapa1::runCombatSmokeTest() {
    return m_impl->runCombatSmokeTest();
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

bool Mapa1::showCombatHint(float timeSeconds) const {
    return timeSeconds <= m_impl->combatHintUntil;
}

bool Mapa1::showSpectralLockedHint(float timeSeconds) const {
    return timeSeconds <= m_impl->spectralLockedHintUntil;
}

bool Mapa1::showSpectralReadyHint(float timeSeconds) const {
    return timeSeconds <= m_impl->spectralReadyHintUntil;
}

bool Mapa1::showSpectralUnlockHint(float timeSeconds) const {
    return timeSeconds <= m_impl->spectralUnlockHintUntil;
}

bool Mapa1::levelComplete() const {
    return m_impl->completed;
}

int Mapa1::remainingEnemyCount() const {
    return m_impl->remainingEnemyCount();
}

int Mapa1::spectralGemCount() const {
    return m_impl->spectralGems;
}

int Mapa1::spectralGemRequirement() const {
    return SpectralGemRequirement;
}

bool Mapa1::spectralUnlocked() const {
    return m_impl->spectralStepUnlocked;
}

bool Mapa1::damageParryUnlocked() const {
    return m_impl->damageParryPurchased;
}

int Mapa1::damageParryCost() const {
    return DamageParryGemRequirement;
}

bool Mapa1::shopOpen() const {
    return m_impl->vanShopOpen;
}

void Mapa1::openShop() {
    m_impl->vanShopOpen = true;
    m_impl->stopChargingPlayerAttack();
}

void Mapa1::closeShop() {
    m_impl->vanShopOpen = false;
}

bool Mapa1::purchaseSpectralStep() {
    return m_impl->purchaseSpectralStep();
}

bool Mapa1::purchaseDamageParry() {
    return m_impl->purchaseDamageParry();
}

bool Mapa1::showVanPrompt(float timeSeconds) const {
    return timeSeconds <= m_impl->vanPromptUntil;
}

bool Mapa1::showVanShopCards(float timeSeconds) const {
    (void)timeSeconds;
    return m_impl->vanShopOpen;
}

int Mapa1::currentHealth() const {
    return m_impl->playerHealth;
}

int Mapa1::maximumHealth() const {
    return PlayerMaximumHealth;
}

float Mapa1::chargeRatio() const {
    return std::clamp(m_impl->playerChargeTime / PlayerChargeTime, 0.0f, 1.0f);
}

bool Mapa1::chargingAttack() const {
    return m_impl->chargingPlayerAttack;
}

bool Mapa1::parryActive(float timeSeconds) const {
    return timeSeconds <= m_impl->parryEffectUntil;
}

const Texture2D& Mapa1::coinIconTexture() const {
    return m_impl->coinIconTexture;
}

const Texture2D& Mapa1::gemIconTexture() const {
    if (!m_impl->gemModel.materials.empty()) {
        const MapMaterial& material = m_impl->gemModel.materials.front();
        if (material.texture && material.texture->valid()) {
            return *material.texture;
        }
    }
    return m_impl->coinIconTexture;
}
