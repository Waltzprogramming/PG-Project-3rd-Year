#pragma once

#include "AudioPlayer.h"
#include "Environment.h"
#include "GameSystems.h"
#include "GameUI.h"
#include "Player.h"
#include "Shader.h"

#include <glm/glm.hpp>
#include <array>
#include <memory>
#include <vector>

struct GLFWwindow;

// representa un disparo del jugador o de un enemigo mientras está vivo en escena
struct Mapa4Projectile {
    glm::vec3 position{0.0f};
    glm::vec3 velocity{0.0f};
    float lifetime{4.20f};
    int damage{1};
    bool fromEnemy{false};
};

// guarda la posición y el tiempo de respawn de cada sol del mapa
struct SunPickup {
    glm::vec3 position{0.0f};
    bool available{true};
    float phase{0.0f};
    float respawnAt{0.0f};
};

// maneja los soles, la recarga de luz y el pequeño render de esos pickups
class Map4LightManager {
public:
    bool initialize();
    void reset(const Environment& environment, const glm::vec3& playerSpawn);
    void update(Player& player, float timeSeconds, float deltaTime, float& energySeconds);
    void render(const Shader& shader, float timeSeconds, const glm::vec3& cameraPosition) const;

private:
    // estas texturas y piezas quedan cacheadas para no recargar el modelo del sol cada vez
    std::shared_ptr<Texture2D> loadSunTexture(const std::string& path);
    std::shared_ptr<Texture2D> loadSunTexture(const LoadedMaterial& material);
    bool loadSunModel();
    Bounds sunBounds(const SunPickup& pickup) const;
    glm::mat4 sunModelMatrix(const SunPickup& pickup, float timeSeconds) const;

    std::vector<MissionRenderablePart> m_parts;
    std::vector<std::shared_ptr<Texture2D>> m_textures;
    Mesh m_fallbackMesh;
    Material m_fallbackMaterial;
    glm::vec3 m_modelMin{0.0f};
    glm::vec3 m_modelMax{0.0f, 1.0f, 0.0f};
    glm::vec3 m_modelCenter{0.0f};
    float m_modelScale{1.0f};
    std::vector<SunPickup> m_pickups;
    bool m_initialized{false};
};

// controla enemigos simples con patrulla corta y disparo a distancia
class SimpleEnemyManager {
public:
    bool initialize();
    void reset(const Environment& environment, const glm::vec3& playerSpawn);
    bool update(const Player& player, const Environment& environment, float deltaTime, float timeSeconds, std::vector<Mapa4Projectile>& projectiles);
    void render(const Shader& shader, float timeSeconds, const glm::vec3& cameraPosition) const;
    bool damageEnemyAt(const glm::vec3& position, float horizontalRadius, float verticalRadius, int damage);
    int aliveCount() const;

private:
    // datos compartidos del modelo que usa cada tipo de enemigo
    struct EnemyModel {
        std::vector<MissionRenderablePart> parts;
        glm::vec3 modelMin{0.0f};
        glm::vec3 modelMax{0.0f, 1.0f, 0.0f};
        glm::vec3 modelCenter{0.0f};
        float modelScale{1.0f};
        glm::vec3 color{1.0f};
    };

    // estado vivo de cada enemigo ya colocado dentro del mapa
    struct Enemy {
        glm::vec3 position{0.0f};
        glm::vec3 patrolCenter{0.0f};
        float patrolRadius{1.8f};
        float patrolSpeed{0.9f};
        float patrolPhase{0.0f};
        bool patrolOnX{true};
        float yaw{0.0f};
        float attackCooldown{0.0f};
        float detectionRange{4.6f};
        float attackRange{5.2f};
        int modelIndex{0};
        int health{3};
        bool alive{true};
    };

    bool loadEnemyModel(const std::string& path, const glm::vec3& fallbackColor, EnemyModel& model);
    std::shared_ptr<Texture2D> loadEnemyTexture(const std::string& path);
    std::shared_ptr<Texture2D> loadFirstEnemyTexture(const std::vector<std::string>& paths);
    glm::vec3 findSpawnPosition(const Environment& environment, const glm::vec3& playerSpawn, const glm::vec2& anchor) const;
    bool tryMoveEnemy(Enemy& enemy, const Environment& environment, const glm::vec3& step) const;
    bool findFloorAt(const Environment& environment, float x, float z, float preferredY, float& floorY) const;
    Bounds enemyBounds(const Enemy& enemy) const;
    glm::mat4 enemyModelMatrix(const Enemy& enemy, float timeSeconds) const;
    void buildFallbackModel(const glm::vec3& color, EnemyModel& model);

    std::array<EnemyModel, 2> m_models;
    std::vector<Enemy> m_enemies;
    std::vector<std::shared_ptr<Texture2D>> m_textures;
    bool m_initialized{false};
};

// junta todo el estado vivo de mapa 4 para poder entrar, salir y reanudar la sesión
struct Mapa4Runtime {
    Environment environment;
    Player player;
    MissionManager mission;
    Map4LightManager lightPickups;
    SimpleEnemyManager enemies;
    AudioPlayer music;
    AudioPlayer coinSound;
    bool initialized{false};
    bool sessionActive{false};
    bool startSequencePending{false};
    bool skipFirstUpdateFrame{false};
    bool musicOpen{false};
    bool musicPlaying{false};
    bool coinSoundOpen{false};
    int lastCollectedCount{0};
    float coinCollectDelay{0.0f};
    int health{3};
    int maxHealth{3};
    float damageCooldown{0.0f};
    int pendingHits{0};
    bool shieldActive{false};
    float shieldTimer{0.0f};
    float lightEnergy{20.0f};
    bool gameOver{false};
    std::vector<Mapa4Projectile> projectiles;
    float projectileCooldown{0.0f};
};

bool isMarioMapa4Environment(const Environment& environment);
bool iniciarMapa4(Mapa4Runtime& mapa4);
void volverAlMenu(Mapa4Runtime& mapa4);
void renderMapa4(GLFWwindow* window, Mapa4Runtime& mapa4, const Shader& sceneShader, const Shader& lavaShader, float now);
void renderMapa4Hud(MenuContext& menu, const Mapa4Runtime& mapa4, int width, int height, float now);
