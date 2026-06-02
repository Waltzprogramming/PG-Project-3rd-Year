#pragma once

#include <memory>

struct GLFWwindow;
class Texture2D;

class Mapa1 {
public:
    Mapa1();
    Mapa1(const Mapa1&) = delete;
    Mapa1& operator=(const Mapa1&) = delete;
    ~Mapa1();

    bool initialize(bool enableAudio = true);
    bool runCombatSmokeTest();
    void render(GLFWwindow* window, float deltaTime);
    void shutdown();

    int collectedCount() const;
    int messageCount() const;
    bool showCoinMessage(float timeSeconds) const;
    bool showStarMessage(float timeSeconds) const;
    bool showCombatHint(float timeSeconds) const;
    bool levelComplete() const;
    int remainingEnemyCount() const;
    int currentHealth() const;
    int maximumHealth() const;
    float chargeRatio() const;
    bool chargingAttack() const;
    bool parryActive(float timeSeconds) const;
    const Texture2D& coinIconTexture() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
