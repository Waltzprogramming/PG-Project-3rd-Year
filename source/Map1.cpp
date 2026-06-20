#include "Map1.h"

#include "GameRuntime.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

namespace {
constexpr float Mundo1PlayerSpeed3D = 3.55f;
constexpr float Mundo1PlayerSpeed2D = 3.90f;
constexpr float Mundo1JumpBufferSeconds = 0.16f;
constexpr float Mundo1Camera3DDistance = 3.25f;
constexpr float Mundo1Camera3DBaseHeight = 0.42f;
constexpr float Mundo1Camera3DTargetHeight = 0.48f;
constexpr float Mundo1Camera2DDistance = 4.15f;
constexpr float Mundo1Camera2DHeight = 0.86f;
constexpr float Mundo1Camera2DTargetHeight = 0.56f;
constexpr float Mundo1CameraCollisionPadding = 0.18f;
constexpr float Mundo1CameraMinimumDistance = 1.25f;
constexpr float Mundo1SafeStepProbe = 0.34f;
constexpr float Mundo1SafeDropProbe = 4.25f;

bool mundo1FloorAt(const std::vector<Bounds>& colliders, float x, float z, float preferredY, float maxBelow, float maxAbove, float& floorY) {
    bool found = false;
    float bestScore = std::numeric_limits<float>::max();

    for (const Bounds& collider : colliders) {
        const float width = collider.halfExtent.x * 2.0f;
        const float depth = collider.halfExtent.z * 2.0f;
        const float area = width * depth;
        const float top = collider.center.y + collider.halfExtent.y;
        const bool floorLike = collider.halfExtent.y <= 0.42f && area >= 0.16f;
        const bool inside =
            x >= collider.center.x - collider.halfExtent.x - 0.10f &&
            x <= collider.center.x + collider.halfExtent.x + 0.10f &&
            z >= collider.center.z - collider.halfExtent.z - 0.10f &&
            z <= collider.center.z + collider.halfExtent.z + 0.10f;
        const bool reachable = top <= preferredY + maxAbove && top >= preferredY - maxBelow;
        if (!floorLike || !inside || !reachable) {
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

std::vector<Bounds> buildMundo1BasePlayerColliders(const Environment& environment) {
    const std::vector<Bounds>& rawColliders = environment.collisionPreview();
    const glm::vec3 worldMin = environment.worldMin();
    std::vector<Bounds> filtered;
    filtered.reserve(rawColliders.size());

    for (const Bounds& collider : rawColliders) {
        const float width = collider.halfExtent.x * 2.0f;
        const float height = collider.halfExtent.y * 2.0f;
        const float depth = collider.halfExtent.z * 2.0f;
        const float area = width * depth;
        const float top = collider.center.y + collider.halfExtent.y;
        const bool floorLike = collider.halfExtent.y <= 0.42f && area >= 0.16f;
        const bool majorBlocker = height >= 1.10f && (width >= 0.34f || depth >= 0.34f);
        const bool largeLowObstacle = height >= 0.56f && area >= 1.20f;
        const bool tinyTrim = area < 0.035f || width < 0.045f || depth < 0.045f;
        const bool playableHeight = top >= worldMin.y - 0.45f;

        if (!playableHeight || (tinyTrim && !majorBlocker)) {
            continue;
        }

        if (floorLike || majorBlocker || largeLowObstacle) {
            filtered.push_back(collider);
        }
    }

    return filtered.empty() ? rawColliders : filtered;
}

std::vector<Bounds> mundo1PlayerColliders(const Mundo2Runtime& mundo1) {
    const std::vector<Bounds>& baseColliders = mundo1.collisionBounds.empty()
        ? mundo1.environment.collisionPreview()
        : mundo1.collisionBounds;
    std::vector<Bounds> filtered;
    filtered.reserve(baseColliders.size());

    for (const Bounds& collider : baseColliders) {
        if (currentMode == PlayMode::Mode2D) {
            const bool nearLockedPlane =
                locked2DDepth >= collider.center.z - collider.halfExtent.z - 0.42f &&
                locked2DDepth <= collider.center.z + collider.halfExtent.z + 0.42f;
            if (!nearLockedPlane) {
                continue;
            }
        }
        filtered.push_back(collider);
    }

    return filtered.empty() ? baseColliders : filtered;
}

glm::vec3 mundo1MoveDirectionFromInput(const PlayerInput& input) {
    glm::vec3 desiredDirection(0.0f);

    if (input.mode == PlayMode::Mode3D) {
        const glm::vec3 cameraForward = glm::normalize(glm::vec3(-std::sin(input.cameraYawRadians), 0.0f, -std::cos(input.cameraYawRadians)));
        const glm::vec3 cameraRight = glm::normalize(glm::vec3(std::cos(input.cameraYawRadians), 0.0f, -std::sin(input.cameraYawRadians)));
        desiredDirection = cameraRight * input.move.x + cameraForward * input.move.y;
    } else {
        desiredDirection = {input.move.x, 0.0f, 0.0f};
    }

    if (glm::length(desiredDirection) > 1.0f) {
        desiredDirection = glm::normalize(desiredDirection);
    }
    return desiredDirection;
}

glm::vec3 findMundo1SpawnPoint(const Environment& environment, const std::vector<Bounds>& colliders) {
    const glm::vec3 requested = environment.recommendedSpawnPoint();
    const glm::vec3 worldMin = environment.worldMin();
    const glm::vec3 worldMax = environment.worldMax();
    glm::vec3 best = requested;
    float bestScore = std::numeric_limits<float>::max();

    for (const Bounds& collider : colliders) {
        const float top = collider.center.y + collider.halfExtent.y;
        const float width = collider.halfExtent.x * 2.0f;
        const float depth = collider.halfExtent.z * 2.0f;
        const float area = width * depth;
        const bool floorLike = collider.halfExtent.y <= 0.42f && area >= 0.20f;
        const bool validHeight = top >= worldMin.y - 0.25f && top <= worldMax.y + 0.60f;
        if (!floorLike || !validHeight) {
            continue;
        }

        const float safeX = std::max(collider.halfExtent.x - 0.24f, 0.0f);
        const float safeZ = std::max(collider.halfExtent.z - 0.24f, 0.0f);
        glm::vec3 candidate{
            std::clamp(requested.x, collider.center.x - safeX, collider.center.x + safeX),
            top,
            std::clamp(requested.z, collider.center.z - safeZ, collider.center.z + safeZ)
        };

        const float score = glm::length(glm::vec2(candidate.x - requested.x, candidate.z - requested.z)) +
            std::abs(candidate.y - requested.y) * 0.45f;
        if (score < bestScore) {
            bestScore = score;
            best = candidate;
        }
    }

    if (worldMax.x > worldMin.x && worldMax.z > worldMin.z) {
        best.x = std::clamp(best.x, worldMin.x + 0.34f, worldMax.x - 0.34f);
        best.z = std::clamp(best.z, worldMin.z + 0.34f, worldMax.z - 0.34f);
    }
    return best;
}

void rememberMundo1SafePosition(Mundo2Runtime& mundo1, const std::vector<Bounds>& colliders) {
    const glm::vec3 position = mundo1.player.position();
    float floorY = position.y;
    if (!mundo1FloorAt(colliders, position.x, position.z, position.y, 0.40f, 0.18f, floorY)) {
        return;
    }

    if (!mundo1.player.grounded() && std::abs(position.y - floorY) > 0.12f) {
        return;
    }

    mundo1.safePlayerPosition = {position.x, floorY, position.z};
    mundo1.hasSafePlayerPosition = true;
}

void guardMundo1Edges(const Player& player, PlayerInput& input, const std::vector<Bounds>& colliders) {
    if (!player.grounded() || glm::length(input.move) <= 0.01f) {
        return;
    }

    const glm::vec3 direction = mundo1MoveDirectionFromInput(input);
    if (glm::length(direction) <= 0.01f) {
        return;
    }

    const glm::vec3 current = player.position();
    const glm::vec3 probe = current + direction * Mundo1SafeStepProbe;
    float currentFloorY = current.y;
    float nextFloorY = probe.y;
    const bool hasCurrentFloor = mundo1FloorAt(colliders, current.x, current.z, current.y, 0.55f, 0.20f, currentFloorY);
    const bool hasNextFloor = mundo1FloorAt(colliders, probe.x, probe.z, current.y, 1.15f, 0.34f, nextFloorY);
    if (hasCurrentFloor && !hasNextFloor) {
        input.move = glm::vec2(0.0f);
    }
}

void rescueMundo1FromVoid(Mundo2Runtime& mundo1, const std::vector<Bounds>& colliders) {
    const glm::vec3 position = mundo1.player.position();
    float floorY = position.y;
    const bool floorBelow = mundo1FloorAt(colliders, position.x, position.z, position.y, Mundo1SafeDropProbe, 0.45f, floorY);
    if (floorBelow) {
        rememberMundo1SafePosition(mundo1, colliders);
        return;
    }

    const bool belowWorld = position.y <= mundo1.environment.worldMin().y + 0.25f;
    const bool fallingFast = mundo1.player.velocity().y < -4.5f;
    const bool tooFarFromSafe = mundo1.hasSafePlayerPosition && position.y < mundo1.safePlayerPosition.y - 0.75f;
    if (mundo1.hasSafePlayerPosition && (belowWorld || fallingFast || tooFarFromSafe)) {
        mundo1.player.teleportTo(mundo1.safePlayerPosition);
    }
}

void prepareMundo1Jump(Mundo2Runtime& mundo1, PlayerInput& input, const std::vector<Bounds>& colliders, float timeSeconds) {
    if (input.jumpPressed) {
        mundo1.jumpBufferUntil = static_cast<double>(timeSeconds) + Mundo1JumpBufferSeconds;
    }

    if (timeSeconds > static_cast<float>(mundo1.jumpBufferUntil)) {
        input.jumpPressed = false;
        return;
    }

    input.jumpPressed = true;
    if (mundo1.player.grounded()) {
        mundo1.jumpBufferUntil = 0.0;
        return;
    }

    float floorY = mundo1.player.position().y;
    if (!mundo1FloorAt(colliders, mundo1.player.position().x, mundo1.player.position().z, mundo1.player.position().y, 0.45f, 0.30f, floorY)) {
        input.jumpPressed = false;
        return;
    }

    const float distanceToFloor = mundo1.player.position().y - floorY;
    if (distanceToFloor < -0.06f || distanceToFloor > 0.30f) {
        input.jumpPressed = false;
        return;
    }

    PlayerInput settleInput = input;
    settleInput.jumpPressed = false;
    mundo1.player.update(settleInput, colliders, mundo1.environment.worldMin(), mundo1.environment.worldMax(), 1.0f / 60.0f);
    if (!mundo1.player.grounded()) {
        input.jumpPressed = false;
        return;
    }

    mundo1.player.update(input, colliders, mundo1.environment.worldMin(), mundo1.environment.worldMax(), 0.0f);
    input.jumpPressed = false;
    mundo1.jumpBufferUntil = 0.0;
}

bool mundo1CameraColliderRelevant(const Bounds& collider, const glm::vec3& from, const glm::vec3& to) {
    const float width = collider.halfExtent.x * 2.0f;
    const float height = collider.halfExtent.y * 2.0f;
    const float depth = collider.halfExtent.z * 2.0f;
    const float top = collider.center.y + collider.halfExtent.y;
    const float lowestRayY = std::min(from.y, to.y);
    const bool visibleBlocker = width >= 0.08f && depth >= 0.08f && height >= 0.08f;
    return visibleBlocker && top >= lowestRayY - 0.32f;
}

bool mundo1SegmentHitsBounds(const glm::vec3& start, const glm::vec3& end, const Bounds& bounds, float padding, float& hitT) {
    const glm::vec3 expandedHalf = bounds.halfExtent + glm::vec3(padding);
    const glm::vec3 minBounds = bounds.center - expandedHalf;
    const glm::vec3 maxBounds = bounds.center + expandedHalf;
    const glm::vec3 direction = end - start;
    float tMin = 0.0f;
    float tMax = 1.0f;

    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(direction[axis]) < 0.00001f) {
            if (start[axis] < minBounds[axis] || start[axis] > maxBounds[axis]) {
                return false;
            }
            continue;
        }

        float t1 = (minBounds[axis] - start[axis]) / direction[axis];
        float t2 = (maxBounds[axis] - start[axis]) / direction[axis];
        if (t1 > t2) {
            std::swap(t1, t2);
        }

        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) {
            return false;
        }
    }

    hitT = tMin;
    return true;
}

glm::vec3 resolveMundo1CameraPosition(const glm::vec3& target, const glm::vec3& desiredPosition, const std::vector<Bounds>& colliders) {
    const glm::vec3 ray = desiredPosition - target;
    const float rayLength = glm::length(ray);
    if (rayLength < 0.001f) {
        return desiredPosition;
    }

    float closestT = 1.0f;
    for (const Bounds& collider : colliders) {
        if (!mundo1CameraColliderRelevant(collider, target, desiredPosition)) {
            continue;
        }

        float hitT = 1.0f;
        if (mundo1SegmentHitsBounds(target, desiredPosition, collider, Mundo1CameraCollisionPadding, hitT) &&
            hitT > 0.035f &&
            hitT < closestT) {
            closestT = hitT;
        }
    }

    if (closestT >= 0.999f) {
        return desiredPosition;
    }

    const float safeT = std::max(Mundo1CameraMinimumDistance / rayLength, closestT - 0.055f);
    return target + ray * std::clamp(safeT, 0.08f, 1.0f);
}

glm::vec3 chooseMundo1StarCameraPosition(const Player& player, const glm::vec3& target, const std::vector<Bounds>& colliders) {
    glm::vec3 preferred = player.position() - target;
    preferred.y = 0.0f;
    if (glm::length(preferred) < 0.1f) {
        preferred = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    preferred = glm::normalize(preferred);

    const std::array<glm::vec3, 8> candidates = {
        preferred,
        glm::vec3(-preferred.z, 0.0f, preferred.x),
        glm::vec3(preferred.z, 0.0f, -preferred.x),
        -preferred,
        glm::normalize(preferred + glm::vec3(-preferred.z, 0.0f, preferred.x)),
        glm::normalize(preferred + glm::vec3(preferred.z, 0.0f, -preferred.x)),
        glm::normalize(-preferred + glm::vec3(-preferred.z, 0.0f, preferred.x)),
        glm::normalize(-preferred + glm::vec3(preferred.z, 0.0f, -preferred.x))
    };

    glm::vec3 bestPosition = target + preferred * 4.55f + glm::vec3(0.0f, 2.10f, 0.0f);
    float bestScore = std::numeric_limits<float>::max();

    for (const glm::vec3& direction : candidates) {
        const glm::vec3 desired = target + direction * 4.55f + glm::vec3(0.0f, 2.10f, 0.0f);
        const glm::vec3 resolved = resolveMundo1CameraPosition(target, desired, colliders);
        const float blockedPenalty = glm::length(desired - resolved) * 4.5f;
        const float preferredPenalty = (1.0f - glm::dot(direction, preferred)) * 0.28f;
        const float continuityPenalty = cameraInitialized ? glm::length(resolved - gameplayCameraPosition) * 0.05f : 0.0f;
        const float score = blockedPenalty + preferredPenalty + continuityPenalty;
        if (score < bestScore) {
            bestScore = score;
            bestPosition = resolved;
        }
    }

    return bestPosition;
}

void updateMundo1GameplayCamera(Mundo2Runtime& mundo1, float timeSeconds, float dt) {
    const std::vector<Bounds>& colliders = mundo1.collisionBounds.empty()
        ? mundo1.environment.collisionPreview()
        : mundo1.collisionBounds;
    glm::vec3 desiredLead(0.0f);

    if (currentMode == PlayMode::Mode3D && dt > 0.0001f) {
        glm::vec3 playerStep = mundo1.player.position() - mundo1.previousCameraPlayerPosition;
        playerStep.y = 0.0f;
        const float stepLengthSq = glm::dot(playerStep, playerStep);
        if (stepLengthSq > 0.001f * 0.001f && stepLengthSq < 1.2f * 1.2f) {
            const glm::vec3 playerVelocity = playerStep / dt;
            const float speedSq = glm::dot(playerVelocity, playerVelocity);
            if (speedSq > 0.05f * 0.05f) {
                const float speed = std::sqrt(speedSq);
                desiredLead = (playerVelocity / speed) * std::min(0.26f, speed * 0.055f);
            }
        }
    }

    const float leadSmoothing = dt > 0.0f ? 1.0f - std::exp(-8.0f * dt) : 1.0f;
    mundo1.cameraLead = glm::mix(mundo1.cameraLead, desiredLead, leadSmoothing);
    if (currentMode != PlayMode::Mode3D) {
        mundo1.cameraLead = glm::mix(mundo1.cameraLead, glm::vec3(0.0f), leadSmoothing);
    }
    mundo1.previousCameraPlayerPosition = mundo1.player.position();

    glm::vec3 desiredTarget = mundo1.player.position() + glm::vec3(0.0f, Mundo1Camera3DTargetHeight, 0.0f);
    glm::vec3 desiredPosition;

    if (mundo1.mission.starFocusActive(timeSeconds)) {
        const glm::vec3 starTarget = mundo1.mission.starPosition() + glm::vec3(0.0f, 0.38f, 0.0f);
        desiredTarget = starTarget;
        desiredPosition = chooseMundo1StarCameraPosition(mundo1.player, starTarget, colliders);
    } else if (currentMode == PlayMode::Mode3D) {
        const float yaw = glm::radians(cameraYawDegrees);
        const float pitch = glm::radians(cameraPitchDegrees);
        const float horizontalDistance = std::cos(pitch) * Mundo1Camera3DDistance;
        const glm::vec3 orbitOffset(
            std::sin(yaw) * horizontalDistance,
            Mundo1Camera3DBaseHeight + std::sin(pitch) * Mundo1Camera3DDistance,
            std::cos(yaw) * horizontalDistance);
        desiredTarget = mundo1.player.position() + glm::vec3(0.0f, Mundo1Camera3DTargetHeight, 0.0f) + mundo1.cameraLead;
        desiredPosition = resolveMundo1CameraPosition(desiredTarget, desiredTarget + orbitOffset, colliders);
    } else {
        desiredTarget = mundo1.player.position() + glm::vec3(0.0f, Mundo1Camera2DTargetHeight, 0.0f);
        desiredPosition = resolveMundo1CameraPosition(desiredTarget, desiredTarget + glm::vec3(0.0f, Mundo1Camera2DHeight, Mundo1Camera2DDistance), colliders);
    }

    const float positionSmoothing = dt > 0.0f ? 1.0f - std::exp(-(currentMode == PlayMode::Mode3D ? 6.2f : 7.4f) * dt) : 1.0f;
    const float targetSmoothing = dt > 0.0f ? 1.0f - std::exp(-(currentMode == PlayMode::Mode3D ? 9.0f : 7.4f) * dt) : 1.0f;
    if (!cameraInitialized) {
        gameplayCameraPosition = desiredPosition;
        gameplayCameraTarget = desiredTarget;
        cameraInitialized = true;
    } else {
        gameplayCameraTarget = glm::mix(gameplayCameraTarget, desiredTarget, targetSmoothing);
        gameplayCameraPosition = glm::mix(gameplayCameraPosition, desiredPosition, positionSmoothing);
        gameplayCameraPosition = resolveMundo1CameraPosition(gameplayCameraTarget, gameplayCameraPosition, colliders);
    }
}

void initializeMundo2HudResources(Mundo2HudResources& hud) {
    if (hud.initialized) {
        return;
    }

    const glm::vec3 white(1.0f);
    const glm::vec3 titleColor(1.0f, 0.92f, 0.35f);
    hud.promptHablarToad = createTextSprite(L"Press F to talk", 28, white, 330, false, true);
    hud.nombreToad = createTextSprite(L"Toad", 30, titleColor, 170, false, true);
    hud.dialogoToad = createTextSprite(
        L"Oh nooo! All these enemies came to get in the way...\n"
        L"To win, collect all 10 coins on the map. Once you have them,\n"
        L"a crystal star will appear. Take it to complete the level.",
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

    loadWorldOnePlayerSprites(mundo2.player);
    mundo2.player.configureMovementSpeeds(Mundo1PlayerSpeed3D, Mundo1PlayerSpeed2D);
    mundo2.collisionBounds = buildMundo1BasePlayerColliders(mundo2.environment);
    const glm::vec3 spawnPoint = findMundo1SpawnPoint(mundo2.environment, mundo2.collisionBounds);
    mundo2.player.spawnAt(spawnPoint);
    mundo2.mission.initialize();
    mundo2.mission.reset(mundo2.environment, spawnPoint);
    mundo2.toad.initialize();
    mundo2.toad.reset(mundo2.environment, spawnPoint);
    mundo2.lastInteractKey = false;
    mundo2.jumpBufferUntil = 0.0;
    mundo2.safePlayerPosition = spawnPoint;
    mundo2.hasSafePlayerPosition = true;
    mundo2.cameraLead = glm::vec3(0.0f);
    mundo2.previousCameraPlayerPosition = spawnPoint;
    resetGameplayView(mundo2.player);
    currentMode = PlayMode::Mode3D;
    updateMundo1GameplayCamera(mundo2, static_cast<float>(glfwGetTime()), 0.0f);
    beginLevelIntro(mundo2.environment, mundo2.player, static_cast<float>(glfwGetTime()));

    std::cout << "World 1 ready. Collision volumes: " << mundo2.environment.collisionPreview().size() << std::endl;
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
    mundo2.collisionBounds.clear();
    mundo2.jumpBufferUntil = 0.0;
    mundo2.hasSafePlayerPosition = false;
    mundo2.cameraLead = glm::vec3(0.0f);
    mundo2.previousCameraPlayerPosition = glm::vec3(0.0f);
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
    if (levelIntroActive()) {
        consumeLevelIntroInput(window);
        mundo2.lastInteractKey = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
        updateLevelIntroCamera(now);
        const std::vector<Bounds>& cameraColliders = mundo2.collisionBounds.empty()
            ? mundo2.environment.collisionPreview()
            : mundo2.collisionBounds;
        gameplayCameraPosition = resolveMundo1CameraPosition(gameplayCameraTarget, gameplayCameraPosition, cameraColliders);
    } else if (!mundo2.mission.levelComplete()) {
        PlayerInput playerInput = buildPlayerInput(window, mundo2.player);
        const bool interactDown = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
        const bool interactPressed = interactDown && !mundo2.lastInteractKey;
        mundo2.lastInteractKey = interactDown;

        std::vector<Bounds> playerColliders = mundo1PlayerColliders(mundo2);
        appendDimensionRestrictionColliders(playerColliders, mundo2.environment, locked2DDepth);
        rememberMundo1SafePosition(mundo2, playerColliders);
        guardMundo1Edges(mundo2.player, playerInput, playerColliders);
        prepareMundo1Jump(mundo2, playerInput, playerColliders, now);
        mundo2.player.update(playerInput, playerColliders, mundo2.environment.worldMin(), mundo2.environment.worldMax(), deltaTime);
        rescueMundo1FromVoid(mundo2, playerColliders);
        mundo2.mission.update(mundo2.player, now);
        mundo2.toad.update(mundo2.player, interactPressed, now);
        updateMundo1GameplayCamera(mundo2, now, deltaTime);
    }

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
