#pragma once

#include "Environment.h"
#include "Player.h"
#include "Texture2D.h"

#include <glm/glm.hpp>
#include <memory>
#include <vector>

struct MissionRenderablePart {
    Mesh mesh;
    Material material;
    glm::vec3 localPosition{0.0f};
    glm::vec3 localRotation{0.0f};
    glm::vec3 localScale{1.0f};
};

struct Coin {
    glm::vec3 position{0.0f};
    bool collected{false};
    float phase{0.0f};
};

struct Star {
    glm::vec3 position{0.0f};
    bool active{false};
};

class MissionManager {
public:
    bool initialize();
    void reset(const Environment& environment, const glm::vec3& playerSpawn);
    void setCompleteOnAllCoins(bool enabled) { m_completeOnAllCoins = enabled; }
    void generarMonedas(const Environment& environment, const glm::vec3& playerSpawn);
    void update(const Player& player, float timeSeconds);
    void render(const Shader& shader, float timeSeconds, const glm::vec3& cameraPosition) const;
    void forceComplete(float timeSeconds);

    int collectedCount() const { return m_collectedCount; }
    int messageCount() const { return m_messageCount; }
    bool showCoinMessage(float timeSeconds) const { return timeSeconds <= static_cast<float>(m_coinMessageUntil); }
    bool showStarMessage(float timeSeconds) const { return timeSeconds <= static_cast<float>(m_starMessageUntil); }
    bool starFocusActive(float timeSeconds) const { return timeSeconds <= static_cast<float>(m_starFocusUntil); }
    glm::vec3 starPosition() const { return m_star.position; }
    bool levelComplete() const { return m_levelComplete; }
    const Texture2D& coinIconTexture() const { return m_coinIcon; }

private:
    bool loadCoinModel();
    bool loadStarModel();
    std::shared_ptr<Texture2D> loadMissionTexture(const std::string& path);
    std::shared_ptr<Texture2D> loadMissionTexture(const LoadedMaterial& material);
    std::shared_ptr<Texture2D> loadFirstAvailableTexture(const std::vector<std::string>& paths);
    Mesh createStarMesh() const;
    Bounds coinBounds(const Coin& coin) const;
    Bounds starBounds() const;
    glm::mat4 coinModelMatrix(const Coin& coin, float timeSeconds) const;
    glm::mat4 starModelMatrix(float timeSeconds) const;
    bool validCoinPosition(const glm::vec3& position, const std::vector<Bounds>& colliders, const std::vector<Coin>& placed, const glm::vec3& playerSpawn, const glm::vec3& worldMin, const glm::vec3& worldMax, float minCoinDistance) const;
    void recolectarMoneda(Coin& coin, float timeSeconds);
    void mostrarMensajeMonedaTemporal(float timeSeconds);
    void activarEstrella(float timeSeconds);
    void completarNivel(float timeSeconds);

    std::vector<MissionRenderablePart> m_coinParts;
    std::vector<MissionRenderablePart> m_starParts;
    std::vector<std::shared_ptr<Texture2D>> m_textures;
    Texture2D m_coinIcon;
    Mesh m_fallbackCoinMesh;
    Mesh m_starMesh;
    Material m_fallbackCoinMaterial;
    Material m_starMaterial;
    glm::vec3 m_coinModelMin{0.0f};
    glm::vec3 m_coinModelMax{0.0f, 1.0f, 0.0f};
    glm::vec3 m_coinModelCenter{0.0f};
    glm::vec3 m_starModelMin{0.0f};
    glm::vec3 m_starModelMax{0.0f, 1.0f, 0.0f};
    glm::vec3 m_starModelCenter{0.0f};
    float m_coinModelScale{1.0f};
    float m_starModelScale{0.95f};
    std::vector<Coin> m_coins;
    Star m_star;
    int m_collectedCount{0};
    int m_messageCount{0};
    double m_coinMessageUntil{0.0};
    double m_starMessageUntil{0.0};
    double m_starFocusUntil{0.0};
    double m_victoryTime{0.0};
    bool m_initialized{false};
    bool m_levelComplete{false};
    bool m_completeOnAllCoins{false};
    bool m_marioMapStyle{false};
};
