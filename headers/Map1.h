#pragma once

#include "AudioPlayer.h"
#include "Environment.h"
#include "GameSystems.h"
#include "GameUI.h"
#include "Player.h"
#include "Shader.h"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

class ToadNpc {
public:
    bool initialize();
    void reset(const Environment& environment, const glm::vec3& playerSpawn);
    void update(const Player& player, bool interactPressed, float timeSeconds);
    void render(const Shader& shader, float timeSeconds) const;

    bool showPrompt() const { return m_playerNearby && !m_dialogOpen; }
    bool dialogOpen() const { return m_dialogOpen; }

private:
    std::shared_ptr<Texture2D> loadNpcTexture(const std::string& path);
    void buildFallbackModel();
    glm::mat4 modelMatrix(float timeSeconds) const;
    glm::vec3 findSafePosition(const Environment& environment, const glm::vec3& playerSpawn) const;

    std::vector<MissionRenderablePart> m_parts;
    std::vector<std::shared_ptr<Texture2D>> m_textures;
    glm::vec3 m_position{0.0f};
    glm::vec3 m_modelMin{0.0f};
    glm::vec3 m_modelMax{0.0f, 1.0f, 0.0f};
    glm::vec3 m_modelCenter{0.0f};
    float m_modelScale{1.0f};
    float m_facingYaw{0.0f};
    bool m_initialized{false};
    bool m_playerNearby{false};
    bool m_dialogOpen{false};
};

struct Mundo2HudResources {
    TextSprite promptHablarToad;
    TextSprite nombreToad;
    TextSprite dialogoToad;
    bool initialized{false};
};

struct Mundo2Runtime {
    Environment environment;
    Player player;
    MissionManager mission;
    ToadNpc toad;
    AudioPlayer music;
    Mundo2HudResources hud;
    std::vector<Bounds> collisionBounds;
    glm::vec3 safePlayerPosition{0.0f};
    glm::vec3 cameraLead{0.0f};
    glm::vec3 previousCameraPlayerPosition{0.0f};
    double jumpBufferUntil{0.0};
    bool initialized{false};
    bool musicOpen{false};
    bool musicPlaying{false};
    bool lastInteractKey{false};
    bool hasSafePlayerPosition{false};
};

bool iniciarMundo2(Mundo2Runtime& mundo2);
void volverAlMenu(Mundo2Runtime& mundo2);
bool pausarMusicaMundo2(Mundo2Runtime& mundo2);
void reanudarMusicaMundo2(Mundo2Runtime& mundo2, bool shouldResume);
void renderMundo2(GLFWwindow* window, Mundo2Runtime& mundo2, MenuContext& menu, const Shader& sceneShader, const Shader& lavaShader, float now);
