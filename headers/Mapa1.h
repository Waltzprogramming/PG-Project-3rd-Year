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

    bool initialize();
    void render(GLFWwindow* window, float deltaTime);
    void shutdown();

    int collectedCount() const;
    int messageCount() const;
    bool showCoinMessage(float timeSeconds) const;
    bool showStarMessage(float timeSeconds) const;
    bool levelComplete() const;
    const Texture2D& coinIconTexture() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
