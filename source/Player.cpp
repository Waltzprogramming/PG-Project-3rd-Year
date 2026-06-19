#include "Player.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>

namespace {
constexpr float WorldOneSpriteHeight = 1.15f;
constexpr float SharedSpriteRunFramesPerSecond = 12.0f;
constexpr float SharedSpriteJumpFramesPerSecond = 12.0f;
constexpr float SharedSpriteWalkSwayFrequency = 14.0f;
constexpr float SharedSpriteWalkBounce = 0.035f;
constexpr float SharedSpriteWalkTiltDegrees = 4.0f;
constexpr float SharedPlayerJumpSpeed = 7.25f;

bool intersects(const Bounds& a, const Bounds& b) {
    const glm::vec3 delta = glm::abs(a.center - b.center);
    const glm::vec3 total = a.halfExtent + b.halfExtent;
    return delta.x < total.x && delta.y < total.y && delta.z < total.z;
}

glm::vec3 approach(glm::vec3 current, glm::vec3 target, float maxDelta) {
    const glm::vec3 delta = target - current;
    const float distance = glm::length(delta);
    if (distance <= maxDelta || distance <= 0.00001f) {
        return target;
    }
    return current + delta / distance * maxDelta;
}

std::filesystem::path resolvePath(const std::string& rawPath) {
    if (rawPath.empty()) {
        return {};
    }

    const std::filesystem::path original(rawPath);
    const std::filesystem::path fileName = original.filename();
    const std::filesystem::path candidates[] = {
        original,
        std::filesystem::path("assets") / "characters" / "deadpool" / "textures" / fileName,
        std::filesystem::path("assets") / "mapa 4" / "luffy" / "textures" / fileName,
        std::filesystem::path("assets") / "characters" / fileName,
        std::filesystem::path("assets") / "characters" / "luffy" / "textures" / fileName,
        std::filesystem::path("assets") / "textures" / "characters" / fileName,
        std::filesystem::path("..") / ".." / "assets" / "characters" / "deadpool" / "textures" / fileName,
        std::filesystem::path("..") / ".." / "assets" / "mapa 4" / "luffy" / "textures" / fileName,
        std::filesystem::path("..") / ".." / original,
        std::filesystem::path("..") / ".." / "assets" / "characters" / fileName,
        std::filesystem::path("..") / ".." / "assets" / "characters" / "luffy" / "textures" / fileName,
        std::filesystem::path("..") / ".." / "assets" / "textures" / "characters" / fileName
    };

    for (const auto& candidate : candidates) {
        if (!candidate.empty() && std::filesystem::exists(candidate)) {
            return std::filesystem::weakly_canonical(candidate);
        }
    }
    return original;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

int trailingNumber(const std::filesystem::path& path) {
    const std::string stem = path.stem().string();
    int value = 0;
    int multiplier = 1;
    bool found = false;
    for (auto it = stem.rbegin(); it != stem.rend(); ++it) {
        if (!std::isdigit(static_cast<unsigned char>(*it))) {
            break;
        }
        found = true;
        value += (*it - '0') * multiplier;
        multiplier *= 10;
    }
    return found ? value : std::numeric_limits<int>::max();
}

Vertex makeSpriteVertex(const glm::vec3& position, const glm::vec2& uv) {
    return {position, {0.0f, 0.0f, 1.0f}, uv, glm::vec4(1.0f)};
}

Mesh createWorldOneSpriteMesh(int textureWidth, int textureHeight, bool mirrored = false) {
    const float aspect = static_cast<float>(std::max(textureWidth, 1)) / static_cast<float>(std::max(textureHeight, 1));
    const float width = WorldOneSpriteHeight * aspect;
    const float halfWidth = width * 0.5f;
    const float leftU = mirrored ? 1.0f : 0.0f;
    const float rightU = mirrored ? 0.0f : 1.0f;
    const std::vector<Vertex> vertices = {
        makeSpriteVertex({halfWidth, WorldOneSpriteHeight, 0.0f}, {rightU, 1.0f}),
        makeSpriteVertex({halfWidth, 0.0f, 0.0f}, {rightU, 0.0f}),
        makeSpriteVertex({-halfWidth, 0.0f, 0.0f}, {leftU, 0.0f}),
        makeSpriteVertex({-halfWidth, WorldOneSpriteHeight, 0.0f}, {leftU, 1.0f})
    };
    const std::vector<unsigned int> indices = {0, 1, 2, 0, 2, 3};

    Mesh mesh;
    mesh.upload(vertices, indices);
    return mesh;
}

std::string normalizeAssetKey(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    std::transform(path.begin(), path.end(), path.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return path;
}
}

bool Player::load(const std::string& modelPath) {
    m_parts.clear();
    m_runSpriteFrames.clear();
    m_jumpSpriteFrames.clear();
    m_textures.clear();
    m_spritePlayer = false;
    m_spriteMoving = false;
    m_animationTime = 0.0f;
    m_jumpAnimationTime = 0.0f;
    m_spriteBrightness = 1.0f;

    LoadedModel model = ModelLoader::loadModel(resolvePath(modelPath).string());
    if (model.meshes.empty()) {
        std::cerr << "Could not load player model: " << modelPath << ". Using fallback cube." << std::endl;
        Part fallback;
        fallback.mesh = Mesh::cube();
        fallback.material.baseColor = {0.95f, 0.72f, 0.20f};
        fallback.material.roughness = 1.0f;
        fallback.material.fogAmount = 0.35f;
        m_parts.push_back(std::move(fallback));
        m_modelMin = {-0.5f, -0.5f, -0.5f};
        m_modelMax = {0.5f, 0.5f, 0.5f};
    } else {
        m_modelMin = model.minBounds;
        m_modelMax = model.maxBounds;
        m_parts.reserve(model.meshes.size());

        for (LoadedMesh& mesh : model.meshes) {
            Part part;
            if (mesh.materialIndex < model.materials.size()) {
                const LoadedMaterial& material = model.materials[mesh.materialIndex];
                part.material.baseColor = material.diffuseColor;
                part.material.opacity = material.opacity;
                part.material.texture = loadTexture(material);
            }
            part.material.roughness = 1.0f;
            part.material.checkerStrength = 0.0f;
            part.material.fogAmount = 0.30f;
            part.mesh = std::move(mesh.mesh);
            m_parts.push_back(std::move(part));
        }
    }

    m_modelCenter = (m_modelMin + m_modelMax) * 0.5f;
    const std::string normalizedModelPath = normalizeAssetKey(resolvePath(modelPath).string());
    m_deadpoolVariant = normalizedModelPath.find("assets/characters/deadpool") != std::string::npos;
    m_marioMapVariant =
        normalizedModelPath.find("assets/mapa 4/luffy") != std::string::npos ||
        m_deadpoolVariant;
    const float desiredHeight = m_deadpoolVariant ? 0.62f : (m_marioMapVariant ? 0.50f : 0.76f);
    const glm::vec3 collisionHalf = m_deadpoolVariant
        ? glm::vec3(0.17f, desiredHeight * 0.5f, 0.13f)
        : (m_marioMapVariant
            ? glm::vec3(0.14f, desiredHeight * 0.5f, 0.11f)
            : glm::vec3(0.16f, desiredHeight * 0.5f, 0.12f));
    const float maxSpeed3D = m_marioMapVariant ? 2.55f : 4.25f;
    const float maxSpeed2D = m_marioMapVariant ? 2.85f : 4.65f;
    configureCharacterMetrics(desiredHeight, collisionHalf, 0.0f, maxSpeed3D, maxSpeed2D);
    m_modelYawOffset = 0.0f;
    return true;
}

bool Player::loadWorldOneSprites(const std::string& playerAssetRoot) {
    m_parts.clear();
    m_runSpriteFrames.clear();
    m_jumpSpriteFrames.clear();
    m_textures.clear();
    m_marioMapVariant = false;
    m_deadpoolVariant = false;

    const std::filesystem::path root(playerAssetRoot);
    if (!loadSpriteSequence((root / "caminar").string(), m_runSpriteFrames)) {
        std::cerr << "World 1 player walking sprites could not be loaded from "
            << (root / "caminar").string() << "." << std::endl;
        return load("assets/characters/mario64_pinix_style/model/scene.gltf");
    }
    loadSpriteSequence((root / "saltar").string(), m_jumpSpriteFrames);

    m_modelMin = {-WorldOneSpriteHeight * 0.5f, 0.0f, 0.0f};
    m_modelMax = {WorldOneSpriteHeight * 0.5f, WorldOneSpriteHeight, 0.0f};
    m_modelCenter = (m_modelMin + m_modelMax) * 0.5f;
    configureCharacterMetrics(
        WorldOneSpriteHeight,
        glm::vec3(0.16f, WorldOneSpriteHeight * 0.5f, 0.12f),
        0.0f,
        4.25f,
        4.65f);
    m_animationTime = 0.0f;
    m_jumpAnimationTime = 0.0f;
    m_spriteBrightness = 1.0f;
    m_spriteMoving = false;
    m_modelYawOffset = 0.0f;
    m_spritePlayer = true;
    return true;
}

void Player::configureCharacterMetrics(float desiredHeight, const glm::vec3& collisionHalf, float visualYOffset, float maxSpeed3D, float maxSpeed2D) {
    const float modelHeight = std::max(m_modelMax.y - m_modelMin.y, 0.001f);
    m_modelScale = desiredHeight / modelHeight;
    m_collisionHalf = collisionHalf;
    m_visualYOffset = visualYOffset;
    configureMovementSpeeds(maxSpeed3D, maxSpeed2D);
}

void Player::configureMovementSpeeds(float maxSpeed3D, float maxSpeed2D) {
    m_maxSpeed3D = maxSpeed3D;
    m_maxSpeed2D = maxSpeed2D;
}

void Player::setSpriteBrightness(float brightness) {
    m_spriteBrightness = std::clamp(brightness, 0.0f, 1.0f);
}

void Player::spawnAt(const glm::vec3& feetPosition) {
    m_position = feetPosition;
    m_spawnPoint = feetPosition;
    m_velocity = {0.0f, 0.0f, 0.0f};
    m_grounded = false;
    m_animationTime = 0.0f;
    m_jumpAnimationTime = 0.0f;
    m_spriteMoving = false;
}

void Player::teleportTo(const glm::vec3& feetPosition) {
    m_position = feetPosition;
    m_velocity = {0.0f, 0.0f, 0.0f};
    m_grounded = false;
    m_animationTime = 0.0f;
    m_jumpAnimationTime = 0.0f;
    m_spriteMoving = false;
}

void Player::update(const PlayerInput& input, const std::vector<Bounds>& colliders, const glm::vec3& worldMin, const glm::vec3& worldMax, float deltaTime) {
    const float dt = std::clamp(deltaTime, 0.0f, 1.0f / 30.0f);

    m_lastMode = input.mode;
    m_lastCameraYawRadians = input.cameraYawRadians;
    updateHorizontalVelocity(input, dt);

    if (input.jumpPressed && m_grounded) {
        m_velocity.y = SharedPlayerJumpSpeed;
        m_grounded = false;
    }

    m_velocity.y = std::max(m_velocity.y - 20.0f * dt, -26.0f);

    moveAndCollide(0, m_velocity.x * dt, colliders);
    moveAndCollide(2, m_velocity.z * dt, colliders);
    m_grounded = false;
    moveAndCollide(1, m_velocity.y * dt, colliders);

    keepInsideWorld(worldMin, worldMax);

    if (m_position.y < worldMin.y - 6.0f) {
        spawnAt(m_spawnPoint);
    }

    const bool movementInputActive = glm::length(input.move) > 0.01f;
    const float horizontalSpeed = glm::length(glm::vec2(m_velocity.x, m_velocity.z));
    m_spriteMoving = m_grounded && (movementInputActive || horizontalSpeed > 0.18f);
    if (m_spriteMoving || !m_grounded) {
        m_animationTime += dt;
    } else {
        m_animationTime = 0.0f;
    }
    if (!m_grounded) {
        m_jumpAnimationTime += dt;
    } else {
        m_jumpAnimationTime = 0.0f;
    }
}

void Player::render(const Shader& shader) const {
    shader.use();
    if (m_spritePlayer) {
        const SpriteFrame* frame = currentSpriteFrame();
        if (frame == nullptr || !frame->texture || !frame->texture->valid()) {
            return;
        }

        Material material;
        material.baseColor = glm::vec3(m_spriteBrightness);
        material.roughness = 1.0f;
        material.checkerStrength = 0.0f;
        material.fogAmount = 0.0f;
        material.unlit = true;
        material.opacity = 1.0f;
        material.texture = frame->texture;
        shader.setMat4("uModel", spriteModelMatrix());
        bindMaterial(shader, material);
        const bool mirrorSprite = m_spriteFacingLeft;
        const Mesh& spriteMesh = mirrorSprite && frame->mirroredMesh.valid()
            ? frame->mirroredMesh
            : frame->mesh;
        spriteMesh.draw();
        return;
    }

    shader.setMat4("uModel", modelMatrix());

    for (const Part& part : m_parts) {
        bindMaterial(shader, part.material);
        part.mesh.draw();
    }
}

Bounds Player::bounds() const {
    Bounds result;
    result.center = m_position + glm::vec3(0.0f, m_collisionHalf.y, 0.0f);
    result.halfExtent = m_collisionHalf;
    return result;
}

void Player::updateHorizontalVelocity(const PlayerInput& input, float deltaTime) {
    glm::vec3 desiredDirection(0.0f);

    if (input.mode == PlayMode::Mode3D) {
        const glm::vec3 cameraForward = glm::normalize(glm::vec3(-std::sin(input.cameraYawRadians), 0.0f, -std::cos(input.cameraYawRadians)));
        const glm::vec3 cameraRight = glm::normalize(glm::vec3(std::cos(input.cameraYawRadians), 0.0f, -std::sin(input.cameraYawRadians)));
        desiredDirection = cameraRight * input.move.x + cameraForward * input.move.y;
    } else {
        desiredDirection = {input.move.x, 0.0f, 0.0f};
        const float depthError = input.lockedDepth - m_position.z;
        desiredDirection.z = std::clamp(depthError * 1.6f, -1.0f, 1.0f);
    }

    if (glm::length(desiredDirection) > 1.0f) {
        desiredDirection = glm::normalize(desiredDirection);
    }

    if (input.mode == PlayMode::Mode2D) {
        if (input.move.x > 0.01f) {
            m_facingYaw = glm::half_pi<float>();
            m_spriteFacingLeft = false;
        } else if (input.move.x < -0.01f) {
            m_facingYaw = -glm::half_pi<float>();
            m_spriteFacingLeft = true;
        }
    } else {
        if (input.move.x > 0.01f) {
            m_spriteFacingLeft = false;
        } else if (input.move.x < -0.01f) {
            m_spriteFacingLeft = true;
        } else {
            m_spriteFacingLeft = false;
        }

        if (glm::length(desiredDirection) > 0.01f) {
            m_facingYaw = std::atan2(desiredDirection.x, desiredDirection.z);
        }
    }

    const float maxSpeed = input.mode == PlayMode::Mode3D ? m_maxSpeed3D : m_maxSpeed2D;
    const glm::vec3 targetVelocity = desiredDirection * maxSpeed;
    const float acceleration = m_grounded ? 30.0f : 12.0f;
    const float deceleration = m_grounded ? 18.0f : 5.0f;
    const float maxDelta = (glm::length(desiredDirection) > 0.01f ? acceleration : deceleration) * deltaTime;

    glm::vec3 horizontal(m_velocity.x, 0.0f, m_velocity.z);
    horizontal = approach(horizontal, {targetVelocity.x, 0.0f, targetVelocity.z}, maxDelta);
    m_velocity.x = horizontal.x;
    m_velocity.z = horizontal.z;
}

void Player::moveAndCollide(int axis, float amount, const std::vector<Bounds>& colliders) {
    if (std::abs(amount) <= 0.000001f) {
        return;
    }

    m_position[axis] += amount;
    Bounds current = bounds();

    for (const Bounds& collider : colliders) {
        if (!intersects(current, collider)) {
            continue;
        }

        if (amount > 0.0f) {
            current.center[axis] = collider.center[axis] - collider.halfExtent[axis] - current.halfExtent[axis] - 0.001f;
        } else {
            current.center[axis] = collider.center[axis] + collider.halfExtent[axis] + current.halfExtent[axis] + 0.001f;
        }

        m_position[axis] = current.center[axis] - (axis == 1 ? m_collisionHalf.y : 0.0f);

        if (axis == 1) {
            if (amount < 0.0f) {
                m_grounded = true;
            }
            m_velocity.y = 0.0f;
        } else if (axis == 0) {
            m_velocity.x = 0.0f;
        } else if (axis == 2) {
            m_velocity.z = 0.0f;
        }

        current = bounds();
    }
}

void Player::keepInsideWorld(const glm::vec3& worldMin, const glm::vec3& worldMax) {
    if (worldMax.x <= worldMin.x || worldMax.z <= worldMin.z) {
        return;
    }

    const float margin = 0.18f;
    const float minX = worldMin.x + m_collisionHalf.x + margin;
    const float maxX = worldMax.x - m_collisionHalf.x - margin;
    const float minZ = worldMin.z + m_collisionHalf.z + margin;
    const float maxZ = worldMax.z - m_collisionHalf.z - margin;

    m_position.x = std::clamp(m_position.x, minX, maxX);
    m_position.z = std::clamp(m_position.z, minZ, maxZ);
}

glm::mat4 Player::modelMatrix() const {
    glm::mat4 model(1.0f);
    model = glm::translate(model, m_position + glm::vec3(0.0f, m_visualYOffset, 0.0f));
    model = glm::rotate(model, m_facingYaw + m_modelYawOffset, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(m_modelScale));
    model = glm::translate(model, {-m_modelCenter.x, -m_modelMin.y, -m_modelCenter.z});
    return model;
}

glm::mat4 Player::spriteModelMatrix() const {
    float yaw = m_lastCameraYawRadians;
    if (m_lastMode == PlayMode::Mode2D) {
        yaw = 0.0f;
    }

    const float step = m_spriteMoving ? std::sin(m_animationTime * SharedSpriteWalkSwayFrequency) : 0.0f;
    const float bounce = m_spriteMoving ? std::abs(step) * SharedSpriteWalkBounce : 0.0f;
    const float walkTilt = m_spriteMoving ? step * SharedSpriteWalkTiltDegrees : 0.0f;
    const float jumpStrength = !m_grounded
        ? std::clamp(std::abs(m_velocity.y) / SharedPlayerJumpSpeed, 0.0f, 1.0f)
        : 0.0f;
    const float jumpTilt = !m_grounded
        ? (m_velocity.y >= 0.0f ? -10.0f : 8.0f) * jumpStrength
        : 0.0f;
    const glm::vec3 jumpScale = !m_grounded
        ? glm::vec3(1.0f - jumpStrength * 0.06f, 1.0f + jumpStrength * 0.10f, 1.0f)
        : glm::vec3(1.0f);

    glm::mat4 model(1.0f);
    model = glm::translate(model, m_position + glm::vec3(0.0f, m_visualYOffset + bounce, 0.0f));
    model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(walkTilt + jumpTilt), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, jumpScale);
    model = glm::scale(model, glm::vec3(m_modelScale));
    return model;
}

const Player::SpriteFrame* Player::currentSpriteFrame() const {
    const std::vector<SpriteFrame>* frames = &m_runSpriteFrames;
    bool loop = true;
    if (!m_grounded && !m_jumpSpriteFrames.empty()) {
        frames = &m_jumpSpriteFrames;
        loop = false;
    }

    if (frames->empty()) {
        return nullptr;
    }

    const float time = loop ? m_animationTime : m_jumpAnimationTime;
    const float framesPerSecond = loop ? SharedSpriteRunFramesPerSecond : SharedSpriteJumpFramesPerSecond;
    size_t index = static_cast<size_t>(std::max(0.0f, time) * framesPerSecond);
    index = loop ? index % frames->size() : std::min(index, frames->size() - 1);
    return &(*frames)[index];
}

void Player::bindMaterial(const Shader& shader, const Material& material) const {
    shader.setVec3("uMaterial.baseColor", material.baseColor);
    shader.setVec3("uMaterial.emissive", material.emissive);
    shader.setFloat("uMaterial.roughness", material.roughness);
    shader.setFloat("uMaterial.checkerStrength", material.checkerStrength);
    shader.setFloat("uMaterial.fogAmount", material.fogAmount);
    shader.setBool("uMaterial.unlit", material.unlit);
    shader.setFloat("uMaterial.opacity", material.opacity);
    shader.setBool("uMaterial.hasTexture", material.texture && material.texture->valid());
    if (material.texture && material.texture->valid()) {
        material.texture->bind(0);
        shader.setInt("uMaterial.albedoMap", 0);
    }
}

bool Player::loadSpriteSequence(const std::string& folderPath, std::vector<SpriteFrame>& frames) {
    frames.clear();

    const std::filesystem::path directory(folderPath);
    if (!std::filesystem::exists(directory) || !std::filesystem::is_directory(directory)) {
        return false;
    }

    std::vector<std::filesystem::path> framePaths;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && lowercase(entry.path().extension().string()) == ".png") {
            framePaths.push_back(entry.path());
        }
    }

    std::sort(framePaths.begin(), framePaths.end(), [](const std::filesystem::path& left, const std::filesystem::path& right) {
        const int leftNumber = trailingNumber(left);
        const int rightNumber = trailingNumber(right);
        if (leftNumber != rightNumber) {
            return leftNumber < rightNumber;
        }
        return left.filename().string() < right.filename().string();
    });

    frames.reserve(framePaths.size());
    for (const std::filesystem::path& framePath : framePaths) {
        auto texture = std::make_shared<Texture2D>();
        if (!texture->loadFromFile(framePath.string(), false)) {
            std::cerr << "Player sprite frame could not be loaded: " << framePath.string() << std::endl;
            frames.clear();
            return false;
        }

        texture->bind();
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        SpriteFrame frame;
        frame.mesh = createWorldOneSpriteMesh(texture->width(), texture->height());
        frame.mirroredMesh = createWorldOneSpriteMesh(texture->width(), texture->height(), true);
        frame.texture = std::move(texture);
        frames.push_back(std::move(frame));
    }

    return !frames.empty();
}

std::shared_ptr<Texture2D> Player::loadTexture(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    const std::string normalized = resolvePath(path).string();
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

std::shared_ptr<Texture2D> Player::loadTexture(const LoadedMaterial& material) {
    auto texture = loadTexture(material.diffuseTexturePath);
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
