#pragma once

#include "Environment.h"
#include "ModelLoader.h"
#include "Shader.h"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

enum class PlayMode {
    Mode2D,
    Mode3D
};

struct PlayerInput {
    glm::vec2 move{0.0f};
    bool jumpPressed{false};
    PlayMode mode{PlayMode::Mode3D};
    float cameraYawRadians{0.0f};
    float lockedDepth{0.0f};
};

class Player {
public:
    bool load(const std::string& modelPath);
    void configureCharacterMetrics(float desiredHeight, const glm::vec3& collisionHalf, float visualYOffset, float maxSpeed3D, float maxSpeed2D);
    void spawnAt(const glm::vec3& feetPosition);
    void teleportTo(const glm::vec3& feetPosition);
    void update(const PlayerInput& input, const std::vector<Bounds>& colliders, const glm::vec3& worldMin, const glm::vec3& worldMax, float deltaTime);
    void render(const Shader& shader) const;

    glm::vec3 position() const { return m_position; }
    glm::vec3 velocity() const { return m_velocity; }
    bool grounded() const { return m_grounded; }
    float facingYaw() const { return m_facingYaw; }
    Bounds bounds() const;

private:
    struct Part {
        Mesh mesh;
        Material material;
    };

    void updateHorizontalVelocity(const PlayerInput& input, float deltaTime);
    void moveAndCollide(int axis, float amount, const std::vector<Bounds>& colliders);
    void keepInsideWorld(const glm::vec3& worldMin, const glm::vec3& worldMax);
    glm::mat4 modelMatrix() const;
    void bindMaterial(const Shader& shader, const Material& material) const;
    std::shared_ptr<Texture2D> loadTexture(const std::string& path);
    std::shared_ptr<Texture2D> loadTexture(const LoadedMaterial& material);

    std::vector<Part> m_parts;
    std::vector<std::shared_ptr<Texture2D>> m_textures;
    glm::vec3 m_position{0.0f, 5.0f, 0.0f};
    glm::vec3 m_spawnPoint{0.0f, 5.0f, 0.0f};
    glm::vec3 m_velocity{0.0f};
    glm::vec3 m_modelMin{0.0f};
    glm::vec3 m_modelMax{0.0f, 1.0f, 0.0f};
    glm::vec3 m_modelCenter{0.0f};
    glm::vec3 m_collisionHalf{0.34f, 0.82f, 0.28f};
    float m_modelScale{1.0f};
    float m_visualYOffset{0.0f};
    float m_facingYaw{0.0f};
    float m_modelYawOffset{0.0f};
    float m_maxSpeed3D{4.25f};
    float m_maxSpeed2D{4.65f};
    bool m_marioMapVariant{false};
    bool m_deadpoolVariant{false};
    bool m_grounded{false};
};
