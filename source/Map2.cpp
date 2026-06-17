#include "Map2.h"

#include "GameRuntime.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

namespace {

void initializeMundo2HudResources(Mundo2HudResources& hud) {
    if (hud.initialized) {
        return;
    }

    const glm::vec3 white(1.0f);
    const glm::vec3 titleColor(1.0f, 0.92f, 0.35f);
    hud.promptHablarToad = createTextSprite(L"Pulsa F para hablar", 28, white, 330, false, true);
    hud.nombreToad = createTextSprite(L"Toad", 30, titleColor, 170, false, true);
    hud.dialogoToad = createTextSprite(
        L"\u00a1Oh nooo! Todos estos enemigos vinieron a estorbar...\n"
        L"Para ganar, recoge las 10 monedas del mapa. Cuando las tengas,\n"
        L"aparecer\u00e1 una estrella de cristal. T\u00f3mala para completar el nivel.",
        21, white, 790, true, false);
    hud.initialized = true;
}

void drawToadHud(MenuContext& menu, const Mundo2HudResources& hud, const ToadNpc& toad, int width, int height) {
    if (!toad.showPrompt() && !toad.dialogOpen()) {
        return;
    }

    beginUiFrame(menu, width, height);
    if (toad.dialogOpen()) {
        const float panelWidth = std::min(880.0f, static_cast<float>(width) - 96.0f);
        const Rect panel = centeredRect(width * 0.5f, static_cast<float>(height) - 250.0f, panelWidth, 205.0f);
        drawPanel(menu, panel);
        drawText(menu, hud.nombreToad, panel.x + 30.0f, panel.y + 18.0f);
        drawText(menu, hud.dialogoToad, panel.x + (panel.width - hud.dialogoToad.size.x) * 0.5f, panel.y + 58.0f);
    } else {
        const Rect prompt = centeredRect(width * 0.5f, static_cast<float>(height) - 108.0f, 360.0f, 54.0f);
        drawRect(menu, {prompt.x + 6.0f, prompt.y + 7.0f, prompt.width, prompt.height}, {0.01f, 0.02f, 0.05f, 0.45f});
        drawRect(menu, prompt, {0.07f, 0.20f, 0.38f, 0.92f});
        drawText(menu, hud.promptHablarToad, prompt.x + (prompt.width - hud.promptHablarToad.size.x) * 0.5f, prompt.y + (prompt.height - hud.promptHablarToad.size.y) * 0.5f);
    }
}

} // namespace

bool ToadNpc::initialize() {
    if (m_initialized) {
        return true;
    }

    buildFallbackModel();
    m_initialized = true;
    return true;
}

std::shared_ptr<Texture2D> ToadNpc::loadNpcTexture(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    const std::filesystem::path candidate = std::filesystem::path(resolveAssetPath(path));
    if (!std::filesystem::exists(candidate)) {
        return nullptr;
    }

    const std::string normalized = std::filesystem::weakly_canonical(candidate).string();
    for (const auto& texture : m_textures) {
        if (texture && texture->sourcePath() == normalized) {
            return texture;
        }
    }

    auto texture = std::make_shared<Texture2D>();
    if (!texture->loadFromFile(normalized, false)) {
        return nullptr;
    }

    m_textures.push_back(texture);
    return texture;
}

void ToadNpc::buildFallbackModel() {
    m_parts.clear();
    auto makePart = [&](Mesh mesh, const glm::vec3& scale, const glm::vec3& position, const glm::vec3& color, const std::string& texturePath = std::string()) {
        MissionRenderablePart part;
        part.mesh = std::move(mesh);
        part.localScale = scale;
        part.localPosition = position;
        part.material.baseColor = color;
        part.material.roughness = 0.78f;
        part.material.fogAmount = 0.20f;
        part.material.texture = loadNpcTexture(texturePath);
        m_parts.push_back(std::move(part));
    };

    makePart(Mesh::cube(), {0.62f, 0.52f, 0.62f}, {0.0f, 0.36f, 0.0f}, {0.96f, 0.92f, 0.85f}, "assets/npcs/RussT/Russ T/Toad (torso).png");
    makePart(Mesh::cube(), {0.88f, 0.48f, 0.88f}, {0.0f, 0.98f, 0.0f}, {0.98f, 0.96f, 0.92f}, "assets/npcs/RussT/Russ T/Toad (head).png");
    makePart(Mesh::cube(), {0.18f, 0.18f, 0.12f}, {-0.16f, 0.96f, 0.43f}, {0.08f, 0.08f, 0.10f}, "assets/npcs/RussT/Russ T/Toad (eyes).png");
    makePart(Mesh::cube(), {0.18f, 0.18f, 0.12f}, {0.16f, 0.96f, 0.43f}, {0.08f, 0.08f, 0.10f}, "assets/npcs/RussT/Russ T/Toad (eyes).png");
    makePart(Mesh::cube(), {0.18f, 0.34f, 0.18f}, {-0.34f, 0.46f, 0.0f}, {0.95f, 0.82f, 0.62f}, "assets/npcs/RussT/Russ T/Toad (hand).png");
    makePart(Mesh::cube(), {0.18f, 0.34f, 0.18f}, {0.34f, 0.46f, 0.0f}, {0.95f, 0.82f, 0.62f}, "assets/npcs/RussT/Russ T/Toad (hand).png");
    makePart(Mesh::cube(), {0.20f, 0.28f, 0.20f}, {-0.14f, 0.02f, 0.0f}, {0.54f, 0.26f, 0.16f}, "assets/npcs/RussT/Russ T/Toad (shoe).png");
    makePart(Mesh::cube(), {0.20f, 0.28f, 0.20f}, {0.14f, 0.02f, 0.0f}, {0.54f, 0.26f, 0.16f}, "assets/npcs/RussT/Russ T/Toad (shoe).png");

    m_modelMin = {-0.5f, -0.5f, -0.5f};
    m_modelMax = {0.5f, 1.5f, 0.5f};
    m_modelCenter = (m_modelMin + m_modelMax) * 0.5f;
    m_modelScale = 0.98f / std::max(m_modelMax.y - m_modelMin.y, 0.001f);
}

void ToadNpc::reset(const Environment& environment, const glm::vec3& playerSpawn) {
    m_position = findSafePosition(environment, playerSpawn);
    m_facingYaw = 0.0f;
    m_playerNearby = false;
    m_dialogOpen = false;
}

void ToadNpc::update(const Player& player, bool interactPressed, float) {
    const glm::vec3 delta = player.position() - m_position;
    const float distance = glm::length(glm::vec2(delta.x, delta.z));
    m_playerNearby = distance <= 2.45f && std::abs(delta.y) <= 2.2f;

    if (distance > 0.05f) {
        m_facingYaw = std::atan2(delta.x, delta.z);
    }

    if (m_playerNearby && interactPressed) {
        m_dialogOpen = !m_dialogOpen;
    } else if (!m_playerNearby) {
        m_dialogOpen = false;
    }
}

void ToadNpc::render(const Shader& shader, float timeSeconds) const {
    if (!m_initialized) {
        return;
    }

    const glm::mat4 model = modelMatrix(timeSeconds);
    shader.use();
    shader.setFloat("uTime", timeSeconds);
    for (const MissionRenderablePart& part : m_parts) {
        shader.setMat4("uModel", model * localPartMatrix(part));
        bindSceneMaterial(shader, part.material);
        part.mesh.draw();
    }
}

glm::mat4 ToadNpc::modelMatrix(float timeSeconds) const {
    const float bob = std::sin(timeSeconds * 3.2f) * 0.035f;
    const float sway = std::sin(timeSeconds * 2.1f) * 0.055f;
    glm::mat4 model(1.0f);
    model = glm::translate(model, m_position + glm::vec3(0.0f, bob, 0.0f));
    model = glm::rotate(model, m_facingYaw + sway, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(m_modelScale));
    model = glm::translate(model, {-m_modelCenter.x, -m_modelMin.y, -m_modelCenter.z});
    return model;
}

glm::vec3 ToadNpc::findSafePosition(const Environment& environment, const glm::vec3& playerSpawn) const {
    const auto& colliders = environment.collisionPreview();
    const glm::vec2 target{playerSpawn.x - 1.65f, playerSpawn.z + 1.35f};
    glm::vec3 best = playerSpawn + glm::vec3(-1.65f, 0.05f, 1.35f);
    float bestScore = std::numeric_limits<float>::max();

    for (const Bounds& collider : colliders) {
        const float top = collider.center.y + collider.halfExtent.y;
        const float area = (collider.halfExtent.x * 2.0f) * (collider.halfExtent.z * 2.0f);
        const bool floorLike = collider.halfExtent.y <= 0.30f && area >= 0.45f && collider.halfExtent.x >= 0.28f && collider.halfExtent.z >= 0.28f;
        if (!floorLike) {
            continue;
        }

        const float safeX = std::max(collider.halfExtent.x - 0.75f, 0.0f);
        const float safeZ = std::max(collider.halfExtent.z - 0.75f, 0.0f);
        glm::vec3 candidate{
            std::clamp(target.x, collider.center.x - safeX, collider.center.x + safeX),
            top + 0.05f,
            std::clamp(target.y, collider.center.z - safeZ, collider.center.z + safeZ)
        };

        const float score = glm::length(glm::vec2(candidate.x - target.x, candidate.z - target.y)) + std::abs(candidate.y - playerSpawn.y) * 0.4f;
        if (score < bestScore) {
            bestScore = score;
            best = candidate;
        }
    }

    return best;
}

bool iniciarMundo2(Mundo2Runtime& mundo2) {
    if (mundo2.initialized) {
        if (mundo2.musicOpen && !mundo2.musicPlaying) {
            mundo2.musicPlaying = mundo2.music.playLoop();
        }
        return true;
    }

    initializeMundo2HudResources(mundo2.hud);
    mundo2.environment.create();
    mundo2.musicOpen = mundo2.music.open(resolveAssetPath("assets/audio/graffiti_underground_loop.mp3"));
    if (mundo2.musicOpen) {
        mundo2.musicPlaying = mundo2.music.playLoop();
    } else {
        std::cerr << "Background music could not be started." << std::endl;
    }

    mundo2.player.load(resolveAssetPath("assets/characters/mario64_pinix_style/model/scene.gltf"));
    mundo2.player.spawnAt(mundo2.environment.recommendedSpawnPoint());
    mundo2.mission.initialize();
    mundo2.mission.reset(mundo2.environment, mundo2.environment.recommendedSpawnPoint());
    mundo2.toad.initialize();
    mundo2.toad.reset(mundo2.environment, mundo2.environment.recommendedSpawnPoint());
    mundo2.lastInteractKey = false;
    resetGameplayView(mundo2.player);

    std::cout << "Mundo 2 ready. Collision volumes: " << mundo2.environment.collisionPreview().size() << std::endl;
    std::cout << "Controls 3D: WASD move, mouse camera, Space jump, TAB switch to 2D, Esc back to menu." << std::endl;
    std::cout << "Controls 2D: A/D move, Space jump, TAB switch to 3D." << std::endl;
    mundo2.initialized = true;
    return true;
}

void volverAlMenu(Mundo2Runtime& mundo2) {
    if (mundo2.musicOpen && mundo2.musicPlaying) {
        mundo2.music.stop();
        mundo2.musicPlaying = false;
    }
    if (mundo2.musicOpen) {
        mundo2.music.close();
        mundo2.musicOpen = false;
    }
    mundo2.lastInteractKey = false;
    mundo2.initialized = false;
}

bool pausarMusicaMundo2(Mundo2Runtime& mundo2) {
    const bool shouldResume = mundo2.musicOpen && mundo2.musicPlaying;
    if (shouldResume) {
        mundo2.music.stop();
        mundo2.musicPlaying = false;
    }
    return shouldResume;
}

void reanudarMusicaMundo2(Mundo2Runtime& mundo2, bool shouldResume) {
    if (shouldResume && mundo2.musicOpen && !mundo2.musicPlaying) {
        mundo2.musicPlaying = mundo2.music.playLoop();
    }
}

void renderMundo2(GLFWwindow* window, Mundo2Runtime& mundo2, MenuContext& menu, const Shader& sceneShader, const Shader& lavaShader, float now) {
    const PlayerInput playerInput = buildPlayerInput(window, mundo2.player);
    const bool interactDown = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    const bool interactPressed = interactDown && !mundo2.lastInteractKey;
    mundo2.lastInteractKey = interactDown;

    std::vector<Bounds> playerColliders = mundo2.environment.collisionPreview();
    appendDimensionRestrictionColliders(playerColliders, mundo2.environment, locked2DDepth);
    mundo2.player.update(playerInput, playerColliders, mundo2.environment.worldMin(), mundo2.environment.worldMax(), deltaTime);
    mundo2.mission.update(mundo2.player, now);
    mundo2.toad.update(mundo2.player, interactPressed, now);
    updateGameplayCamera(mundo2.player, mundo2.environment, mundo2.mission, now, deltaTime);

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    const glm::mat4 view = glm::lookAt(gameplayCameraPosition, gameplayCameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 180.0f);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.018f, 0.026f, 0.040f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    uploadCommonSceneUniforms(sceneShader, mundo2.environment, gameplayCameraPosition, view, projection, now, nullptr, 1.0f, nullptr);
    lavaShader.use();
    lavaShader.setMat4("uView", view);
    lavaShader.setMat4("uProjection", projection);
    lavaShader.setFloat("uTime", now);

    mundo2.environment.render(sceneShader, lavaShader, now, gameplayCameraPosition);
    mundo2.mission.render(sceneShader, now, gameplayCameraPosition);
    mundo2.toad.render(sceneShader, now);
    mundo2.player.render(sceneShader);
    drawMissionManagerHud(menu, mundo2.mission, width, height, now);
    drawToadHud(menu, mundo2.hud, mundo2.toad, width, height);
}
