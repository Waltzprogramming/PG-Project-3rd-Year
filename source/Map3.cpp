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
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

namespace {
constexpr float Map3EnemySpeed3D = 1.18f;
constexpr float Map3EnemySpeed2D = 1.38f;
constexpr float Map3EnemyDetectionRange = 7.4f;
constexpr float Map3EnemyHitCooldown = 0.85f;
constexpr float Map3DodgeDistance = 1.75f;
constexpr float Map3DodgeCooldown = 0.72f;
constexpr float Map3DodgeActiveTime = 0.28f;
constexpr float Map3ParryActiveTime = 0.42f;
constexpr float Map3ParryRadius = 1.24f;
constexpr float Map3EnemyProjectileSpeed = 3.15f;
constexpr float Map3EnemyProjectileCooldown = 1.65f;
constexpr float Map3ProjectileLifetime = 4.25f;
constexpr float Map3ReflectedProjectileSpeed = 4.65f;
constexpr float Map3ProjectileHitRadius = 0.42f;
constexpr int Map3InitialEnemyCount = 5;
constexpr float Map3EnemyWaveInterval = 30.0f;
constexpr float Map3EnemyVisualSize = 0.56f;
constexpr float Map3EnemyCollisionHalf = 0.20f;
constexpr float Map3EnemyPreferredRange2D = 2.05f;
constexpr float Map3EnemyPreferredRange3D = 2.45f;
constexpr float Map3EnemyMinimumSpawnDistance = 3.2f;
constexpr float Map3EnemyProjectileCollisionRadius = 0.15f;
constexpr float Map3EnemyProjectileVisualRadius = 0.34f;
constexpr float Map3ReflectedProjectileVisualRadius = 0.36f;
constexpr float Map3PathCenterZOffset = 0.72f;
constexpr float Map3FinishX = 12.0f;

bool isMap3PirateEnvironment(const Environment& environment) {
    std::string source = environment.levelSource();
    std::replace(source.begin(), source.end(), '\\', '/');
    return source.find("game_pirate_adventure_map") != std::string::npos;
}

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

bool findFloorAt(const std::vector<Bounds>& colliders, float x, float z, float preferredY, float& floorY);
glm::vec3 findMap3PirateSpawn(const Environment& environment);

std::vector<Bounds> buildMap3PirateCollision(const Environment& environment) {
    const glm::vec3 worldMin = environment.worldMin();
    const glm::vec3 worldMax = environment.worldMax();
    const glm::vec3 worldCenter = (worldMin + worldMax) * 0.5f;
    const glm::vec3 span = glm::max(worldMax - worldMin, glm::vec3(1.0f));

    const float pathLength = span.x * 0.88f;
    const float shoulderHalfWidth = 0.28f;
    const float pathCenterX = worldCenter.x - span.x * 0.08f;
    const glm::vec3 rawSpawn = findMap3PirateSpawn(environment);
    const float pathCenterZ = rawSpawn.z + Map3PathCenterZOffset;
    const float floorTop = rawSpawn.y - 0.08f;
    const float floorCenterY = floorTop - 0.16f;

    std::vector<Bounds> colliders;
    colliders.push_back({{pathCenterX, floorCenterY, pathCenterZ}, {pathLength * 0.5f, 0.16f, shoulderHalfWidth}});

    const float wallThickness = 0.22f;
    const float wallHeight = std::max(12.0f, span.y + 4.0f);
    const float wallCenterY = floorTop + wallHeight * 0.5f;
    colliders.push_back({{pathCenterX, wallCenterY, pathCenterZ - shoulderHalfWidth - wallThickness}, {pathLength * 0.5f, wallHeight * 0.5f, wallThickness}});
    colliders.push_back({{pathCenterX, wallCenterY, pathCenterZ + shoulderHalfWidth + wallThickness}, {pathLength * 0.5f, wallHeight * 0.5f, wallThickness}});
    colliders.push_back({{pathCenterX - pathLength * 0.5f - wallThickness, wallCenterY, pathCenterZ}, {wallThickness, wallHeight * 0.5f, shoulderHalfWidth + wallThickness}});
    colliders.push_back({{pathCenterX + pathLength * 0.5f + wallThickness, wallCenterY, pathCenterZ}, {wallThickness, wallHeight * 0.5f, shoulderHalfWidth + wallThickness}});

    return colliders;
}

const std::vector<Bounds>& map3ActiveColliders(const Map3Runtime& map3) {
    return map3.collisionBounds.empty() ? map3.environment.collisionPreview() : map3.collisionBounds;
}

glm::vec3 findMap3PirateSpawn(const Environment& environment) {
    const auto& colliders = environment.collisionPreview();
    const glm::vec3 worldMin = environment.worldMin();
    const glm::vec3 worldMax = environment.worldMax();
    const glm::vec3 span = glm::max(worldMax - worldMin, glm::vec3(1.0f));
    const glm::vec2 anchor(worldMin.x + span.x * 0.16f, (worldMin.z + worldMax.z) * 0.5f);
    glm::vec3 best = environment.recommendedSpawnPoint();
    float bestScore = std::numeric_limits<float>::max();

    for (const Bounds& collider : colliders) {
        const float top = collider.center.y + collider.halfExtent.y;
        const float width = collider.halfExtent.x * 2.0f;
        const float depth = collider.halfExtent.z * 2.0f;
        const float area = width * depth;
        const bool floorLike = collider.halfExtent.y <= 0.34f && area >= 0.04f;
        const bool playableHeight = top >= worldMin.y + 0.15f && top <= worldMin.y + 3.8f;
        if (!floorLike || !playableHeight) {
            continue;
        }

        const float safeX = std::max(collider.halfExtent.x - 0.34f, 0.0f);
        const float safeZ = std::max(collider.halfExtent.z - 0.34f, 0.0f);
        glm::vec3 candidate{
            std::clamp(anchor.x, collider.center.x - safeX, collider.center.x + safeX),
            top + 0.08f,
            std::clamp(anchor.y, collider.center.z - safeZ, collider.center.z + safeZ)
        };

        const float depthScore = std::abs(candidate.z - anchor.y) * 2.8f;
        const float startScore = std::abs(candidate.x - anchor.x);
        const float roadScore = collider.halfExtent.z <= 0.72f ? -1.8f : 0.0f;
        const float score = startScore + depthScore + std::abs(candidate.y - (worldMin.y + 1.6f)) * 0.55f + roadScore;
        if (score < bestScore) {
            bestScore = score;
            best = candidate;
        }
    }

    return best;
}

glm::vec3 findMap3PirateSpawn(const Environment& environment, const std::vector<Bounds>& colliders) {
    const Bounds& path = colliders.empty()
        ? Bounds{environment.recommendedSpawnPoint(), glm::vec3(1.0f)}
        : colliders.front();
    glm::vec3 spawn{path.center.x - path.halfExtent.x * 0.42f, path.center.y + path.halfExtent.y + 0.08f, path.center.z};
    float floorY = spawn.y;
    if (findFloorAt(colliders, spawn.x, spawn.z, spawn.y, floorY)) {
        spawn.y = floorY;
    }
    return spawn;
}

bool findFloorAt(const std::vector<Bounds>& colliders, float x, float z, float preferredY, float& floorY) {
    bool found = false;
    float bestScore = std::numeric_limits<float>::max();
    for (const Bounds& collider : colliders) {
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

bool tryDodgePlayer(Player& player, const Environment& environment, const std::vector<Bounds>& colliders, const PlayerInput& input) {
    const glm::vec3 direction = map3MoveDirectionFromInput(input);
    const glm::vec3 current = player.position();
    glm::vec3 target = current + direction * Map3DodgeDistance;
    const glm::vec3 worldMin = environment.worldMin();
    const glm::vec3 worldMax = environment.worldMax();

    target.x = std::clamp(target.x, worldMin.x + 0.36f, worldMax.x - 0.36f);
    target.z = std::clamp(target.z, worldMin.z + 0.36f, worldMax.z - 0.36f);
    float floorY = target.y;
    if (!findFloorAt(colliders, target.x, target.z, current.y, floorY)) {
        return false;
    }
    target.y = floorY;

    Bounds candidate = player.bounds();
    candidate.center += target - current;
    if (positionOverlapsWorld(candidate, colliders)) {
        return false;
    }

    player.teleportTo(target);
    return true;
}

void prepareMap3Jump(Player& player, PlayerInput& input, const Environment& environment, const std::vector<Bounds>& colliders) {
    if (!input.jumpPressed || player.grounded()) {
        return;
    }

    float floorY = player.position().y;
    if (!findFloorAt(colliders, player.position().x, player.position().z, player.position().y, floorY)) {
        return;
    }

    const float distanceToFloor = player.position().y - floorY;
    if (distanceToFloor < -0.04f || distanceToFloor > 0.34f) {
        return;
    }

    PlayerInput settleInput = input;
    settleInput.jumpPressed = false;
    player.update(settleInput, colliders, environment.worldMin(), environment.worldMax(), 1.0f / 30.0f);
    if (!player.grounded()) {
        return;
    }

    player.update(input, colliders, environment.worldMin(), environment.worldMax(), 0.0f);
    input.jumpPressed = false;
}

void resetMap3View(const Player& player) {
    currentMode = PlayMode::Mode2D;
    lastToggleKey = false;
    lastJumpKey = false;
    lastShieldKey = false;
    cameraYawDegrees = 0.0f;
    cameraPitchDegrees = 18.0f;
    locked2DDepth = player.position().z;
    cameraInitialized = false;
}

void resetMap3ViewForEnvironment(const Environment& environment, const Player& player) {
    resetMap3View(player);
    if (isMap3PirateEnvironment(environment)) {
        cameraYawDegrees = -90.0f;
        cameraPitchDegrees = 14.0f;
    }
}

Mesh createMap3ActionMesh() {
    return Mesh::sphere(24, 12, 1.0f);
}

Mesh createMap3ProjectileMesh() {
    return Mesh::sphere(18, 9, 0.5f);
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

    const float pulse = 0.92f + std::sin(timeSeconds * 18.0f) * 0.04f;
    const glm::vec3 baseScale = parryActive
        ? glm::vec3(0.24f, 0.18f, 0.24f)
        : glm::vec3(0.34f, 0.24f, 0.34f);
    glm::mat4 model(1.0f);
    model = glm::translate(model, player.position() + glm::vec3(0.0f, 0.18f, 0.0f));
    model = glm::scale(model, baseScale * pulse);

    shader.use();
    shader.setFloat("uTime", timeSeconds);
    shader.setMat4("uModel", model);
    bindSceneMaterial(shader, material);
    glDepthMask(GL_FALSE);
    effectMesh.draw();
    glDepthMask(GL_TRUE);
}

void renderMap3Projectiles(const Shader& shader, const std::vector<Map3Projectile>& projectiles, float timeSeconds, const glm::vec3& cameraPosition) {
    static Mesh projectileMesh = createMap3ProjectileMesh();
    shader.use();
    shader.setFloat("uTime", timeSeconds);

    for (const Map3Projectile& projectile : projectiles) {
        if (glm::length(projectile.position - cameraPosition) > 30.0f) {
            continue;
        }

        auto drawLayer = [&](float radius, const glm::vec3& baseColor, const glm::vec3& emissive, float opacity) {
            Material material;
            material.baseColor = baseColor;
            material.emissive = emissive;
            material.roughness = 0.28f;
            material.fogAmount = 0.02f;
            material.opacity = opacity;

            glm::mat4 model(1.0f);
            model = glm::translate(model, projectile.position);
            model = glm::scale(model, glm::vec3(radius));
            shader.setMat4("uModel", model);
            bindSceneMaterial(shader, material);
            projectileMesh.draw();
        };

        glDepthMask(GL_FALSE);
        if (projectile.reflected) {
            drawLayer(Map3ReflectedProjectileVisualRadius * 1.65f, {0.30f, 0.92f, 1.0f}, {0.20f, 0.72f, 0.95f}, 0.32f);
            drawLayer(Map3ReflectedProjectileVisualRadius, {0.72f, 0.98f, 1.0f}, {0.22f, 0.92f, 1.15f}, 0.96f);
        } else {
            drawLayer(Map3EnemyProjectileVisualRadius * 1.65f, {1.0f, 0.44f, 0.12f}, {0.92f, 0.24f, 0.04f}, 0.34f);
            drawLayer(Map3EnemyProjectileVisualRadius, {1.0f, 0.88f, 0.18f}, {1.20f, 0.30f, 0.08f}, 0.98f);
        }
        glDepthMask(GL_TRUE);
    }
}

bool loadMap3Environment(Environment& environment) {
    const std::array<std::string, 9> candidates = {
        "assets/mundo3/game_pirate_adventure_map/scene_map3.gltf",
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

bool updateMap3Projectiles(Map3Runtime& map3, float deltaTime, bool parryActive, bool dodgeActive) {
    bool hitPlayer = false;
    const Bounds playerBounds = map3.player.bounds();
    const glm::vec3 playerCenter = playerBounds.center;
    const glm::vec3 worldMin = map3.environment.worldMin();
    const glm::vec3 worldMax = map3.environment.worldMax();

    for (size_t index = 0; index < map3.projectiles.size();) {
        Map3Projectile& projectile = map3.projectiles[index];
        projectile.position += projectile.velocity * deltaTime;
        projectile.lifetime -= deltaTime;

        bool remove = projectile.lifetime <= 0.0f ||
            projectile.position.x < worldMin.x - 2.0f ||
            projectile.position.x > worldMax.x + 2.0f ||
            projectile.position.y < worldMin.y - 2.0f ||
            projectile.position.y > worldMax.y + 5.0f ||
            projectile.position.z < worldMin.z - 2.0f ||
            projectile.position.z > worldMax.z + 2.0f;

        if (!remove && !projectile.reflected && parryActive && currentMode == PlayMode::Mode2D) {
            const float horizontalDistance = std::abs(projectile.position.x - playerCenter.x);
            const float verticalDistance = std::abs(projectile.position.y - playerCenter.y);
            const float depthDistance = std::abs(projectile.position.z - playerCenter.z);
            if (horizontalDistance <= Map3ParryRadius && verticalDistance <= 0.92f && depthDistance <= 0.74f) {
                const glm::vec3 reflectedDirection = map3.enemies.directionToClosestEnemy(projectile.position);
                projectile.velocity = reflectedDirection * Map3ReflectedProjectileSpeed;
                projectile.reflected = true;
                projectile.lifetime = Map3ProjectileLifetime;
            }
        }

        if (!remove && projectile.reflected) {
            remove = map3.enemies.damageEnemyAt(projectile.position, Map3ProjectileHitRadius, 0.52f, 1);
        }

        if (!remove && !projectile.reflected && !dodgeActive) {
            const Bounds projectileBounds{projectile.position, glm::vec3(Map3EnemyProjectileCollisionRadius)};
            if (map3BoundsIntersect(projectileBounds, playerBounds)) {
                hitPlayer = true;
                remove = true;
            }
        }

        if (!remove) {
            const Bounds projectileBounds{projectile.position, glm::vec3(0.12f)};
            for (const Bounds& collider : map3ActiveColliders(map3)) {
                const float top = collider.center.y + collider.halfExtent.y;
                const bool likelyFloor = collider.halfExtent.y <= 0.34f && std::abs(projectile.position.y - top) <= 0.16f;
                if (!likelyFloor && map3BoundsIntersect(projectileBounds, collider)) {
                    remove = true;
                    break;
                }
            }
        }

        if (remove) {
            map3.projectiles.erase(map3.projectiles.begin() + static_cast<std::ptrdiff_t>(index));
        } else {
            ++index;
        }
    }

    return hitPlayer;
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

void Map3EnemyManager::reset(const Environment& environment, const std::vector<Bounds>& colliders, const glm::vec3& playerSpawn) {
    m_enemies.clear();
    addEnemies(Map3InitialEnemyCount, environment, colliders, playerSpawn);
}

void Map3EnemyManager::addEnemies(int count, const Environment& environment, const std::vector<Bounds>& colliders, const glm::vec3& playerSpawn) {
    if (count <= 0) {
        return;
    }

    const glm::vec3 worldMin = environment.worldMin();
    const glm::vec3 worldMax = environment.worldMax();
    const glm::vec2 span(std::max(worldMax.x - worldMin.x, 1.0f), std::max(worldMax.z - worldMin.z, 1.0f));
    const size_t baseIndex = m_enemies.size();

    for (int index = 0; index < count; ++index) {
        const size_t absoluteIndex = baseIndex + static_cast<size_t>(index);
        const float lane = static_cast<float>((absoluteIndex % 5) - 2) * 0.18f;
        const float forwardDistance = 3.65f + static_cast<float>(absoluteIndex % 4) * 0.72f;
        const float anchorX = std::clamp(playerSpawn.x + forwardDistance, worldMin.x + 0.8f, worldMax.x - 0.8f);
        const float anchorZ = std::clamp(playerSpawn.z + lane * std::min(span.y, 3.2f), worldMin.z + 0.45f, worldMax.z - 0.45f);
        const glm::vec2 anchor(anchorX, anchorZ);
        Enemy enemy;
        enemy.position = findSpawnPosition(environment, colliders, playerSpawn, anchor);
        enemy.spawnPosition = enemy.position;
        enemy.phase = static_cast<float>(absoluteIndex) * 1.21f;
        enemy.shotCooldown = 0.35f + static_cast<float>(absoluteIndex % 7) * 0.22f;
        enemy.health = 2;
        enemy.alive = true;
        m_enemies.push_back(enemy);
    }
}

bool Map3EnemyManager::update(const Player& player, const Environment& environment, const std::vector<Bounds>& colliders, float deltaTime, float timeSeconds, bool dodgeActive, std::vector<Map3Projectile>& projectiles) {
    bool hitPlayer = false;
    const glm::vec3 playerPosition = player.position();

    for (Enemy& enemy : m_enemies) {
        if (!enemy.alive) {
            continue;
        }

        enemy.hurtTimer = std::max(0.0f, enemy.hurtTimer - deltaTime);
        enemy.shotCooldown = std::max(0.0f, enemy.shotCooldown - deltaTime);
        glm::vec3 toPlayer = playerPosition - enemy.position;
        if (currentMode == PlayMode::Mode2D) {
            toPlayer.z = 0.0f;
            enemy.position.z += (locked2DDepth - enemy.position.z) * std::min(1.0f, deltaTime * 5.0f);
        }

        const float distance = glm::length(toPlayer);
        if (distance <= Map3EnemyDetectionRange && distance > 0.08f) {
            const float speed = currentMode == PlayMode::Mode2D ? Map3EnemySpeed2D : Map3EnemySpeed3D;
            glm::vec3 direction = glm::normalize(toPlayer);
            direction.y = 0.0f;
            const float preferredRange = currentMode == PlayMode::Mode2D ? Map3EnemyPreferredRange2D : Map3EnemyPreferredRange3D;
            if (distance > preferredRange) {
                tryMoveEnemy(enemy, environment, colliders, direction * speed * deltaTime);
            } else if (distance < preferredRange * 0.90f) {
                tryMoveEnemy(enemy, environment, colliders, -direction * speed * 0.86f * deltaTime);
            }
            enemy.yaw = std::atan2(direction.x, direction.z);

            if (enemy.shotCooldown <= 0.0f && distance <= Map3EnemyDetectionRange * 0.92f && distance >= preferredRange * 0.62f) {
                glm::vec3 shotDirection = playerPosition + glm::vec3(0.0f, 0.45f, 0.0f) - (enemy.position + glm::vec3(0.0f, 0.38f, 0.0f));
                if (currentMode == PlayMode::Mode2D) {
                    shotDirection.z = 0.0f;
                }
                if (glm::length(shotDirection) > 0.05f) {
                    shotDirection = glm::normalize(shotDirection);
                    projectiles.push_back({
                        enemy.position + glm::vec3(0.0f, 0.38f, 0.0f) + shotDirection * 0.38f,
                        shotDirection * Map3EnemyProjectileSpeed,
                        Map3ProjectileLifetime,
                        false
                    });
                    enemy.shotCooldown = Map3EnemyProjectileCooldown + std::fmod(enemy.phase, 0.45f);
                }
            }
        } else {
            const float drift = std::sin(timeSeconds * 0.9f + enemy.phase) * 0.36f;
            const glm::vec3 target = enemy.spawnPosition + glm::vec3(drift, 0.0f, currentMode == PlayMode::Mode2D ? 0.0f : std::cos(timeSeconds * 0.7f + enemy.phase) * 0.28f);
            glm::vec3 direction = target - enemy.position;
            direction.y = 0.0f;
            if (glm::length(direction) > 0.05f) {
                direction = glm::normalize(direction);
                tryMoveEnemy(enemy, environment, colliders, direction * Map3EnemySpeed3D * 0.45f * deltaTime);
                enemy.yaw = std::atan2(direction.x, direction.z);
            }
        }

        if (!dodgeActive && map3BoundsIntersect(player.bounds(), enemyBounds(enemy))) {
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
                material.baseColor = glm::mix(material.baseColor, glm::vec3(1.0f, 0.56f, 0.24f), 0.22f);
                material.emissive += glm::vec3(0.16f, 0.06f, 0.025f);
                material.fogAmount = 0.05f;
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
            material.emissive += glm::vec3(0.16f, 0.06f, 0.025f);
            material.fogAmount = 0.05f;
            if (enemy.hurtTimer > 0.0f) {
                material.baseColor = {1.0f, 0.22f, 0.18f};
            }
            shader.setMat4("uModel", model);
            bindSceneMaterial(shader, material);
            m_fallbackMesh.draw();
        }
    }
}

bool Map3EnemyManager::damageEnemyAt(const glm::vec3& position, float horizontalRadius, float verticalRadius, int damage) {
    for (Enemy& enemy : m_enemies) {
        if (!enemy.alive) {
            continue;
        }

        const glm::vec3 center = enemyBounds(enemy).center;
        const float horizontalDistance = glm::length(glm::vec2(position.x - center.x, position.z - center.z));
        const float verticalDistance = std::abs(position.y - center.y);
        if (horizontalDistance > horizontalRadius || verticalDistance > verticalRadius) {
            continue;
        }

        enemy.health -= damage;
        enemy.hurtTimer = 0.28f;
        if (enemy.health <= 0) {
            enemy.alive = false;
        }
        return true;
    }
    return false;
}

glm::vec3 Map3EnemyManager::directionToClosestEnemy(const glm::vec3& position) const {
    glm::vec3 bestDirection(0.0f);
    float bestDistance = std::numeric_limits<float>::max();

    for (const Enemy& enemy : m_enemies) {
        if (!enemy.alive) {
            continue;
        }

        glm::vec3 direction = enemyBounds(enemy).center - position;
        if (currentMode == PlayMode::Mode2D) {
            direction.z = 0.0f;
        }
        const float distance = glm::length(direction);
        if (distance > 0.05f && distance < bestDistance) {
            bestDistance = distance;
            bestDirection = direction / distance;
        }
    }

    if (glm::length(bestDirection) <= 0.05f) {
        return glm::vec3(1.0f, 0.0f, 0.0f);
    }
    return bestDirection;
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
    m_modelScale = Map3EnemyVisualSize / maxExtent;

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
    m_modelScale = Map3EnemyVisualSize;
}

glm::vec3 Map3EnemyManager::findSpawnPosition(const Environment& environment, const std::vector<Bounds>& colliders, const glm::vec3& playerSpawn, const glm::vec2& anchor) const {
    glm::vec3 best{anchor.x, playerSpawn.y, anchor.y};
    float bestScore = std::numeric_limits<float>::max();

    for (const Bounds& collider : colliders) {
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
            top + 0.001f,
            std::clamp(anchor.y, collider.center.z - safeZ, collider.center.z + safeZ)
        };

        const float spawnDistance = glm::length(glm::vec2(candidate.x - playerSpawn.x, candidate.z - playerSpawn.z));
        if (spawnDistance < Map3EnemyMinimumSpawnDistance) {
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

bool Map3EnemyManager::findFloorAt(const std::vector<Bounds>& colliders, float x, float z, float preferredY, float& floorY) const {
    return ::findFloorAt(colliders, x, z, preferredY, floorY);
}

bool Map3EnemyManager::tryMoveEnemy(Enemy& enemy, const Environment& environment, const std::vector<Bounds>& colliders, const glm::vec3& step) const {
    glm::vec3 target = enemy.position + step;
    target.x = std::clamp(target.x, environment.worldMin().x + 0.28f, environment.worldMax().x - 0.28f);
    target.z = std::clamp(target.z, environment.worldMin().z + 0.28f, environment.worldMax().z - 0.28f);

    float floorY = enemy.position.y - 0.05f;
    if (!findFloorAt(colliders, target.x, target.z, enemy.position.y, floorY)) {
        return false;
    }
    target.y = floorY + 0.001f;

    const Bounds candidate{target + glm::vec3(0.0f, Map3EnemyCollisionHalf, 0.0f), glm::vec3(Map3EnemyCollisionHalf)};
    for (const Bounds& collider : colliders) {
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
    return {enemy.position + glm::vec3(0.0f, Map3EnemyCollisionHalf, 0.0f), glm::vec3(Map3EnemyCollisionHalf)};
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
            map3.collisionBounds = isMap3PirateEnvironment(map3.environment)
                ? buildMap3PirateCollision(map3.environment)
                : map3.environment.collisionPreview();
            const glm::vec3 spawnPoint = isMap3PirateEnvironment(map3.environment)
                ? findMap3PirateSpawn(map3.environment, map3.collisionBounds)
                : map3.environment.recommendedSpawnPoint();
            map3.player.spawnAt(spawnPoint);
            map3.mission.reset(map3.environment, spawnPoint);
            map3.enemies.reset(map3.environment, map3.collisionBounds, spawnPoint);
            map3.health = map3.maxHealth;
            map3.damageCooldown = 0.0f;
            map3.dodgeCooldown = 0.0f;
            map3.dodgeActiveUntil = 0.0f;
            map3.parryActiveUntil = 0.0f;
            map3.nextEnemyWaveAt = 0.0f;
            map3.nextEnemyWaveSize = Map3InitialEnemyCount + 1;
            map3.projectiles.clear();
            map3.gameOver = false;
            map3.skipFirstUpdateFrame = true;
            resetMap3ViewForEnvironment(map3.environment, map3.player);
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

    map3.collisionBounds = isMap3PirateEnvironment(map3.environment)
        ? buildMap3PirateCollision(map3.environment)
        : map3.environment.collisionPreview();
    const glm::vec3 spawnPoint = isMap3PirateEnvironment(map3.environment)
        ? findMap3PirateSpawn(map3.environment, map3.collisionBounds)
        : map3.environment.recommendedSpawnPoint();
    map3.player.spawnAt(spawnPoint);
    map3.mission.initialize();
    map3.mission.reset(map3.environment, spawnPoint);
    map3.enemies.initialize();
    map3.enemies.reset(map3.environment, map3.collisionBounds, spawnPoint);
    map3.health = map3.maxHealth;
    map3.damageCooldown = 0.0f;
    map3.dodgeCooldown = 0.0f;
    map3.dodgeActiveUntil = 0.0f;
    map3.parryActiveUntil = 0.0f;
    map3.nextEnemyWaveAt = 0.0f;
    map3.nextEnemyWaveSize = Map3InitialEnemyCount + 1;
    map3.projectiles.clear();
    map3.gameOver = false;
    map3.skipFirstUpdateFrame = true;
    resetMap3ViewForEnvironment(map3.environment, map3.player);

    std::cout << "Mundo 3 ready. Collision volumes: " << map3ActiveColliders(map3).size() << std::endl;
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
    map3.projectiles.clear();
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

        const std::vector<Bounds>& colliders = map3ActiveColliders(map3);
        std::vector<Bounds> playerColliders = colliders;
        appendDimensionRestrictionColliders(playerColliders, map3.environment, locked2DDepth);
        prepareMap3Jump(map3.player, playerInput, map3.environment, playerColliders);
        map3.player.update(playerInput, playerColliders, map3.environment.worldMin(), map3.environment.worldMax(), frameDelta);

        if (actionPressed) {
            if (currentMode == PlayMode::Mode3D && map3.dodgeCooldown <= 0.0f) {
                if (tryDodgePlayer(map3.player, map3.environment, colliders, playerInput)) {
                    map3.dodgeActiveUntil = now + Map3DodgeActiveTime;
                    map3.dodgeCooldown = Map3DodgeCooldown;
                }
            } else if (currentMode == PlayMode::Mode2D) {
                map3.parryActiveUntil = now + Map3ParryActiveTime;
            }
        }

        const bool activeDodgeAfterInput = now <= map3.dodgeActiveUntil;
        if (map3.nextEnemyWaveAt <= 0.0f) {
            map3.nextEnemyWaveAt = now + Map3EnemyWaveInterval;
        }
        if (now >= map3.nextEnemyWaveAt) {
            map3.enemies.addEnemies(map3.nextEnemyWaveSize, map3.environment, colliders, map3.player.position());
            map3.nextEnemyWaveAt = now + Map3EnemyWaveInterval;
        }

        const bool enemyTouchedPlayer = map3.enemies.update(map3.player, map3.environment, colliders, frameDelta, now, activeDodgeAfterInput, map3.projectiles);
        const bool projectileHitPlayer = updateMap3Projectiles(map3, frameDelta, now <= map3.parryActiveUntil, activeDodgeAfterInput);
        if ((enemyTouchedPlayer || projectileHitPlayer) && map3.damageCooldown <= 0.0f) {
            map3.health = std::max(0, map3.health - 1);
            map3.damageCooldown = Map3EnemyHitCooldown;
            if (map3.health <= 0) {
                map3.gameOver = true;
            }
        }

        if (map3.player.position().x >= Map3FinishX) {
            map3.mission.forceComplete(now);
        }
    }

    updateGameplayCamera(map3.player, map3.environment, map3.mission, now, frameDelta);

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    const glm::mat4 view = glm::lookAt(gameplayCameraPosition, gameplayCameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 180.0f);

    glEnable(GL_DEPTH_TEST);
    const bool pirateMap = isMap3PirateEnvironment(map3.environment);
    glClearColor(pirateMap ? 0.48f : 0.42f, pirateMap ? 0.76f : 0.58f, pirateMap ? 0.90f : 0.78f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    uploadCommonSceneUniforms(sceneShader, map3.environment, gameplayCameraPosition, view, projection, now, nullptr, 1.0f, nullptr);
    if (pirateMap) {
        sceneShader.use();
        sceneShader.setVec3("uAmbientColor", {0.62f, 0.66f, 0.58f});
        sceneShader.setVec3("uDirectionalLight.direction", {-0.35f, -0.78f, -0.28f});
        sceneShader.setVec3("uDirectionalLight.color", {0.74f, 0.78f, 0.66f});
        sceneShader.setVec3("uFogColor", {0.48f, 0.76f, 0.90f});
        sceneShader.setFloat("uSceneExposure", 1.08f);
    }
    lavaShader.use();
    lavaShader.setMat4("uView", view);
    lavaShader.setMat4("uProjection", projection);
    lavaShader.setFloat("uTime", now);

    map3.environment.render(sceneShader, lavaShader, now, gameplayCameraPosition);
    map3.enemies.render(sceneShader, now, gameplayCameraPosition);
    renderMap3Projectiles(sceneShader, map3.projectiles, now, gameplayCameraPosition);
    renderMap3ActionEffect(sceneShader, map3.player, now, now <= map3.parryActiveUntil, now <= map3.dodgeActiveUntil);
    map3.player.render(sceneShader);
}

bool map3DefensiveActionActive(const Map3Runtime& map3, float timeSeconds) {
    return timeSeconds <= map3.parryActiveUntil || timeSeconds <= map3.dodgeActiveUntil;
}
