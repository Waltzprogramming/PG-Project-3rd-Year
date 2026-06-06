#include "Map3.h"

#include "GameRuntime.h"
#include "ModelLoader.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

namespace {
constexpr float Map3EnemySpeed3D = 1.18f;
constexpr float Map3EnemySpeed2D = 1.38f;
constexpr float Map3EnemyDetectionRange = 6.2f;
constexpr float Map3EnemyHitCooldown = 0.85f;
constexpr float Map3DodgeDistance = 1.75f;
constexpr float Map3DodgeCooldown = 0.72f;
constexpr float Map3DodgeActiveTime = 0.28f;
constexpr float Map3ParryActiveTime = 0.42f;
constexpr float Map3ParryRadius = 1.24f;

bool map3BoundsIntersect(const Bounds& a, const Bounds& b) {
    const glm::vec3 delta = glm::abs(a.center - b.center);
    const glm::vec3 total = a.halfExtent + b.halfExtent;
    return delta.x < total.x && delta.y < total.y && delta.z < total.z;
}

glm::vec3 map3MoveDirectionFromInput(const PlayerInput& input) {
    glm::vec3 direction(0.0f);
    if (currentMode == PlayMode::Mode3D) {
        const glm::vec3 cameraForward = glm::normalize(glm::vec3(-std::sin(input.cameraYawRadians), 0.0f, -std::cos(input.cameraYawRadians)));
        const glm::vec3 cameraRight = glm::normalize(glm::vec3(std::cos(input.cameraYawRadians), 0.0f, -std::sin(input.cameraYawRadians)));
        direction = cameraRight * input.move.x + cameraForward * input.move.y;
    } else {
        direction = {input.move.x, 0.0f, 0.0f};
    }

    if (glm::length(direction) <= 0.05f) {
        return {std::sin(input.cameraYawRadians), 0.0f, std::cos(input.cameraYawRadians)};
    }
    return glm::normalize(direction);
}

bool positionOverlapsWorld(const Bounds& bounds, const std::vector<Bounds>& colliders) {
    for (const Bounds& collider : colliders) {
        if (map3BoundsIntersect(bounds, collider)) {
            return true;
        }
    }
    return false;
}

bool findFloorAt(const Environment& environment, float x, float z, float preferredY, float& floorY) {
    bool found = false;
    float bestScore = std::numeric_limits<float>::max();
    for (const Bounds& collider : environment.collisionPreview()) {
        const float top = collider.center.y + collider.halfExtent.y;
        const float area = (collider.halfExtent.x * 2.0f) * (collider.halfExtent.z * 2.0f);
        const bool floorLike = collider.halfExtent.y <= 0.34f && area >= 0.24f;
        const bool inside =
            x >= collider.center.x - collider.halfExtent.x - 0.08f &&
            x <= collider.center.x + collider.halfExtent.x + 0.08f &&
            z >= collider.center.z - collider.halfExtent.z - 0.08f &&
            z <= collider.center.z + collider.halfExtent.z + 0.08f;
        if (!floorLike || !inside) {
            continue;
        }

        const float score = std::abs(top - preferredY);
        if (score < bestScore) {
            bestScore = score;
            floorY = top;
            found = true;
        }
    }
    return found;
}

bool tryDodgePlayer(Player& player, const Environment& environment, const PlayerInput& input) {
    const glm::vec3 direction = map3MoveDirectionFromInput(input);
    const glm::vec3 current = player.position();
    glm::vec3 target = current + direction * Map3DodgeDistance;
    const glm::vec3 worldMin = environment.worldMin();
    const glm::vec3 worldMax = environment.worldMax();

    target.x = std::clamp(target.x, worldMin.x + 0.36f, worldMax.x - 0.36f);
    target.z = std::clamp(target.z, worldMin.z + 0.36f, worldMax.z - 0.36f);
    float floorY = target.y;
    if (findFloorAt(environment, target.x, target.z, current.y, floorY)) {
        target.y = floorY + 0.05f;
    }

    Bounds candidate = player.bounds();
    candidate.center += target - current;
    if (positionOverlapsWorld(candidate, environment.collisionPreview())) {
        return false;
    }

    player.teleportTo(target);
    return true;
}

void resetMap3View(const Player& player) {
    currentMode = PlayMode::Mode3D;
    lastToggleKey = false;
    lastJumpKey = false;
    lastShieldKey = false;
    cameraYawDegrees = 0.0f;
    cameraPitchDegrees = 18.0f;
    locked2DDepth = player.position().z;
    cameraInitialized = false;
}

Mesh createMap3ActionMesh() {
    return Mesh::sphere(24, 12, 1.0f);
}

void renderMap3ActionEffect(const Shader& shader, const Player& player, float timeSeconds, bool parryActive, bool dodgeActive) {
    if (!parryActive && !dodgeActive) {
        return;
    }

    static Mesh effectMesh = createMap3ActionMesh();
    Material material;
    material.baseColor = parryActive ? glm::vec3(0.22f, 1.0f, 0.94f) : glm::vec3(1.0f, 0.82f, 0.18f);
    material.emissive = parryActive ? glm::vec3(0.03f, 0.16f, 0.14f) : glm::vec3(0.18f, 0.12f, 0.02f);
    material.roughness = 0.9f;
    material.fogAmount = 0.05f;
    material.opacity = parryActive ? 0.22f : 0.16f;

    const float pulse = 0.84f + std::sin(timeSeconds * 18.0f) * 0.05f;
    glm::mat4 model(1.0f);
    model = glm::translate(model, player.position() + glm::vec3(0.0f, 0.52f, 0.0f));
    model = glm::scale(model, glm::vec3(0.78f, 0.62f, 0.78f) * pulse);

    shader.use();
    shader.setFloat("uTime", timeSeconds);
    shader.setMat4("uModel", model);
    bindSceneMaterial(shader, material);
    effectMesh.draw();
}

bool loadMap3Environment(Environment& environment) {
    const std::array<std::string, 8> candidates = {
        "assets/mundo3/Wii - Mario Kart Wii - Courses - Mario Circuit/Mario Circuit/course.dae",
        "assets/mundo3/Wii - Mario Kart Wii - Courses - Mario Circuit/Mario Circuit/course_fix.dae",
        "assets/mundo3/Wii - Mario Kart Wii - Courses - Mario Circuit/Mario Circuit",
        "assets/mundo3/Wii - Mario Kart Wii - Courses - Mario Circuit/Mario Circuit/map.dae",
        "assets/mundo3/Wii - Mario Kart Wii - Courses - Mario Circuit/Mario Circuit/course_d.dae",
        "assets/Mundos/FreezeezyPeak/Freezeezy Peak.dae",
        "assets/mapa1/world1/CourseSelectW1.dae",
        "assets/mapa 4/mapamian/World 1/World 1/CourseSelectW1.dae"
    };

    for (const std::string& candidate : candidates) {
        const std::string resolved = resolveAssetPath(candidate);
        environment.create(resolved, true);
        if (environmentUsable(environment)) {
            std::cout << "Map3 environment loaded from: " << resolved << std::endl;
            return true;
        }
        std::cerr << "Map3 skipped unusable environment: " << resolved << std::endl;
    }

    environment.create();
    if (environmentUsable(environment)) {
        std::cout << "Map3 environment loaded from default world." << std::endl;
        return true;
    }

    return false;
}
}

bool Map3EnemyManager::initialize() {
    if (m_initialized) {
        return true;
    }

    if (!loadEnemyModel()) {
        buildFallbackModel();
    }

    m_initialized = true;
    return true;
}

void Map3EnemyManager::reset(const Environment& environment, const glm::vec3& playerSpawn) {
    m_enemies.clear();
    const glm::vec3 worldMin = environment.worldMin();
    const glm::vec3 worldMax = environment.worldMax();
    const glm::vec2 center((worldMin.x + worldMax.x) * 0.5f, (worldMin.z + worldMax.z) * 0.5f);
    const glm::vec2 span(std::max(worldMax.x - worldMin.x, 1.0f), std::max(worldMax.z - worldMin.z, 1.0f));
    const std::array<glm::vec2, 7> anchors = {
        center + glm::vec2(-span.x * 0.30f, -span.y * 0.22f),
        center + glm::vec2(-span.x * 0.12f, span.y * 0.18f),
        center + glm::vec2(span.x * 0.12f, -span.y * 0.18f),
        center + glm::vec2(span.x * 0.30f, span.y * 0.18f),
        center + glm::vec2(-span.x * 0.22f, span.y * 0.32f),
        center + glm::vec2(span.x * 0.22f, -span.y * 0.32f),
        center + glm::vec2(0.0f, span.y * 0.04f)
    };

    for (size_t index = 0; index < anchors.size(); ++index) {
        Enemy enemy;
        enemy.position = findSpawnPosition(environment, playerSpawn, anchors[index]);
        enemy.spawnPosition = enemy.position;
        enemy.phase = static_cast<float>(index) * 1.21f;
        enemy.health = 2;
        enemy.alive = true;
        m_enemies.push_back(enemy);
    }
}

bool Map3EnemyManager::update(const Player& player, const Environment& environment, float deltaTime, float timeSeconds, bool parryActive, bool dodgeActive) {
    bool hitPlayer = false;
    const glm::vec3 playerPosition = player.position();

    for (Enemy& enemy : m_enemies) {
        if (!enemy.alive) {
            continue;
        }

        enemy.hurtTimer = std::max(0.0f, enemy.hurtTimer - deltaTime);
        glm::vec3 toPlayer = playerPosition - enemy.position;
        if (currentMode == PlayMode::Mode2D) {
            toPlayer.z = 0.0f;
            enemy.position.z += (locked2DDepth - enemy.position.z) * std::min(1.0f, deltaTime * 5.0f);
        }

        const float distance = glm::length(toPlayer);
        if (parryActive && currentMode == PlayMode::Mode2D && distance <= Map3ParryRadius) {
            enemy.health -= 1;
            enemy.hurtTimer = 0.28f;
            if (enemy.health <= 0) {
                enemy.alive = false;
                continue;
            }

            const glm::vec3 push = glm::length(toPlayer) > 0.05f ? -glm::normalize(toPlayer) * 0.62f : glm::vec3(0.62f, 0.0f, 0.0f);
            tryMoveEnemy(enemy, environment, push);
            continue;
        }

        if (distance <= Map3EnemyDetectionRange && distance > 0.08f) {
            const float speed = currentMode == PlayMode::Mode2D ? Map3EnemySpeed2D : Map3EnemySpeed3D;
            glm::vec3 direction = glm::normalize(toPlayer);
            direction.y = 0.0f;
            tryMoveEnemy(enemy, environment, direction * speed * deltaTime);
            enemy.yaw = std::atan2(direction.x, direction.z);
        } else {
            const float drift = std::sin(timeSeconds * 0.9f + enemy.phase) * 0.36f;
            const glm::vec3 target = enemy.spawnPosition + glm::vec3(drift, 0.0f, currentMode == PlayMode::Mode2D ? 0.0f : std::cos(timeSeconds * 0.7f + enemy.phase) * 0.28f);
            glm::vec3 direction = target - enemy.position;
            direction.y = 0.0f;
            if (glm::length(direction) > 0.05f) {
                direction = glm::normalize(direction);
                tryMoveEnemy(enemy, environment, direction * Map3EnemySpeed3D * 0.45f * deltaTime);
                enemy.yaw = std::atan2(direction.x, direction.z);
            }
        }

        if (!dodgeActive && !parryActive && map3BoundsIntersect(player.bounds(), enemyBounds(enemy))) {
            hitPlayer = true;
        }
    }

    return hitPlayer;
}

void Map3EnemyManager::render(const Shader& shader, float timeSeconds, const glm::vec3& cameraPosition) const {
    shader.use();
    shader.setFloat("uTime", timeSeconds);

    for (const Enemy& enemy : m_enemies) {
        if (!enemy.alive || glm::length(enemy.position - cameraPosition) > 32.0f) {
            continue;
        }

        const glm::mat4 model = enemyModelMatrix(enemy, timeSeconds);
        if (!m_parts.empty()) {
            for (const MissionRenderablePart& part : m_parts) {
                Material material = part.material;
                if (enemy.hurtTimer > 0.0f) {
                    material.baseColor = glm::mix(material.baseColor, glm::vec3(1.0f, 0.25f, 0.18f), 0.65f);
                    material.emissive += glm::vec3(0.25f, 0.02f, 0.01f);
                }
                shader.setMat4("uModel", model * localPartMatrix(part));
                bindSceneMaterial(shader, material);
                part.mesh.draw();
            }
        } else {
            Material material = m_fallbackMaterial;
            if (enemy.hurtTimer > 0.0f) {
                material.baseColor = {1.0f, 0.22f, 0.18f};
            }
            shader.setMat4("uModel", model);
            bindSceneMaterial(shader, material);
            m_fallbackMesh.draw();
        }
    }
}

int Map3EnemyManager::aliveCount() const {
    return static_cast<int>(std::count_if(m_enemies.begin(), m_enemies.end(), [](const Enemy& enemy) {
        return enemy.alive;
    }));
}

bool Map3EnemyManager::loadEnemyModel() {
    const std::string enemyPath = resolveFirstExistingAsset({
        "assets/mundo3/Nintendo Switch - Super Mario Odyssey - Enemies (2D) - Fuzzy (2D)/Fuzzy (2D)/Fuzzy (2D)/Chorobon2D.dae"
    });
    LoadedModel model = ModelLoader::loadModel(enemyPath);
    if (model.meshes.empty()) {
        std::cerr << "Map3 fuzzy enemy could not be loaded. Using fallback." << std::endl;
        return false;
    }

    m_modelMin = model.minBounds;
    m_modelMax = model.maxBounds;
    m_modelCenter = (m_modelMin + m_modelMax) * 0.5f;
    const glm::vec3 size = m_modelMax - m_modelMin;
    const float maxExtent = std::max({size.x, size.y, size.z, 0.001f});
    m_modelScale = 0.72f / maxExtent;

    m_parts.clear();
    m_parts.reserve(model.meshes.size());
    const std::filesystem::path modelPath(enemyPath);
    for (LoadedMesh& mesh : model.meshes) {
        MissionRenderablePart part;
        if (mesh.materialIndex < model.materials.size()) {
            const LoadedMaterial& material = model.materials[mesh.materialIndex];
            part.material.baseColor = material.diffuseColor;
            part.material.opacity = material.opacity;
            part.material.texture = loadTextureFromMaterial(material, modelPath, m_textures);
        }
        part.material.roughness = 0.74f;
        part.material.fogAmount = 0.12f;
        if (!part.material.texture) {
            part.material.baseColor = {0.08f, 0.08f, 0.10f};
        }
        part.mesh = std::move(mesh.mesh);
        m_parts.push_back(std::move(part));
    }
    return true;
}

void Map3EnemyManager::buildFallbackModel() {
    m_fallbackMesh = Mesh::sphere(20, 10, 0.5f);
    m_fallbackMaterial.baseColor = {0.05f, 0.05f, 0.06f};
    m_fallbackMaterial.emissive = {0.02f, 0.02f, 0.03f};
    m_fallbackMaterial.roughness = 0.8f;
    m_fallbackMaterial.fogAmount = 0.15f;
    m_modelMin = {-0.5f, -0.5f, -0.5f};
    m_modelMax = {0.5f, 0.5f, 0.5f};
    m_modelCenter = glm::vec3(0.0f);
    m_modelScale = 0.72f;
}

glm::vec3 Map3EnemyManager::findSpawnPosition(const Environment& environment, const glm::vec3& playerSpawn, const glm::vec2& anchor) const {
    glm::vec3 best{anchor.x, playerSpawn.y, anchor.y};
    float bestScore = std::numeric_limits<float>::max();

    for (const Bounds& collider : environment.collisionPreview()) {
        const float top = collider.center.y + collider.halfExtent.y;
        const float area = (collider.halfExtent.x * 2.0f) * (collider.halfExtent.z * 2.0f);
        const bool floorLike = collider.halfExtent.y <= 0.34f && area >= 0.28f;
        if (!floorLike || top < environment.worldMin().y - 0.1f || top > environment.worldMax().y + 0.4f) {
            continue;
        }

        const float safeX = std::max(collider.halfExtent.x - 0.45f, 0.0f);
        const float safeZ = std::max(collider.halfExtent.z - 0.45f, 0.0f);
        glm::vec3 candidate{
            std::clamp(anchor.x, collider.center.x - safeX, collider.center.x + safeX),
            top + 0.05f,
            std::clamp(anchor.y, collider.center.z - safeZ, collider.center.z + safeZ)
        };

        const float spawnDistance = glm::length(glm::vec2(candidate.x - playerSpawn.x, candidate.z - playerSpawn.z));
        if (spawnDistance < 2.2f) {
            continue;
        }

        const float score = glm::length(glm::vec2(candidate.x - anchor.x, candidate.z - anchor.y)) + std::abs(candidate.y - playerSpawn.y) * 0.45f;
        if (score < bestScore) {
            bestScore = score;
            best = candidate;
        }
    }

    return best;
}

bool Map3EnemyManager::findFloorAt(const Environment& environment, float x, float z, float preferredY, float& floorY) const {
    return ::findFloorAt(environment, x, z, preferredY, floorY);
}

bool Map3EnemyManager::tryMoveEnemy(Enemy& enemy, const Environment& environment, const glm::vec3& step) const {
    glm::vec3 target = enemy.position + step;
    target.x = std::clamp(target.x, environment.worldMin().x + 0.28f, environment.worldMax().x - 0.28f);
    target.z = std::clamp(target.z, environment.worldMin().z + 0.28f, environment.worldMax().z - 0.28f);

    float floorY = enemy.position.y - 0.05f;
    if (!findFloorAt(environment, target.x, target.z, enemy.position.y, floorY)) {
        return false;
    }
    target.y = floorY + 0.05f;

    const Bounds candidate{target + glm::vec3(0.0f, 0.34f, 0.0f), {0.34f, 0.34f, 0.34f}};
    for (const Bounds& collider : environment.collisionPreview()) {
        const float top = collider.center.y + collider.halfExtent.y;
        if (std::abs(top - floorY) <= 0.08f) {
            continue;
        }
        if (map3BoundsIntersect(candidate, collider)) {
            return false;
        }
    }

    enemy.position = target;
    return true;
}

Bounds Map3EnemyManager::enemyBounds(const Enemy& enemy) const {
    return {enemy.position + glm::vec3(0.0f, 0.36f, 0.0f), {0.36f, 0.36f, 0.36f}};
}

glm::mat4 Map3EnemyManager::enemyModelMatrix(const Enemy& enemy, float timeSeconds) const {
    const float bob = std::sin(timeSeconds * 4.8f + enemy.phase) * 0.04f;
    glm::mat4 model(1.0f);
    model = glm::translate(model, enemy.position + glm::vec3(0.0f, bob, 0.0f));
    model = glm::rotate(model, currentMode == PlayMode::Mode2D ? glm::half_pi<float>() : enemy.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(m_modelScale));
    model = glm::translate(model, {-m_modelCenter.x, -m_modelMin.y, -m_modelCenter.z});
    return model;
}

bool iniciarMap3(Map3Runtime& map3) {
    if (map3.initialized) {
        if (!map3.sessionActive) {
            const glm::vec3 spawnPoint = map3.environment.recommendedSpawnPoint();
            map3.player.spawnAt(spawnPoint);
            map3.mission.reset(map3.environment, spawnPoint);
            map3.enemies.reset(map3.environment, spawnPoint);
            map3.health = map3.maxHealth;
            map3.damageCooldown = 0.0f;
            map3.dodgeCooldown = 0.0f;
            map3.dodgeActiveUntil = 0.0f;
            map3.parryActiveUntil = 0.0f;
            map3.gameOver = false;
            map3.skipFirstUpdateFrame = true;
            resetMap3View(map3.player);
            map3.sessionActive = true;
        }
        return true;
    }

    if (!loadMap3Environment(map3.environment)) {
        std::cerr << "Map3 environment could not be initialized from any available candidate." << std::endl;
        return false;
    }

    const std::string playerPath = resolveFirstExistingAsset({
        "assets/mundo3/DS _ DSi - Flower Sun and Rain_ Murder and Mystery in Paradise - Playable Characters - Sumio Mondo (Horse)/SumioMondoS4/ch_01_switchskin.dae"
    });
    map3.player.load(playerPath);

    const glm::vec3 spawnPoint = map3.environment.recommendedSpawnPoint();
    map3.player.spawnAt(spawnPoint);
    map3.mission.initialize();
    map3.mission.reset(map3.environment, spawnPoint);
    map3.enemies.initialize();
    map3.enemies.reset(map3.environment, spawnPoint);
    map3.health = map3.maxHealth;
    map3.damageCooldown = 0.0f;
    map3.dodgeCooldown = 0.0f;
    map3.dodgeActiveUntil = 0.0f;
    map3.parryActiveUntil = 0.0f;
    map3.gameOver = false;
    map3.skipFirstUpdateFrame = true;
    resetMap3View(map3.player);

    std::cout << "Mundo 3 ready. Collision volumes: " << map3.environment.collisionPreview().size() << std::endl;
    std::cout << "Controls: TAB cambia 2D/3D, E esquiva en 3D y hace parry en 2D." << std::endl;

    map3.initialized = true;
    map3.sessionActive = true;
    return true;
}

void volverAlMenu(Map3Runtime& map3) {
    map3.sessionActive = false;
    map3.skipFirstUpdateFrame = false;
    map3.damageCooldown = 0.0f;
    map3.dodgeCooldown = 0.0f;
    map3.dodgeActiveUntil = 0.0f;
    map3.parryActiveUntil = 0.0f;
}

void renderMap3(GLFWwindow* window, Map3Runtime& map3, const Shader& sceneShader, const Shader& lavaShader, float now) {
    const float frameDelta = map3.skipFirstUpdateFrame ? 0.0f : deltaTime;
    map3.skipFirstUpdateFrame = false;

    const bool parryActive = now <= map3.parryActiveUntil;
    const bool dodgeActive = now <= map3.dodgeActiveUntil;

    if (!map3.gameOver && !map3.mission.levelComplete()) {
        PlayerInput playerInput = buildPlayerInput(window, map3.player);
        const bool actionDown = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
        const bool actionPressed = actionDown && !lastShieldKey;
        lastShieldKey = actionDown;

        map3.dodgeCooldown = std::max(0.0f, map3.dodgeCooldown - frameDelta);
        map3.damageCooldown = std::max(0.0f, map3.damageCooldown - frameDelta);

        map3.player.update(playerInput, map3.environment.collisionPreview(), map3.environment.worldMin(), map3.environment.worldMax(), frameDelta);

        if (actionPressed) {
            if (currentMode == PlayMode::Mode3D && map3.dodgeCooldown <= 0.0f) {
                if (tryDodgePlayer(map3.player, map3.environment, playerInput)) {
                    map3.dodgeActiveUntil = now + Map3DodgeActiveTime;
                    map3.dodgeCooldown = Map3DodgeCooldown;
                }
            } else if (currentMode == PlayMode::Mode2D) {
                map3.parryActiveUntil = now + Map3ParryActiveTime;
            }
        }

        const bool activeParryAfterInput = now <= map3.parryActiveUntil;
        const bool activeDodgeAfterInput = now <= map3.dodgeActiveUntil;
        if (map3.enemies.update(map3.player, map3.environment, frameDelta, now, activeParryAfterInput, activeDodgeAfterInput) && map3.damageCooldown <= 0.0f) {
            map3.health = std::max(0, map3.health - 1);
            map3.damageCooldown = Map3EnemyHitCooldown;
            if (map3.health <= 0) {
                map3.gameOver = true;
            }
        }

        map3.mission.update(map3.player, now);
    }

    updateGameplayCamera(map3.player, map3.environment, map3.mission, now, frameDelta);

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    const glm::mat4 view = glm::lookAt(gameplayCameraPosition, gameplayCameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 180.0f);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.42f, 0.58f, 0.78f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    uploadCommonSceneUniforms(sceneShader, map3.environment, gameplayCameraPosition, view, projection, now, nullptr, 1.0f, nullptr);
    lavaShader.use();
    lavaShader.setMat4("uView", view);
    lavaShader.setMat4("uProjection", projection);
    lavaShader.setFloat("uTime", now);

    map3.environment.render(sceneShader, lavaShader, now, gameplayCameraPosition);
    map3.mission.render(sceneShader, now, gameplayCameraPosition);
    map3.enemies.render(sceneShader, now, gameplayCameraPosition);
    renderMap3ActionEffect(sceneShader, map3.player, now, now <= map3.parryActiveUntil, now <= map3.dodgeActiveUntil);
    map3.player.render(sceneShader);
}

bool map3DefensiveActionActive(const Map3Runtime& map3, float timeSeconds) {
    return timeSeconds <= map3.parryActiveUntil || timeSeconds <= map3.dodgeActiveUntil;
}
