#pragma once

#include "Environment.h"
#include "GameSystems.h"
#include "Player.h"
#include "Shader.h"

#include <glm/glm.hpp>

#include <memory>
#include <vector>

struct GLFWwindow;

struct Map3Projectile {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float lifetime{4.0f};
    bool reflected{false};
};

class Map3EnemyManager {
public:
    bool initialize();
    void reset(const Environment& environment, const glm::vec3& playerSpawn);
    bool update(const Player& player, const Environment& environment, float deltaTime, float timeSeconds, bool dodgeActive, std::vector<Map3Projectile>& projectiles);
    void render(const Shader& shader, float timeSeconds, const glm::vec3& cameraPosition) const;
    bool damageEnemyAt(const glm::vec3& position, float horizontalRadius, float verticalRadius, int damage);
    glm::vec3 directionToClosestEnemy(const glm::vec3& position) const;
    int aliveCount() const;

private:
    struct Enemy {
        glm::vec3 position{0.0f};
        glm::vec3 spawnPosition{0.0f};
        float yaw{0.0f};
        float phase{0.0f};
        float hurtTimer{0.0f};
        float shotCooldown{0.0f};
        int health{2};
        bool alive{true};
    };

    bool loadEnemyModel();
    void buildFallbackModel();
    glm::vec3 findSpawnPosition(const Environment& environment, const glm::vec3& playerSpawn, const glm::vec2& anchor) const;
    bool findFloorAt(const Environment& environment, float x, float z, float preferredY, float& floorY) const;
    bool tryMoveEnemy(Enemy& enemy, const Environment& environment, const glm::vec3& step) const;
    Bounds enemyBounds(const Enemy& enemy) const;
    glm::mat4 enemyModelMatrix(const Enemy& enemy, float timeSeconds) const;

    std::vector<MissionRenderablePart> m_parts;
    std::vector<std::shared_ptr<Texture2D>> m_textures;
    Mesh m_fallbackMesh;
    Material m_fallbackMaterial;
    glm::vec3 m_modelMin{0.0f};
    glm::vec3 m_modelMax{0.0f, 1.0f, 0.0f};
    glm::vec3 m_modelCenter{0.0f};
    float m_modelScale{1.0f};
    std::vector<Enemy> m_enemies;
    bool m_initialized{false};
};

struct Map3Runtime {
    Environment environment;
    Player player;
    MissionManager mission;
    Map3EnemyManager enemies;
    bool initialized{false};
    bool sessionActive{false};
    bool skipFirstUpdateFrame{false};
    int health{3};
    int maxHealth{3};
    float damageCooldown{0.0f};
    float dodgeCooldown{0.0f};
    float dodgeActiveUntil{0.0f};
    float parryActiveUntil{0.0f};
    std::vector<Map3Projectile> projectiles;
    bool gameOver{false};
};

bool iniciarMap3(Map3Runtime& map3);
void volverAlMenu(Map3Runtime& map3);
void renderMap3(GLFWwindow* window, Map3Runtime& map3, const Shader& sceneShader, const Shader& lavaShader, float now);
bool map3DefensiveActionActive(const Map3Runtime& map3, float timeSeconds);
