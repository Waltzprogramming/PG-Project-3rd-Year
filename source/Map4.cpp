#ifdef MAP4_IMPLEMENTATION

#include "Map4.h"

#include "GameRuntime.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
// Ajustes base de combate y supervivencia exclusivos de Mapa 4.
constexpr float Map4ProjectileLifetime = 4.20f;
constexpr float Map4ProjectileSpeed = 5.40f;
constexpr float Map4ProjectileCooldown = 0.18f;
constexpr float Map4ProjectileLength = 0.72f;
constexpr float Map4EnemyProjectileSpeed = 2.05f;
constexpr float Map4EnemyProjectileCooldown = 2.10f;
constexpr float Map4EnemyDetectionRange = 4.6f;
constexpr float Map4EnemyProjectileRange = 5.2f;
constexpr int Map4EnemyMaximumHealth = 3;
constexpr float Map4LightEnergyMaximum = 20.0f;
constexpr float Map4LightRespawnTime = 10.0f;
constexpr size_t Map4SunPickupTotal = 5;

bool map4BoundsIntersect(const Bounds& a, const Bounds& b) {
    // Intersección simple AABB usada por pickups, jugador y enemigos.
    const glm::vec3 delta = glm::abs(a.center - b.center);
    const glm::vec3 total = a.halfExtent + b.halfExtent;
    return delta.x < total.x && delta.y < total.y && delta.z < total.z;
}

glm::vec3 findMarioMapa4Spawn(const Environment& environment) {
    // Busca una plataforma segura en la mitad izquierda para el spawn inicial.
    const auto& colliders = environment.collisionPreview();
    const glm::vec3 worldMin = environment.worldMin();
    const glm::vec3 worldMax = environment.worldMax();
    glm::vec3 best{worldMin.x + 2.5f, worldMin.y + 1.2f, (worldMin.z + worldMax.z) * 0.5f};
    float bestScore = std::numeric_limits<float>::max();

    for (const Bounds& collider : colliders) {
        const float top = collider.center.y + collider.halfExtent.y;
        const float area = (collider.halfExtent.x * 2.0f) * (collider.halfExtent.z * 2.0f);
        const bool floorLike = collider.halfExtent.y <= 0.35f && area >= 0.30f;
        if (!floorLike) {
            continue;
        }
        if (isMarioMapa4Environment(environment) && top < worldMin.y + 0.55f) {
            continue;
        }

        const bool leftSide = collider.center.x <= worldMin.x + (worldMax.x - worldMin.x) * 0.22f;
        const bool validHeight = top >= worldMin.y - 0.2f && top <= worldMin.y + 4.0f;
        if (!leftSide || !validHeight) {
            continue;
        }

        glm::vec3 candidate = collider.center;
        candidate.y = top + 0.05f;
        // Premia plataformas cercanas al punto de inicio previsto para evitar apariciones extrañas.
        const float score = std::abs(candidate.x - (worldMin.x + 2.4f)) + std::abs(candidate.y - (worldMin.y + 0.8f)) * 0.5f;
        if (score < bestScore) {
            bestScore = score;
            best = candidate;
        }
    }

    return best;
}

void configureMarioMapa4PipeTeleport(Mapa4Runtime& mapa4) {
    // Calcula las dos bocas de la tubería a partir del tamaño real del mapa cargado.
    const glm::vec3 worldMin = mapa4.environment.worldMin();
    const glm::vec3 worldMax = mapa4.environment.worldMax();
    const float pipeX = worldMin.x + (worldMax.x - worldMin.x) * 0.52f;
    const float centerZ = (worldMin.z + worldMax.z) * 0.5f;
    mapa4.pipeTopEntry = {pipeX, worldMin.y + 3.10f, centerZ};
    mapa4.pipeBottomEntry = {pipeX, worldMin.y + 0.30f, centerZ};
}

void updateMarioMapa4PipeTeleport(Mapa4Runtime& mapa4, float dt, bool teleportPressed) {
    // El teletransporte solo se activa al tocar la tubería y confirmar la acción.
    if (!isMarioMapa4Environment(mapa4.environment)) {
        return;
    }

    mapa4.pipeTeleportCooldown = std::max(0.0f, mapa4.pipeTeleportCooldown - dt);
    if (mapa4.pipeTeleportCooldown > 0.0f || !teleportPressed) {
        return;
    }

    const Bounds playerBounds = mapa4.player.bounds();
    const Bounds topTrigger{mapa4.pipeTopEntry + glm::vec3(0.0f, 0.38f, 0.0f), {1.10f, 1.15f, 0.95f}};
    const Bounds bottomTrigger{mapa4.pipeBottomEntry + glm::vec3(0.0f, 0.38f, 0.0f), {1.10f, 1.15f, 0.95f}};

    const auto intersects = [](const Bounds& a, const Bounds& b) {
        return std::abs(a.center.x - b.center.x) <= a.halfExtent.x + b.halfExtent.x &&
            std::abs(a.center.y - b.center.y) <= a.halfExtent.y + b.halfExtent.y &&
            std::abs(a.center.z - b.center.z) <= a.halfExtent.z + b.halfExtent.z;
    };

    if (intersects(playerBounds, topTrigger)) {
        // El cooldown evita rebotes inmediatos entre ambas bocas.
        mapa4.player.teleportTo(mapa4.pipeBottomEntry + glm::vec3(0.0f, 0.15f, 0.0f));
        mapa4.pipeTeleportCooldown = 0.85f;
    } else if (intersects(playerBounds, bottomTrigger)) {
        mapa4.player.teleportTo(mapa4.pipeTopEntry + glm::vec3(0.0f, 0.15f, 0.0f));
        mapa4.pipeTeleportCooldown = 0.85f;
    }
}

Mesh createMapa4ShieldRingMesh() {
    // Geometría ligera para dibujar el efecto visual del escudo sin cargar modelos externos.
    constexpr int segments = 48;
    constexpr float outerRadius = 1.0f;
    constexpr float innerRadius = 0.88f;
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve((segments + 1) * 2);
    indices.reserve(segments * 6);

    for (int index = 0; index <= segments; ++index) {
        const float t = static_cast<float>(index) / static_cast<float>(segments);
        const float angle = t * glm::two_pi<float>();
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        Vertex outer{};
        outer.position = {c * outerRadius, s * outerRadius, 0.0f};
        outer.normal = {0.0f, 0.0f, 1.0f};
        outer.uv = {t, 1.0f};
        outer.color = glm::vec4(1.0f);
        vertices.push_back(outer);

        Vertex inner{};
        inner.position = {c * innerRadius, s * innerRadius, 0.0f};
        inner.normal = {0.0f, 0.0f, 1.0f};
        inner.uv = {t, 0.0f};
        inner.color = glm::vec4(1.0f);
        vertices.push_back(inner);
    }

    for (int index = 0; index < segments; ++index) {
        const unsigned int root = static_cast<unsigned int>(index * 2);
        indices.push_back(root + 0);
        indices.push_back(root + 1);
        indices.push_back(root + 2);
        indices.push_back(root + 2);
        indices.push_back(root + 1);
        indices.push_back(root + 3);
    }

    Mesh mesh;
    mesh.upload(vertices, indices);
    return mesh;
}

glm::vec3 map4MouseAimDirection(GLFWwindow* window, const glm::mat4& view, const glm::mat4& projection, const Player& player) {
    // Convierte la posición del mouse en una dirección de disparo sobre el plano del jugador.
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    if (width <= 0 || height <= 0) {
        return {1.0f, 0.0f, 0.0f};
    }

    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    const glm::vec4 viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    const glm::vec3 nearPoint = glm::unProject(
        glm::vec3(static_cast<float>(mouseX), static_cast<float>(height) - static_cast<float>(mouseY), 0.0f),
        view,
        projection,
        viewport);
    const glm::vec3 farPoint = glm::unProject(
        glm::vec3(static_cast<float>(mouseX), static_cast<float>(height) - static_cast<float>(mouseY), 1.0f),
        view,
        projection,
        viewport);

    const glm::vec3 ray = farPoint - nearPoint;
    const float planeZ = player.position().z;
    // En 2D el raycast se aplana sobre la profundidad fija del personaje.
    const float amount = std::abs(ray.z) > 0.0001f ? (planeZ - nearPoint.z) / ray.z : 0.0f;
    const glm::vec3 cursorPosition = nearPoint + ray * amount;
    glm::vec3 direction = cursorPosition - (player.position() + glm::vec3(0.0f, 0.56f, 0.0f));
    direction.z = 0.0f;
    if (glm::length(direction) < 0.05f) {
        return {1.0f, 0.0f, 0.0f};
    }
    return glm::normalize(direction);
}

void updateMapa4Projectiles(Mapa4Runtime& mapa4, float dt) {
    // Resuelve movimiento, colisiones y daño de todos los disparos activos.
    for (size_t i = 0; i < mapa4.projectiles.size();) {
        Mapa4Projectile& projectile = mapa4.projectiles[i];
        projectile.position += projectile.velocity * dt;
        projectile.lifetime -= dt;

        const glm::vec3 worldMin = mapa4.environment.worldMin();
        const glm::vec3 worldMax = mapa4.environment.worldMax();
        const bool outsideWorld =
            projectile.position.x < worldMin.x - 2.0f ||
            projectile.position.x > worldMax.x + 2.0f ||
            projectile.position.y < worldMin.y - 2.0f ||
            projectile.position.y > worldMax.y + 8.0f ||
            projectile.position.z < worldMin.z - 2.0f ||
            projectile.position.z > worldMax.z + 2.0f;

        bool remove = projectile.lifetime <= 0.0f || outsideWorld;
        if (!remove) {
            if (projectile.fromEnemy) {
                // Los impactos enemigos alimentan el sistema de daño gradual del mapa.
                const glm::vec3 playerCenter = mapa4.player.bounds().center;
                const bool hitPlayer =
                    std::abs(projectile.position.x - playerCenter.x) <= 0.52f &&
                    std::abs(projectile.position.y - playerCenter.y) <= 0.88f &&
                    std::abs(projectile.position.z - playerCenter.z) <= 0.52f;
                if (hitPlayer) {
                    remove = true;
                    if (!mapa4.shieldActive) {
                        ++mapa4.pendingHits;
                        if (mapa4.pendingHits >= 2) {
                            mapa4.pendingHits = 0;
                            mapa4.health = std::max(0, mapa4.health - 1);
                            if (mapa4.health <= 0) {
                                mapa4.gameOver = true;
                            }
                        }
                    }
                }
            } else {
                // El disparo del jugador delega el impacto al gestor de enemigos.
                remove = mapa4.enemies.damageEnemyAt(projectile.position, 0.58f, 0.72f, projectile.damage);
            }
        }

        if (remove) {
            mapa4.projectiles.erase(mapa4.projectiles.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
}

void renderMapa4Projectiles(const Shader& shader, const std::vector<Mapa4Projectile>& projectiles, const glm::vec3& cameraPosition, float timeSeconds) {
    // Dibuja flechas del jugador y enemigos con materiales distintos pero una malla compartida.
    static Mesh arrowMesh = Mesh::cylinder(18, 1.0f, 0.08f);
    Material playerArrowMaterial;
    playerArrowMaterial.baseColor = {0.86f, 0.82f, 0.72f};
    playerArrowMaterial.emissive = {0.10f, 0.08f, 0.04f};
    playerArrowMaterial.roughness = 0.42f;
    playerArrowMaterial.fogAmount = 0.12f;
    Material enemyArrowMaterial;
    enemyArrowMaterial.baseColor = {0.78f, 0.22f, 0.18f};
    enemyArrowMaterial.emissive = {0.20f, 0.05f, 0.04f};
    enemyArrowMaterial.roughness = 0.48f;
    enemyArrowMaterial.fogAmount = 0.12f;

    for (const Mapa4Projectile& projectile : projectiles) {
        if (glm::length(projectile.position - cameraPosition) > 22.0f) {
            continue;
        }

        glm::vec3 direction = glm::length(projectile.velocity) > 0.0001f
            ? glm::normalize(projectile.velocity)
            : glm::vec3(1.0f, 0.0f, 0.0f);
        const float yaw = std::atan2(direction.x, direction.z);
        const float pitch = -std::atan2(direction.y, std::max(std::sqrt(direction.x * direction.x + direction.z * direction.z), 0.0001f));

        glm::mat4 model(1.0f);
        model = glm::translate(model, projectile.position);
        model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, pitch, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(projectile.fromEnemy ? 0.12f : 0.10f, Map4ProjectileLength, projectile.fromEnemy ? 0.12f : 0.10f));
        shader.use();
        shader.setMat4("uModel", model);
        shader.setFloat("uTime", timeSeconds);
        bindSceneMaterial(shader, projectile.fromEnemy ? enemyArrowMaterial : playerArrowMaterial);
        arrowMesh.draw();
    }
}

void renderMapa4Shield(const Shader& shader, const Player& player, float timeSeconds) {
    // El escudo se compone de anillos cruzados para que el personaje siga siendo visible.
    static Mesh shieldRing = createMapa4ShieldRingMesh();

    const float pulse = 0.96f + 0.04f * std::sin(timeSeconds * 9.0f);
    Material ringMaterial;
    ringMaterial.baseColor = {0.22f, 1.00f, 0.94f};
    ringMaterial.emissive = {0.02f, 0.08f, 0.08f};
    ringMaterial.roughness = 0.95f;
    ringMaterial.fogAmount = 0.02f;
    ringMaterial.opacity = 0.16f;

    const glm::vec3 center = player.position() + glm::vec3(0.0f, 0.58f, 0.0f);
    glm::mat4 frontRing(1.0f);
    frontRing = glm::translate(frontRing, center);
    frontRing = glm::scale(frontRing, glm::vec3(0.88f, 0.94f, 0.88f) * pulse);

    glm::mat4 sideRing(1.0f);
    sideRing = glm::translate(sideRing, center);
    sideRing = glm::rotate(sideRing, glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    sideRing = glm::scale(sideRing, glm::vec3(0.88f, 0.94f, 0.88f) * pulse);

    glm::mat4 horizontalRing(1.0f);
    horizontalRing = glm::translate(horizontalRing, center);
    horizontalRing = glm::rotate(horizontalRing, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
    horizontalRing = glm::scale(horizontalRing, glm::vec3(0.76f, 0.76f, 0.76f) * pulse);

    shader.use();
    shader.setFloat("uTime", timeSeconds);
    shader.setMat4("uModel", frontRing);
    bindSceneMaterial(shader, ringMaterial);
    shieldRing.draw();

    shader.setMat4("uModel", sideRing);
    bindSceneMaterial(shader, ringMaterial);
    shieldRing.draw();

    Material horizontalMaterial = ringMaterial;
    horizontalMaterial.opacity = 0.11f;
    shader.setMat4("uModel", horizontalRing);
    bindSceneMaterial(shader, horizontalMaterial);
    shieldRing.draw();
}

void renderMapa4Backdrop(const Shader& shader, const Environment& environment, const glm::vec3& cameraPosition, float timeSeconds) {
    // El skybox sigue a la cámara para que el fondo siempre envuelva el nivel.
    static bool skyboxLoaded = false;
    static LoadedModel skyboxModel;
    static std::vector<std::shared_ptr<Texture2D>> skyboxTextures;
    static std::vector<Material> skyboxMaterials;
    static std::string activeSkyboxPath;

    const std::string skyboxPath = resolveFirstExistingAsset({
        "assets/Mundos/skybox noche.glb",
        "assets/mundos/skybox noche.glb",
        "assets/Mundos/Skybox Noche.glb",
        "assets/Mundos/skyboxmapa4.glb",
        "assets/mundos/skyboxmapa4.glb",
        "assets/Mundos/SkyboxMapa4.glb"
    });

    if (!skyboxPath.empty() && (!skyboxLoaded || activeSkyboxPath != skyboxPath)) {
        skyboxLoaded = false;
        skyboxModel = LoadedModel{};
        skyboxTextures.clear();
        skyboxMaterials.clear();
        activeSkyboxPath = skyboxPath;

        const std::string resolvedSkyboxPath = resolveAssetPath(skyboxPath);
        skyboxModel = ModelLoader::loadModel(resolvedSkyboxPath);
        if (!skyboxModel.meshes.empty()) {
            skyboxMaterials.reserve(skyboxModel.materials.size());
            const std::filesystem::path skyboxModelPath = std::filesystem::path(resolvedSkyboxPath);
            for (const LoadedMaterial& loadedMaterial : skyboxModel.materials) {
                Material material;
                material.baseColor = loadedMaterial.diffuseColor;
                material.emissive = glm::vec3(0.10f, 0.10f, 0.14f);
                material.roughness = 1.0f;
                material.fogAmount = 0.0f;
                material.texture = loadTextureFromMaterial(loadedMaterial, skyboxModelPath, skyboxTextures);
                skyboxMaterials.push_back(material);
            }
            skyboxLoaded = true;
        }
    }

    if (!skyboxLoaded || skyboxModel.meshes.empty()) {
        return;
    }

    glDepthMask(GL_FALSE);
    // El fondo no debe escribir profundidad para no tapar la geometría jugable.
    shader.use();
    shader.setFloat("uTime", timeSeconds);
    const glm::vec3 worldMin = environment.worldMin();
    const glm::vec3 worldMax = environment.worldMax();
    const glm::vec3 worldCenter = (worldMin + worldMax) * 0.5f;
    const float span = std::max({worldMax.x - worldMin.x, worldMax.y - worldMin.y, worldMax.z - worldMin.z, 1.0f});

    glm::mat4 model(1.0f);
    model = glm::translate(model, cameraPosition + glm::vec3(0.0f, (worldCenter.y - cameraPosition.y) * 0.08f + 0.6f, 0.0f));
    model = glm::scale(model, glm::vec3(span * 0.24f));

    const size_t materialCount = skyboxMaterials.size();
    for (size_t meshIndex = 0; meshIndex < skyboxModel.meshes.size(); ++meshIndex) {
        shader.setMat4("uModel", model);
        if (materialCount > 0) {
            const size_t materialIndex = std::min<size_t>(skyboxModel.meshes[meshIndex].materialIndex, materialCount - 1);
            bindSceneMaterial(shader, skyboxMaterials[materialIndex]);
        }
        skyboxModel.meshes[meshIndex].mesh.draw();
    }
    glDepthMask(GL_TRUE);
}

} // namespace

bool SimpleEnemyManager::initialize() {
    // Carga ambos modelos de enemigo una sola vez y reutiliza los recursos.
    if (m_initialized) {
        return true;
    }

    loadEnemyModel(resolveAssetPath("assets/mapa 4/enemigo-01/source/enemigo-01.fbx"), {0.86f, 0.18f, 0.12f}, m_models[0]);
    loadEnemyModel(resolveAssetPath("assets/mapa 4/enemigo-02/source/enemigo-02.fbx"), {0.18f, 0.42f, 0.90f}, m_models[1]);
    m_initialized = true;
    return true;
}

void SimpleEnemyManager::reset(const Environment& environment, const glm::vec3& playerSpawn) {
    // Reparte enemigos en puntos válidos del mapa y les asigna una patrulla simple.
    m_enemies.clear();
    const glm::vec3 worldMin = environment.worldMin();
    const glm::vec3 worldMax = environment.worldMax();
    const glm::vec2 center((worldMin.x + worldMax.x) * 0.5f, (worldMin.z + worldMax.z) * 0.5f);
    const std::array<glm::vec2, 6> anchors = {
        center + glm::vec2(-7.0f, -5.4f),
        center + glm::vec2(7.0f, -4.8f),
        center + glm::vec2(-6.1f, 5.8f),
        center + glm::vec2(6.3f, 5.6f),
        center + glm::vec2(-1.8f, -6.2f),
        center + glm::vec2(2.4f, 6.4f)
    };

    for (size_t index = 0; index < anchors.size(); ++index) {
        Enemy enemy;
        enemy.modelIndex = static_cast<int>(index % m_models.size());
        enemy.position = findSpawnPosition(environment, playerSpawn, anchors[index]);
        enemy.patrolCenter = enemy.position;
        enemy.patrolRadius = 2.8f + static_cast<float>(index % 3) * 0.65f;
        // Variar radio y fase evita que todas las patrullas se vean idénticas.
        enemy.patrolSpeed = 0.56f + static_cast<float>(index) * 0.08f;
        enemy.patrolPhase = static_cast<float>(index) * 1.37f;
        enemy.patrolOnX = index % 2 == 0;
        enemy.attackCooldown = static_cast<float>(index) * 0.22f;
        enemy.detectionRange = Map4EnemyDetectionRange + static_cast<float>(index % 2) * 0.35f;
        enemy.attackRange = Map4EnemyProjectileRange + static_cast<float>(index % 3) * 0.25f;
        enemy.health = Map4EnemyMaximumHealth;
        enemy.alive = true;
        m_enemies.push_back(enemy);
    }
}

bool SimpleEnemyManager::update(const Player& player, const Environment& environment, float deltaTime, float timeSeconds, std::vector<Mapa4Projectile>& projectiles) {
    // Controla patrulla, detección cercana y disparos lentos de los enemigos.
    bool damagedPlayer = false;
    const Bounds playerBounds = player.bounds();
    const glm::vec3 playerPosition = player.position();
    const float dt = std::clamp(deltaTime, 0.0f, 1.0f / 30.0f);
    const bool patrolOnly = isMarioMapa4Environment(environment);

    for (Enemy& enemy : m_enemies) {
        if (!enemy.alive) {
            continue;
        }
        enemy.attackCooldown = std::max(0.0f, enemy.attackCooldown - dt);
        glm::vec3 toPlayer = playerPosition - enemy.position;
        toPlayer.y = 0.0f;
        const float distance = glm::length(toPlayer);

        if (distance > 24.0f) {
            // Si está demasiado lejos, se omite la IA completa por rendimiento.
            continue;
        }

        if (patrolOnly) {
            glm::vec3 patrolTarget = enemy.patrolCenter;
            const float oscillation = std::sin(timeSeconds * enemy.patrolSpeed + enemy.patrolPhase) * enemy.patrolRadius;
            if (enemy.patrolOnX) {
                patrolTarget.x += oscillation;
            } else {
                patrolTarget.z += oscillation;
            }

            glm::vec3 toPatrol = patrolTarget - enemy.position;
            toPatrol.y = 0.0f;
            const float patrolDistance = glm::length(toPatrol);
            if (patrolDistance > 0.08f) {
                const glm::vec3 direction = glm::normalize(toPatrol);
                enemy.yaw = std::atan2(direction.x, direction.z);
                const float patrolScale = distance > 16.0f ? 0.70f : 1.15f;
                const glm::vec3 step = direction * (patrolScale * dt);
                if (!tryMoveEnemy(enemy, environment, step)) {
                    tryMoveEnemy(enemy, environment, {step.x, 0.0f, 0.0f});
                    tryMoveEnemy(enemy, environment, {0.0f, 0.0f, step.z});
                }
            }

            if (distance <= enemy.detectionRange && distance > 0.001f) {
                enemy.yaw = std::atan2(toPlayer.x, toPlayer.z);
                if (distance <= enemy.attackRange && enemy.attackCooldown <= 0.0f) {
                    const glm::vec3 shotDirection = glm::normalize(glm::vec3(toPlayer.x, 0.0f, toPlayer.z));
                    projectiles.push_back({
                        enemy.position + glm::vec3(shotDirection.x * 0.45f, 0.62f, shotDirection.z * 0.45f),
                        shotDirection * Map4EnemyProjectileSpeed,
                        Map4ProjectileLifetime,
                        1,
                        true
                    });
                    enemy.attackCooldown = Map4EnemyProjectileCooldown;
                }
            }
        } else {
            if (distance > 0.001f) {
                enemy.yaw = std::atan2(toPlayer.x, toPlayer.z);
            }

            if (distance < 8.5f && distance > 0.85f) {
                const glm::vec3 direction = glm::normalize(toPlayer);
                const glm::vec3 step = direction * (1.35f * dt);
                if (!tryMoveEnemy(enemy, environment, step)) {
                    tryMoveEnemy(enemy, environment, {step.x, 0.0f, 0.0f});
                    tryMoveEnemy(enemy, environment, {0.0f, 0.0f, step.z});
                }
            }
        }

        if (map4BoundsIntersect(playerBounds, enemyBounds(enemy)) && enemy.attackCooldown <= 0.0f) {
            enemy.attackCooldown = 1.15f;
            damagedPlayer = true;
        }
    }
    return damagedPlayer;
}

void SimpleEnemyManager::render(const Shader& shader, float timeSeconds, const glm::vec3& cameraPosition) const {
    // Omite enemigos lejanos para mantener el coste de render controlado.
    shader.use();
    shader.setFloat("uTime", timeSeconds);
    for (const Enemy& enemy : m_enemies) {
        if (!enemy.alive || glm::length(enemy.position - cameraPosition) > 24.0f) {
            continue;
        }
        const EnemyModel& modelData = m_models[std::clamp(enemy.modelIndex, 0, static_cast<int>(m_models.size() - 1))];
        const glm::mat4 model = enemyModelMatrix(enemy, timeSeconds);
        for (const MissionRenderablePart& part : modelData.parts) {
            shader.setMat4("uModel", model * localPartMatrix(part));
            bindSceneMaterial(shader, part.material);
            part.mesh.draw();
        }
    }
}

bool SimpleEnemyManager::loadEnemyModel(const std::string& path, const glm::vec3& fallbackColor, EnemyModel& modelData) {
    // Intenta cargar el modelo real; si falla, deja listo un sustituto procedural.
    LoadedModel model = ModelLoader::loadModel(path);
    if (model.meshes.empty()) {
        buildFallbackModel(fallbackColor, modelData);
        return false;
    }

    modelData.parts.clear();
    modelData.modelMin = model.minBounds;
    modelData.modelMax = model.maxBounds;
    modelData.modelCenter = (modelData.modelMin + modelData.modelMax) * 0.5f;
    modelData.color = fallbackColor;
    const glm::vec3 size = modelData.modelMax - modelData.modelMin;
    const float height = std::max(size.y, 0.001f);
    modelData.modelScale = 0.72f / height;

    const bool enemy01 = path.find("enemigo-01") != std::string::npos;
    const std::vector<std::string> fallbackTextures = enemy01
        ? std::vector<std::string>{"assets/characters/enemigo-01/textures/enemigo-01_1001_AlbedoTransparency.png"}
        : std::vector<std::string>{"assets/characters/enemigo-02/textures/enemigo-02_1001_AlbedoTransparency.png"};

    for (LoadedMesh& mesh : model.meshes) {
        MissionRenderablePart part;
        if (mesh.materialIndex < model.materials.size()) {
            const LoadedMaterial& material = model.materials[mesh.materialIndex];
            part.material.baseColor = material.diffuseColor;
            part.material.opacity = material.opacity;
            part.material.texture = loadEnemyTexture(material.diffuseTexturePath);
        } else {
            part.material.baseColor = fallbackColor;
        }
        // Los FBX de los enemigos no siempre reportan rutas de textura a Assimp.
        if (!part.material.texture) {
            part.material.texture = loadFirstEnemyTexture(fallbackTextures);
        }
        part.material.roughness = 0.72f;
        part.material.fogAmount = 0.24f;
        part.mesh = std::move(mesh.mesh);
        modelData.parts.push_back(std::move(part));
    }
    return true;
}

std::shared_ptr<Texture2D> SimpleEnemyManager::loadFirstEnemyTexture(const std::vector<std::string>& paths) {
    for (const std::string& path : paths) {
        auto texture = loadEnemyTexture(path);
        if (texture && texture->valid()) {
            return texture;
        }
    }
    return nullptr;
}

std::shared_ptr<Texture2D> SimpleEnemyManager::loadEnemyTexture(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    const std::filesystem::path original(path);
    const std::filesystem::path fileName = original.filename();
    const std::filesystem::path candidates[] = {
        original,
        std::filesystem::path("assets") / "mapa 4" / "enemigo-01" / "textures" / fileName,
        std::filesystem::path("assets") / "mapa 4" / "enemigo-02" / "textures" / fileName,
        std::filesystem::path("assets") / "characters" / "enemigo-01" / "textures" / fileName,
        std::filesystem::path("assets") / "characters" / "enemigo-02" / "textures" / fileName,
        std::filesystem::path("..") / ".." / "assets" / "mapa 4" / "enemigo-01" / "textures" / fileName,
        std::filesystem::path("..") / ".." / "assets" / "mapa 4" / "enemigo-02" / "textures" / fileName,
        std::filesystem::path("..") / ".." / "assets" / "characters" / "enemigo-01" / "textures" / fileName,
        std::filesystem::path("..") / ".." / "assets" / "characters" / "enemigo-02" / "textures" / fileName
    };

    std::filesystem::path resolved = original;
    for (const auto& candidate : candidates) {
        if (!candidate.empty() && std::filesystem::exists(candidate)) {
            resolved = std::filesystem::weakly_canonical(candidate);
            break;
        }
    }

    const std::string normalized = resolved.string();
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

glm::vec3 SimpleEnemyManager::findSpawnPosition(const Environment& environment, const glm::vec3& playerSpawn, const glm::vec2& anchor) const {
    // Busca plataformas amplias y seguras para distribuir enemigos lejos del spawn.
    glm::vec3 best = playerSpawn + glm::vec3(anchor.x >= 0.0f ? 4.0f : -4.0f, 0.0f, anchor.y >= 0.0f ? 4.0f : -4.0f);
    float bestScore = std::numeric_limits<float>::max();

    for (const Bounds& collider : environment.collisionPreview()) {
        const float top = collider.center.y + collider.halfExtent.y;
        const float area = (collider.halfExtent.x * 2.0f) * (collider.halfExtent.z * 2.0f);
        const bool floorLike = collider.halfExtent.y <= 0.30f && area > 0.35f;
        if (!floorLike) {
            continue;
        }
        if (isMarioMapa4Environment(environment) && top < environment.worldMin().y + 0.55f) {
            continue;
        }

        const glm::vec3 candidate{collider.center.x, top + 0.04f, collider.center.z};
        const float playerDistance = glm::length(glm::vec2(candidate.x - playerSpawn.x, candidate.z - playerSpawn.z));
        if (playerDistance < 3.0f) {
            // Mantiene un espacio seguro entre el jugador y el spawn enemigo.
            continue;
        }

        const float score = glm::length(glm::vec2(candidate.x, candidate.z) - anchor);
        if (score < bestScore) {
            bestScore = score;
            best = candidate;
        }
    }
    return best;
}

bool SimpleEnemyManager::tryMoveEnemy(Enemy& enemy, const Environment& environment, const glm::vec3& step) const {
    // Valida movimiento contra colisiones y corrige la altura al piso más cercano.
    glm::vec3 candidate = enemy.position + step;
    const glm::vec3 worldMin = environment.worldMin();
    const glm::vec3 worldMax = environment.worldMax();
    if (worldMax.x <= worldMin.x || worldMax.z <= worldMin.z) {
        return false;
    }
    candidate.x = std::clamp(candidate.x, worldMin.x + 0.45f, worldMax.x - 0.45f);
    candidate.z = std::clamp(candidate.z, worldMin.z + 0.45f, worldMax.z - 0.45f);

    float floorY = candidate.y;
    if (!findFloorAt(environment, candidate.x, candidate.z, enemy.position.y, floorY)) {
        // Sin piso válido, el enemigo no sale de su zona jugable.
        return false;
    }
    candidate.y = floorY + 0.04f;

    const Bounds candidateBounds{candidate + glm::vec3(0.0f, 0.55f, 0.0f), {0.32f, 0.55f, 0.32f}};
    for (const Bounds& collider : environment.collisionPreview()) {
        const float top = collider.center.y + collider.halfExtent.y;
        const bool floorUnderEnemy = top <= candidate.y + 0.06f;
        if (!floorUnderEnemy && map4BoundsIntersect(candidateBounds, collider)) {
            return false;
        }
    }

    enemy.position = candidate;
    return true;
}

bool SimpleEnemyManager::findFloorAt(const Environment& environment, float x, float z, float preferredY, float& floorY) const {
    bool found = false;
    float bestScore = std::numeric_limits<float>::max();
    for (const Bounds& collider : environment.collisionPreview()) {
        const float top = collider.center.y + collider.halfExtent.y;
        const float area = (collider.halfExtent.x * 2.0f) * (collider.halfExtent.z * 2.0f);
        const bool floorLike = collider.halfExtent.y <= 0.30f && area > 0.20f;
        const bool inside =
            x >= collider.center.x - collider.halfExtent.x - 0.18f &&
            x <= collider.center.x + collider.halfExtent.x + 0.18f &&
            z >= collider.center.z - collider.halfExtent.z - 0.18f &&
            z <= collider.center.z + collider.halfExtent.z + 0.18f;
        if (!floorLike || !inside) {
            continue;
        }

        const float score = std::abs(top - preferredY);
        if (score < bestScore && score < 1.25f) {
            bestScore = score;
            floorY = top;
            found = true;
        }
    }
    return found;
}

Bounds SimpleEnemyManager::enemyBounds(const Enemy& enemy) const {
    return {enemy.position + glm::vec3(0.0f, 0.55f, 0.0f), {0.34f, 0.58f, 0.34f}};
}

glm::mat4 SimpleEnemyManager::enemyModelMatrix(const Enemy& enemy, float timeSeconds) const {
    const EnemyModel& modelData = m_models[std::clamp(enemy.modelIndex, 0, static_cast<int>(m_models.size() - 1))];
    const float bob = std::sin(timeSeconds * 4.2f + static_cast<float>(enemy.modelIndex)) * 0.025f;
    glm::mat4 model(1.0f);
    model = glm::translate(model, enemy.position + glm::vec3(0.0f, bob, 0.0f));
    model = glm::rotate(model, enemy.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(modelData.modelScale));
    model = glm::translate(model, {-modelData.modelCenter.x, -modelData.modelMin.y, -modelData.modelCenter.z});
    return model;
}

void SimpleEnemyManager::buildFallbackModel(const glm::vec3& color, EnemyModel& modelData) {
    modelData.parts.clear();
    MissionRenderablePart body;
    body.mesh = Mesh::cube();
    body.material.baseColor = color;
    body.material.roughness = 0.82f;
    body.localScale = {0.62f, 1.05f, 0.62f};
    modelData.parts.push_back(std::move(body));
    modelData.modelMin = {-0.5f, -0.5f, -0.5f};
    modelData.modelMax = {0.5f, 0.5f, 0.5f};
    modelData.modelCenter = {0.0f, 0.0f, 0.0f};
    modelData.modelScale = 1.0f;
    modelData.color = color;
}

bool SimpleEnemyManager::damageEnemyAt(const glm::vec3& position, float horizontalRadius, float verticalRadius, int damage) {
    for (Enemy& enemy : m_enemies) {
        if (!enemy.alive) {
            continue;
        }

        const glm::vec3 enemyCenter = enemy.position + glm::vec3(0.0f, 0.55f, 0.0f);
        if (std::abs(enemyCenter.x - position.x) <= horizontalRadius &&
            std::abs(enemyCenter.y - position.y) <= verticalRadius &&
            std::abs(enemyCenter.z - position.z) <= horizontalRadius) {
            enemy.health = std::max(0, enemy.health - damage);
            if (enemy.health <= 0) {
                enemy.alive = false;
            }
            return true;
        }
    }
    return false;
}

int SimpleEnemyManager::aliveCount() const {
    return static_cast<int>(std::count_if(m_enemies.begin(), m_enemies.end(), [](const Enemy& enemy) {
        return enemy.alive;
    }));
}

bool Map4LightManager::initialize() {
    // Prepara el modelo de Sol y su material emisivo para el ciclo de luz.
    if (m_initialized) {
        return true;
    }

    m_fallbackMesh = Mesh::cube();
    m_fallbackMaterial.baseColor = {1.0f, 0.86f, 0.18f};
    m_fallbackMaterial.emissive = {1.20f, 0.84f, 0.18f};
    m_fallbackMaterial.roughness = 0.24f;
    m_fallbackMaterial.fogAmount = 0.04f;
    loadSunModel();
    m_initialized = true;
    return true;
}

void Map4LightManager::reset(const Environment& environment, const glm::vec3& playerSpawn) {
    // Distribuye los Soles en puntos alcanzables para sostener la exploración nocturna.
    m_pickups.clear();
    struct SurfaceCandidate {
        Bounds bounds;
        float top{0.0f};
        float area{0.0f};
    };

    std::vector<SurfaceCandidate> surfaces;
    for (const Bounds& collider : environment.collisionPreview()) {
        const float top = collider.center.y + collider.halfExtent.y;
        const float area = (collider.halfExtent.x * 2.0f) * (collider.halfExtent.z * 2.0f);
        const bool floorLike = collider.halfExtent.y <= 0.32f && area >= 0.85f && collider.halfExtent.x >= 0.38f && collider.halfExtent.z >= 0.38f;
        const bool reachableHeight = top >= playerSpawn.y - 0.25f && top <= playerSpawn.y + 1.25f;
        if (floorLike && reachableHeight) {
            surfaces.push_back({collider, top, area});
        }
    }

    if (surfaces.empty()) {
        return;
    }

    std::sort(surfaces.begin(), surfaces.end(), [](const SurfaceCandidate& a, const SurfaceCandidate& b) {
        if (a.bounds.center.x == b.bounds.center.x) {
            return a.area > b.area;
        }
        return a.bounds.center.x < b.bounds.center.x;
    });

    const std::array<glm::vec2, 8> offsets = {
        glm::vec2(0.0f, 0.0f), glm::vec2(-0.35f, 0.28f), glm::vec2(0.35f, -0.28f), glm::vec2(0.0f, 0.42f),
        glm::vec2(-0.42f, 0.0f), glm::vec2(0.42f, 0.0f), glm::vec2(-0.22f, -0.36f), glm::vec2(0.22f, 0.36f)
    };

    auto tryAddPickup = [&](const SurfaceCandidate& surface, const glm::vec2& bias) {
        if (m_pickups.size() >= Map4SunPickupTotal) {
            return;
        }

        const float safeX = std::max(surface.bounds.halfExtent.x - 0.48f, 0.0f);
        const float safeZ = std::max(surface.bounds.halfExtent.z - 0.48f, 0.0f);
        SunPickup pickup;
        pickup.position = {
            surface.bounds.center.x + safeX * bias.x,
            surface.top + 0.64f,
            surface.bounds.center.z + safeZ * bias.y
        };
        pickup.phase = static_cast<float>(m_pickups.size()) * 0.93f;

        if (glm::length(glm::vec2(pickup.position.x - playerSpawn.x, pickup.position.z - playerSpawn.z)) < 4.4f) {
            return;
        }

        const Bounds candidate = sunBounds(pickup);
        for (const Bounds& collider : environment.collisionPreview()) {
            if (map4BoundsIntersect(candidate, collider)) {
                return;
            }
        }

        for (const SunPickup& existing : m_pickups) {
            if (glm::length(glm::vec2(existing.position.x - pickup.position.x, existing.position.z - pickup.position.z)) < 4.0f) {
                return;
            }
        }

        m_pickups.push_back(pickup);
    };

    for (size_t i = 0; i < surfaces.size() && m_pickups.size() < Map4SunPickupTotal; ++i) {
        const size_t desiredCount = std::max<size_t>(1, Map4SunPickupTotal > 0 ? Map4SunPickupTotal - 1 : 1);
        const size_t index = surfaces.size() > 1
            ? std::min(surfaces.size() - 1, i * (surfaces.size() - 1) / desiredCount)
            : 0;
        for (const glm::vec2& offset : offsets) {
            tryAddPickup(surfaces[index], offset);
            if (m_pickups.size() >= Map4SunPickupTotal) {
                break;
            }
        }
    }

    for (const SurfaceCandidate& surface : surfaces) {
        if (m_pickups.size() >= Map4SunPickupTotal) {
            break;
        }
        for (const glm::vec2& offset : offsets) {
            tryAddPickup(surface, offset);
            if (m_pickups.size() >= Map4SunPickupTotal) {
                break;
            }
        }
    }
}

void Map4LightManager::update(Player& player, float timeSeconds, float deltaTime, float& energySeconds) {
    // Consume energía con el tiempo y recarga la barra cuando el jugador recoge un Sol.
    energySeconds = std::max(0.0f, energySeconds - deltaTime);
    const Bounds playerBounds = player.bounds();
    for (SunPickup& pickup : m_pickups) {
        if (!pickup.available) {
            if (timeSeconds >= pickup.respawnAt) {
                pickup.available = true;
            }
            continue;
        }

        if (map4BoundsIntersect(playerBounds, sunBounds(pickup))) {
            pickup.available = false;
            pickup.respawnAt = timeSeconds + Map4LightRespawnTime;
            // Cada Sol rellena una carga completa, pero nunca supera el máximo.
            energySeconds = std::min(Map4LightEnergyMaximum, energySeconds + Map4LightEnergyMaximum);
        }
    }
}

void Map4LightManager::render(const Shader& shader, float timeSeconds, const glm::vec3& cameraPosition) const {
    shader.use();
    shader.setFloat("uTime", timeSeconds);
    for (const SunPickup& pickup : m_pickups) {
        if (!pickup.available || glm::length(pickup.position - cameraPosition) > 24.0f) {
            continue;
        }

        const glm::mat4 model = sunModelMatrix(pickup, timeSeconds);
        if (!m_parts.empty()) {
            for (const MissionRenderablePart& part : m_parts) {
                shader.setMat4("uModel", model * localPartMatrix(part));
                bindSceneMaterial(shader, part.material);
                part.mesh.draw();
            }
        } else {
            shader.setMat4("uModel", model);
            bindSceneMaterial(shader, m_fallbackMaterial);
            m_fallbackMesh.draw();
        }
    }
}

std::vector<glm::vec3> Map4LightManager::activeLightPositions(const glm::vec3& cameraPosition, size_t maxCount) const {
    struct Candidate {
        glm::vec3 position{0.0f};
        float distance{0.0f};
    };

    std::vector<Candidate> candidates;
    candidates.reserve(m_pickups.size());
    for (const SunPickup& pickup : m_pickups) {
        if (!pickup.available) {
            continue;
        }
        const float distance = glm::length(pickup.position - cameraPosition);
        if (distance > 14.0f) {
            continue;
        }
        candidates.push_back({pickup.position, distance});
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.distance < b.distance;
    });

    std::vector<glm::vec3> result;
    result.reserve(std::min(maxCount, candidates.size()));
    for (size_t i = 0; i < candidates.size() && i < maxCount; ++i) {
        result.push_back(candidates[i].position);
    }
    return result;
}

std::shared_ptr<Texture2D> Map4LightManager::loadSunTexture(const std::string& path) {
    if (path.empty()) {
        return {};
    }

    std::filesystem::path original(path);
    const std::filesystem::path fileName = original.filename();
    const std::filesystem::path candidates[] = {
        original,
        std::filesystem::path("assets") / "Items" / fileName,
        std::filesystem::path("assets") / "items" / fileName,
        std::filesystem::path("..") / ".." / original,
        std::filesystem::path("..") / ".." / "assets" / "Items" / fileName,
        std::filesystem::path("..") / ".." / "assets" / "items" / fileName
    };

    std::filesystem::path resolved = original;
    for (const auto& candidate : candidates) {
        if (!candidate.empty() && std::filesystem::exists(candidate)) {
            resolved = std::filesystem::weakly_canonical(candidate);
            break;
        }
    }

    const std::string normalized = resolved.string();
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

std::shared_ptr<Texture2D> Map4LightManager::loadSunTexture(const LoadedMaterial& material) {
    auto texture = loadSunTexture(material.diffuseTexturePath);
    if (texture && texture->valid()) {
        return texture;
    }

    if (material.embeddedTextureData.empty()) {
        return nullptr;
    }

    auto embedded = std::make_shared<Texture2D>();
    if (material.embeddedTextureCompressed) {
        if (!embedded->loadFromMemory(material.embeddedTextureData.data(), static_cast<int>(material.embeddedTextureData.size()), false)) {
            return nullptr;
        }
    } else if (material.embeddedTextureWidth > 0 && material.embeddedTextureHeight > 0) {
        embedded->createFromRGBA(material.embeddedTextureWidth, material.embeddedTextureHeight, material.embeddedTextureData.data(), false);
    } else {
        return nullptr;
    }

    m_textures.push_back(embedded);
    return embedded;
}

bool Map4LightManager::loadSunModel() {
    // Carga el coleccionable Sol y ajusta sus materiales para que brillen en la oscuridad.
    LoadedModel model = ModelLoader::loadModel(resolveAssetPath("assets/Items/sol.glb"));
    if (model.meshes.empty()) {
        model = ModelLoader::loadModel(resolveAssetPath("assets/items/sol.glb"));
    }
    if (model.meshes.empty()) {
        std::cerr << "Mapa 4 sun model could not be loaded. Using procedural fallback sun." << std::endl;
        return false;
    }

    m_modelMin = model.minBounds;
    m_modelMax = model.maxBounds;
    m_modelCenter = (m_modelMin + m_modelMax) * 0.5f;
    const glm::vec3 size = m_modelMax - m_modelMin;
    const float maxExtent = std::max({size.x, size.y, size.z, 0.001f});
    m_modelScale = 0.42f / maxExtent;

    m_parts.clear();
    m_parts.reserve(model.meshes.size());
    for (LoadedMesh& mesh : model.meshes) {
        MissionRenderablePart part;
        if (mesh.materialIndex < model.materials.size()) {
            const LoadedMaterial& material = model.materials[mesh.materialIndex];
            part.material.baseColor = material.diffuseColor;
            part.material.opacity = material.opacity;
            part.material.texture = loadSunTexture(material);
        }
        part.material.emissive = {2.10f, 1.55f, 0.34f};
        part.material.roughness = 0.20f;
        part.material.fogAmount = 0.0f;
        if (!part.material.texture) {
            part.material.baseColor = {1.0f, 0.90f, 0.22f};
        }
        part.mesh = std::move(mesh.mesh);
        m_parts.push_back(std::move(part));
    }
    return true;
}

Bounds Map4LightManager::sunBounds(const SunPickup& pickup) const {
    return {pickup.position, {0.42f, 0.48f, 0.42f}};
}

glm::mat4 Map4LightManager::sunModelMatrix(const SunPickup& pickup, float timeSeconds) const {
    glm::mat4 model(1.0f);
    const float bob = std::sin(timeSeconds * 1.9f + pickup.phase) * 0.10f;
    model = glm::translate(model, pickup.position + glm::vec3(0.0f, bob, 0.0f));
    model = glm::rotate(model, timeSeconds * 1.7f + pickup.phase, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(m_modelScale));
    model = glm::translate(model, -m_modelCenter);
    return model;
}

bool isMarioMapa4Environment(const Environment& environment) {
    // Detecta si el runtime actual debe usar las reglas especiales de Mapa 4.
    const std::string source = environment.levelSource();
    return source.find("mapamian") != std::string::npos ||
        source.find("mapa verde") != std::string::npos ||
        source.find("mapa 4") != std::string::npos;
}

bool iniciarMapa4(Mapa4Runtime& mapa4) {
    // Inicializa o reactiva la sesión de Mapa 4 sin recargar recursos innecesariamente.
    if (mapa4.initialized) {
        if (!mapa4.sessionActive) {
            const glm::vec3 spawnPoint = isMarioMapa4Environment(mapa4.environment)
                ? findMarioMapa4Spawn(mapa4.environment)
                : mapa4.environment.recommendedSpawnPoint();
            mapa4.player.spawnAt(spawnPoint);
            mapa4.mission.reset(mapa4.environment, spawnPoint);
            mapa4.enemies.reset(mapa4.environment, spawnPoint);
            mapa4.lightPickups.initialize();
            mapa4.lightPickups.reset(mapa4.environment, spawnPoint);
            configureMarioMapa4PipeTeleport(mapa4);
            mapa4.lastCollectedCount = 0;
            mapa4.coinCollectDelay = 1.10f;
            mapa4.pipeTeleportCooldown = 0.0f;
            mapa4.health = mapa4.maxHealth;
            mapa4.damageCooldown = 0.0f;
            mapa4.pendingHits = 0;
            mapa4.shieldActive = false;
            mapa4.shieldTimer = 0.0f;
            mapa4.lightEnergy = Map4LightEnergyMaximum;
            mapa4.gameOver = false;
            mapa4.startSequencePending = true;
            mapa4.skipFirstUpdateFrame = true;
            mapa4.projectiles.clear();
            mapa4.projectileCooldown = 0.0f;
            currentMode = PlayMode::Mode2D;
            locked2DDepth = spawnPoint.z;
            cameraInitialized = false;
            mapa4.sessionActive = true;
        }
        return true;
    }

    const std::string levelPath = resolveFirstExistingAsset({
        "assets/mapa 4/mapamian/World 1/World 1/CourseSelectW1.dae",
        "assets/mapa 4/mapamian/World 1/World 1/CourseSelectW1A.dae",
        "assets/mapa 4/mapa verde/World 1/World 1/CourseSelectW1.dae"
    });
    if (levelPath.empty()) {
        return false;
    }

    mapa4.environment.create(levelPath, true);
    if (!environmentUsable(mapa4.environment)) {
        return false;
    }

    mapa4.coinSoundOpen = mapa4.coinSound.open(resolveFirstExistingAsset({
        "assets/mapa 4/coin sound.mp3",
        "assets/mapa 4/Coin Sound.mp3",
        "assets/audio/coin sound.mp3"
    }));
    if (mapa4.coinSoundOpen) {
        mapa4.coinSound.setVolume(860);
    }

    mapa4.player.load(resolveAssetPath("assets/characters/deadpool.glb"));
    const glm::vec3 spawnPoint = isMarioMapa4Environment(mapa4.environment)
        ? findMarioMapa4Spawn(mapa4.environment)
        : mapa4.environment.recommendedSpawnPoint();
    mapa4.player.spawnAt(spawnPoint);
    mapa4.mission.initialize();
    mapa4.mission.setCompleteOnAllCoins(true);
    mapa4.mission.reset(mapa4.environment, spawnPoint);
    mapa4.lightPickups.initialize();
    mapa4.lightPickups.reset(mapa4.environment, spawnPoint);
    mapa4.enemies.initialize();
    mapa4.enemies.reset(mapa4.environment, spawnPoint);
    configureMarioMapa4PipeTeleport(mapa4);
    mapa4.lastCollectedCount = 0;
    mapa4.coinCollectDelay = 1.10f;
    mapa4.pipeTeleportCooldown = 0.0f;
    mapa4.health = mapa4.maxHealth;
    mapa4.damageCooldown = 0.0f;
    mapa4.pendingHits = 0;
    mapa4.shieldActive = false;
    mapa4.shieldTimer = 0.0f;
    mapa4.lightEnergy = Map4LightEnergyMaximum;
    mapa4.gameOver = false;
    mapa4.startSequencePending = true;
    mapa4.skipFirstUpdateFrame = true;
    mapa4.projectiles.clear();
    mapa4.projectileCooldown = 0.0f;
    currentMode = PlayMode::Mode2D;
    locked2DDepth = spawnPoint.z;
    cameraInitialized = false;

    std::cout << "Mapa 4 ready. Collision volumes: " << mapa4.environment.collisionPreview().size() << std::endl;

    mapa4.initialized = true;
    mapa4.sessionActive = true;
    return true;
}

void volverAlMenu(Mapa4Runtime& mapa4) {
    // Limpia el estado temporal del nivel al regresar al menú.
    if (mapa4.musicOpen && mapa4.musicPlaying) {
        mapa4.music.stop();
        mapa4.musicPlaying = false;
    }
    mapa4.lastCollectedCount = 0;
    mapa4.coinCollectDelay = 0.0f;
    mapa4.pipeTeleportCooldown = 0.0f;
    mapa4.projectiles.clear();
    mapa4.projectileCooldown = 0.0f;
    mapa4.pendingHits = 0;
    mapa4.shieldActive = false;
    mapa4.shieldTimer = 0.0f;
    mapa4.lightEnergy = Map4LightEnergyMaximum;
    mapa4.startSequencePending = false;
    mapa4.skipFirstUpdateFrame = false;
    mapa4.sessionActive = false;
}

void renderMapa4(GLFWwindow* window, Mapa4Runtime& mapa4, const Shader& sceneShader, const Shader& lavaShader, float now) {
    // Este ciclo concentra la jugabilidad y el render específicos de Mapa 4.
    if (mapa4.startSequencePending) {
        // La música arranca solo cuando el nivel ya terminó de prepararse.
        if (!mapa4.musicOpen) {
            const std::vector<std::string> musicCandidates = {
                "assets/audio/map4.mp3",
                "assets/audio/Map4.mp3",
                "assets/mapa 4/audio 4.mp3",
                "assets/mapa 4/Audio 4.mp3",
                "assets/mapa 4/audio4.mp3",
                "assets/mapa 4/Audio4.mp3"
            };
            for (const std::string& candidate : musicCandidates) {
                if (std::filesystem::exists(resolveAssetPath(candidate))) {
                    mapa4.musicOpen = mapa4.music.open(resolveAssetPath(candidate));
                    if (mapa4.musicOpen) {
                        break;
                    }
                }
            }
        }
        if (mapa4.musicOpen && !mapa4.musicPlaying) {
            mapa4.music.playLoop();
            mapa4.musicPlaying = true;
        }
        mapa4.startSequencePending = false;
    }

    const float frameDelta = mapa4.skipFirstUpdateFrame ? 0.0f : deltaTime;
    mapa4.skipFirstUpdateFrame = false;

    if (!mapa4.gameOver && !mapa4.mission.levelComplete()) {
        const PlayerInput playerInput = buildPlayerInput(window, mapa4.player);
        const bool teleportDown = glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
        const bool teleportPressed = teleportDown && !lastTeleportKey;
        lastTeleportKey = teleportDown;
        const bool shieldDown = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
        const bool shieldPressed = shieldDown && !lastShieldKey;
        lastShieldKey = shieldDown;
        if (shieldPressed) {
            if (mapa4.shieldActive) {
                mapa4.shieldActive = false;
                mapa4.shieldTimer = 0.0f;
            } else {
                mapa4.shieldActive = true;
                mapa4.shieldTimer = 1.0f;
            }
        }
        if (mapa4.shieldActive) {
            mapa4.shieldTimer = std::max(0.0f, mapa4.shieldTimer - frameDelta);
            if (mapa4.shieldTimer <= 0.0f) {
                mapa4.shieldActive = false;
            }
        }
        mapa4.player.update(playerInput, mapa4.environment.collisionPreview(), mapa4.environment.worldMin(), mapa4.environment.worldMax(), frameDelta);
        mapa4.lightPickups.update(mapa4.player, now, frameDelta, mapa4.lightEnergy);
        mapa4.coinCollectDelay = std::max(0.0f, mapa4.coinCollectDelay - frameDelta);
        updateMarioMapa4PipeTeleport(mapa4, frameDelta, teleportPressed);
        if (mapa4.coinCollectDelay <= 0.0f) {
            mapa4.mission.update(mapa4.player, now);
        }
        const int collectedCount = mapa4.mission.collectedCount();
        if (mapa4.coinSoundOpen && collectedCount > mapa4.lastCollectedCount) {
            mapa4.coinSound.playOnce();
        }
        mapa4.lastCollectedCount = collectedCount;
        mapa4.damageCooldown = std::max(0.0f, mapa4.damageCooldown - frameDelta);
        if (mapa4.enemies.update(mapa4.player, mapa4.environment, frameDelta, now, mapa4.projectiles) && mapa4.damageCooldown <= 0.0f) {
            if (!mapa4.shieldActive) {
                ++mapa4.pendingHits;
                mapa4.damageCooldown = 0.28f;
                if (mapa4.pendingHits >= 2) {
                    mapa4.pendingHits = 0;
                    mapa4.health = std::max(0, mapa4.health - 1);
                    if (mapa4.health <= 0) {
                        mapa4.gameOver = true;
                    }
                }
            }
        }
    }

    updateGameplayCamera(mapa4.player, mapa4.environment, mapa4.mission, now, frameDelta);

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    const glm::mat4 view = glm::lookAt(gameplayCameraPosition, gameplayCameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 projection = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 180.0f);

    mapa4.projectileCooldown = std::max(0.0f, mapa4.projectileCooldown - frameDelta);
    if (!mapa4.gameOver && !mapa4.mission.levelComplete()) {
        // En 2D el jugador mantiene el disparo continuo; en 3D se desactiva.
        if (currentMode == PlayMode::Mode2D) {
            const bool attackDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            if (attackDown && mapa4.projectileCooldown <= 0.0f) {
                glm::vec3 aimDirection = map4MouseAimDirection(window, view, projection, mapa4.player);
                const float facing = std::sin(mapa4.player.facingYaw()) >= 0.0f ? 1.0f : -1.0f;
                aimDirection = glm::vec3(facing, 0.0f, 0.0f);
                const glm::vec3 spawnPosition = mapa4.player.position() + glm::vec3(aimDirection.x * 0.52f, 0.60f, 0.0f);
                mapa4.projectiles.push_back({
                    spawnPosition,
                    aimDirection * Map4ProjectileSpeed,
                    Map4ProjectileLifetime,
                    1
                });
                mapa4.projectileCooldown = Map4ProjectileCooldown;
            }
        } else {
            mapa4.projectileCooldown = std::min(mapa4.projectileCooldown, Map4ProjectileCooldown * 0.4f);
        }
    }
    updateMapa4Projectiles(mapa4, frameDelta);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.02f, 0.03f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const glm::vec3 flashlightAnchor = mapa4.player.position();
    const std::vector<glm::vec3> sunGlowPositions = mapa4.lightPickups.activeLightPositions(gameplayCameraPosition, 3);
    // La luz del jugador y el glow tenue de los Soles entran al shader como luces extra.
    uploadCommonSceneUniforms(sceneShader, mapa4.environment, gameplayCameraPosition, view, projection, now, &flashlightAnchor, mapa4.lightEnergy / Map4LightEnergyMaximum, &sunGlowPositions);
    lavaShader.use();
    lavaShader.setMat4("uView", view);
    lavaShader.setMat4("uProjection", projection);
    lavaShader.setFloat("uTime", now);

    renderMapa4Backdrop(sceneShader, mapa4.environment, gameplayCameraPosition, now);
    mapa4.environment.render(sceneShader, lavaShader, now, gameplayCameraPosition);
    mapa4.lightPickups.render(sceneShader, now, gameplayCameraPosition);
    mapa4.mission.render(sceneShader, now, gameplayCameraPosition);
    mapa4.enemies.render(sceneShader, now, gameplayCameraPosition);
    renderMapa4Projectiles(sceneShader, mapa4.projectiles, gameplayCameraPosition, now);
    if (mapa4.shieldActive) {
        renderMapa4Shield(sceneShader, mapa4.player, now);
    }
    mapa4.player.render(sceneShader);
}

void renderMapa4Hud(MenuContext& menu, const Mapa4Runtime& mapa4, int width, int height, float now) {
    // HUD propio: barra de luz y mensaje guía permanente del nivel.
    const Rect lightPanel{22.0f, 106.0f, 282.0f, 72.0f};
    drawRect(menu, {lightPanel.x + 6.0f, lightPanel.y + 7.0f, lightPanel.width, lightPanel.height}, {0.01f, 0.02f, 0.05f, 0.48f});
    drawRect(menu, lightPanel, {0.06f, 0.12f, 0.24f, 0.94f});
    drawText(menu, menu.luzJugador, lightPanel.x + 14.0f, lightPanel.y + 10.0f, glm::vec4(1.0f));
    const float ratio = std::clamp(mapa4.lightEnergy / Map4LightEnergyMaximum, 0.0f, 1.0f);
    drawRect(menu, {lightPanel.x + 14.0f, lightPanel.y + 44.0f, 224.0f, 16.0f}, {0.14f, 0.17f, 0.22f, 0.95f});
    drawRect(menu, {lightPanel.x + 14.0f, lightPanel.y + 44.0f, 224.0f * ratio, 16.0f}, {0.99f, 0.82f, 0.22f, 1.0f});

    const Rect hintPanel{
        22.0f,
        static_cast<float>(height) - 78.0f,
        std::min(470.0f, static_cast<float>(width) - 44.0f),
        44.0f
    };
    drawRect(menu, {hintPanel.x + 5.0f, hintPanel.y + 6.0f, hintPanel.width, hintPanel.height}, {0.01f, 0.02f, 0.05f, 0.34f});
    drawRect(menu, hintPanel, {0.09f, 0.16f, 0.28f, 0.78f});
    drawText(menu, menu.mapa4Hint, hintPanel.x + 14.0f, hintPanel.y + (hintPanel.height - menu.mapa4Hint.size.y) * 0.5f, glm::vec4(1.0f, 0.93f, 0.78f, 0.96f));

}

#else

int g_map4_compilation_stub = 0;

#endif

