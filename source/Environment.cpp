#include "Environment.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <vector>

namespace {
constexpr int MaxLights = 24;

std::string lowerExtension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return extension;
}

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool containsToken(const std::string& text, const char* token) {
    return text.find(token) != std::string::npos;
}

glm::mat4 transform(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale) {
    glm::mat4 model(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, scale);
    return model;
}

std::filesystem::path resolveTexturePath(const std::string& rawPath, const std::filesystem::path& worldDirectory, const std::filesystem::path& parentWorldDirectory) {
    if (rawPath.empty()) {
        return {};
    }

    const std::filesystem::path original(rawPath);
    const std::filesystem::path fileName = original.filename();
    const std::filesystem::path candidates[] = {
        original,
        worldDirectory / "textures" / fileName,
        worldDirectory / fileName,
        parentWorldDirectory / "textures" / fileName,
        parentWorldDirectory / fileName
    };

    for (const auto& candidate : candidates) {
        if (!candidate.empty() && std::filesystem::exists(candidate)) {
            return std::filesystem::weakly_canonical(candidate);
        }
    }

    return original;
}

void bindMaterial(const Shader& shader, const Material& material) {
    shader.setVec3("uMaterial.baseColor", material.baseColor);
    shader.setVec3("uMaterial.emissive", material.emissive);
    shader.setFloat("uMaterial.roughness", material.roughness);
    shader.setFloat("uMaterial.checkerStrength", material.checkerStrength);
    shader.setFloat("uMaterial.fogAmount", material.fogAmount);
    shader.setFloat("uMaterial.opacity", material.opacity);
    shader.setBool("uMaterial.hasTexture", material.texture && material.texture->valid());
    if (material.texture && material.texture->valid()) {
        material.texture->bind(0);
        shader.setInt("uMaterial.albedoMap", 0);
    }
}
}

void Environment::create(const std::string& levelDirectory, bool loadImportedTextures) {
    m_objects.clear();
    m_levelParts.clear();
    m_lights.clear();
    m_collisionPreview.clear();
    m_loadedTextures.clear();
    m_hasReferenceLevel = false;
    m_levelDirectory = levelDirectory;
    m_loadImportedTextures = loadImportedTextures;
    m_worldMin = glm::vec3(0.0f);
    m_worldMax = glm::vec3(0.0f);
    m_recommendedSpawn = {0.0f, 6.0f, 0.0f};
    createMeshes();
    createMaterials();

    if (!loadReferenceLevel()) {
        std::cerr << "Level model could not be loaded from " << levelDirectory << "." << std::endl;
    }
}

void Environment::render(const Shader& sceneShader, const Shader& lavaShader, float timeSeconds, const glm::vec3& cameraPosition) const {
    const bool marioMap4 = m_levelDirectory.find("mapamian") != std::string::npos ||
        m_levelDirectory.find("mapa verde") != std::string::npos ||
        m_levelDirectory.find("mapa 4") != std::string::npos;
    if (m_hasReferenceLevel) {
        sceneShader.use();
        sceneShader.setFloat("uTime", timeSeconds);
        sceneShader.setMat4("uModel", m_levelTransform);
        for (const ImportedLevelPart& part : m_levelParts) {
            if (marioMap4 && !shouldRenderBounds(part.sourceBounds, cameraPosition, 3.6f)) {
                continue;
            }
            bindMaterial(sceneShader, part.material);
            part.mesh.draw();
        }
    }

    const Shader* activeShader = nullptr;
    for (const SceneObject& object : m_objects) {
        const float padding = object.surface == GameplaySurface::Decoration
            ? (marioMap4 ? 2.1f : 3.5f)
            : (marioMap4 ? 5.0f : 8.0f);
        if (marioMap4 && !shouldRenderBounds(worldBounds(object), cameraPosition, padding)) {
            continue;
        }
        const Shader& shader = object.lava ? lavaShader : sceneShader;
        if (activeShader != &shader) {
            shader.use();
            shader.setFloat("uTime", timeSeconds);
            activeShader = &shader;
        }
        shader.setMat4("uModel", modelMatrix(object));

        if (object.lava) {
            shader.setVec3("uBaseColor", object.material.baseColor);
            shader.setVec3("uEmissiveColor", object.material.emissive);
            if (object.material.texture && object.material.texture->valid()) {
                object.material.texture->bind(0);
                shader.setInt("uLavaMap", 0);
            }
        } else {
            bindMaterial(shader, object.material);
        }

        const Mesh* mesh = nullptr;
        switch (object.mesh) {
        case MeshType::Cube:
            mesh = &m_cube;
            break;
        case MeshType::Plane:
            mesh = &m_plane;
            break;
        case MeshType::Cylinder:
            mesh = &m_cylinder;
            break;
        case MeshType::Cone:
            mesh = &m_cone;
            break;
        case MeshType::Ramp:
            mesh = &m_ramp;
            break;
        }

        if (mesh != nullptr) {
            mesh->draw();
        }
    }
}

void Environment::createMaterials() {
    auto stoneTexture = std::make_shared<Texture2D>();
    stoneTexture->createChecker(128, 128, {0.34f, 0.33f, 0.36f}, {0.23f, 0.23f, 0.27f}, 16);

    auto blockTexture = std::make_shared<Texture2D>();
    blockTexture->createChecker(128, 128, {0.68f, 0.32f, 0.12f}, {0.46f, 0.18f, 0.08f}, 32);

    auto lavaTexture = std::make_shared<Texture2D>();
    lavaTexture->createLava(256, 256);

    m_stone = {{0.46f, 0.45f, 0.49f}, {0.0f, 0.0f, 0.0f}, 0.88f, 0.28f, 1.0f, stoneTexture};
    m_darkStone = {{0.22f, 0.21f, 0.26f}, {0.0f, 0.0f, 0.0f}, 0.94f, 0.18f, 1.2f, stoneTexture};
    m_trim = {{0.12f, 0.12f, 0.15f}, {0.0f, 0.0f, 0.0f}, 0.8f, 0.08f, 1.0f, nullptr};
    m_block = {{0.92f, 0.38f, 0.12f}, {0.0f, 0.0f, 0.0f}, 0.82f, 0.45f, 0.9f, blockTexture};
    m_bridge = {{0.55f, 0.24f, 0.10f}, {0.0f, 0.0f, 0.0f}, 0.82f, 0.12f, 0.95f, nullptr};
    m_lava = {{1.0f, 0.21f, 0.02f}, {3.5f, 1.05f, 0.12f}, 0.35f, 0.0f, 0.0f, lavaTexture};
    m_flame = {{1.0f, 0.62f, 0.10f}, {4.0f, 1.65f, 0.18f}, 0.45f, 0.0f, 0.0f, nullptr};
    m_metal = {{0.08f, 0.07f, 0.07f}, {0.0f, 0.0f, 0.0f}, 0.55f, 0.0f, 1.0f, nullptr};
    m_volcanic = {{0.13f, 0.11f, 0.11f}, {0.03f, 0.005f, 0.0f}, 0.95f, 0.12f, 1.4f, stoneTexture};
}

void Environment::createMeshes() {
    m_cube = Mesh::cube();
    m_plane = Mesh::plane();
    m_cylinder = Mesh::cylinder(24);
    m_cone = Mesh::cone(24);
    m_ramp = Mesh::ramp();
}

void Environment::buildCastle() {
    addObject({"main_lava_sea", MeshType::Plane, m_lava, {0.0f, -1.18f, -16.0f}, {0.0f, 0.0f, 0.0f}, {48.0f, 1.0f, 92.0f}, true, false, GameplaySurface::Hazard});
    addObject({"entry_platform", MeshType::Cube, m_stone, {0.0f, 0.0f, 8.0f}, {0.0f, 0.0f, 0.0f}, {8.0f, 1.0f, 8.0f}});
    addObject({"central_causeway", MeshType::Cube, m_stone, {0.0f, 0.15f, -5.5f}, {0.0f, 0.0f, 0.0f}, {5.2f, 1.0f, 20.0f}});
    addObject({"thin_danger_bridge", MeshType::Cube, m_stone, {0.0f, 0.42f, -21.0f}, {0.0f, 0.0f, 0.0f}, {2.5f, 0.72f, 16.0f}});
    addObject({"boss_gate_platform", MeshType::Cube, m_stone, {0.0f, 0.65f, -35.0f}, {0.0f, 0.0f, 0.0f}, {10.5f, 1.25f, 8.0f}});

    addObject({"left_upper_walkway", MeshType::Cube, m_darkStone, {-9.5f, 3.1f, -10.0f}, {0.0f, 0.0f, 0.0f}, {4.0f, 0.8f, 26.0f}});
    addObject({"right_upper_walkway", MeshType::Cube, m_darkStone, {9.5f, 3.1f, -10.0f}, {0.0f, 0.0f, 0.0f}, {4.0f, 0.8f, 26.0f}});
    addObject({"left_balcony", MeshType::Cube, m_stone, {-12.2f, 3.35f, -27.5f}, {0.0f, 0.0f, 0.0f}, {6.5f, 0.8f, 7.0f}});
    addObject({"right_balcony", MeshType::Cube, m_stone, {12.2f, 3.35f, -27.5f}, {0.0f, 0.0f, 0.0f}, {6.5f, 0.8f, 7.0f}});

    addStaircase({-4.8f, 0.78f, 2.0f}, 9, -1.0f, m_stone, "left_entry_stairs");
    addStaircase({4.8f, 0.78f, 2.0f}, 9, 1.0f, m_stone, "right_entry_stairs");

    addBridge({-7.8f, 2.22f, -0.5f}, 11, 1.9f, m_bridge, "left_suspended_bridge");
    addBridge({7.8f, 2.22f, -0.5f}, 11, 1.9f, m_bridge, "right_suspended_bridge");

    addBlockWall({-18.0f, 1.0f, 12.0f}, 18, 5, {1.8f, 1.0f, 1.0f}, m_darkStone, "back_left_wall");
    addBlockWall({3.0f, 1.0f, 12.0f}, 18, 5, {1.8f, 1.0f, 1.0f}, m_darkStone, "back_right_wall");
    addBlockWall({-20.5f, 1.0f, -40.0f}, 23, 7, {1.8f, 1.0f, 1.0f}, m_darkStone, "far_wall");

    addObject({"left_castle_wall", MeshType::Cube, m_darkStone, {-18.0f, 2.8f, -13.5f}, {0.0f, 0.0f, 0.0f}, {1.4f, 7.5f, 58.0f}, false, false, GameplaySurface::Decoration});
    addObject({"right_castle_wall", MeshType::Cube, m_darkStone, {18.0f, 2.8f, -13.5f}, {0.0f, 0.0f, 0.0f}, {1.4f, 7.5f, 58.0f}, false, false, GameplaySurface::Decoration});
    addObject({"ceiling_arch_left", MeshType::Cube, m_trim, {-7.0f, 7.2f, -16.0f}, {0.0f, 0.0f, 0.0f}, {9.0f, 1.0f, 44.0f}, false, false, GameplaySurface::Decoration});
    addObject({"ceiling_arch_right", MeshType::Cube, m_trim, {7.0f, 7.2f, -16.0f}, {0.0f, 0.0f, 0.0f}, {9.0f, 1.0f, 44.0f}, false, false, GameplaySurface::Decoration});

    for (int i = 0; i < 9; ++i) {
        const float z = 7.0f - static_cast<float>(i) * 6.0f;
        addColumnPair(z, 14.2f, "main_columns_" + std::to_string(i));
    }

    for (int i = 0; i < 13; ++i) {
        const float z = 6.0f - static_cast<float>(i) * 3.25f;
        const float x = (i % 2 == 0) ? -3.9f : 3.9f;
        addObject({"floating_step_" + std::to_string(i), MeshType::Cube, m_block, {x, 1.25f + 0.18f * static_cast<float>(i % 3), z - 12.0f}, {0.0f, 0.0f, 0.0f}, {2.6f, 0.65f, 1.6f}, false, true, GameplaySurface::MovingCandidate});
    }

    for (int i = 0; i < 12; ++i) {
        const float x = -8.25f + static_cast<float>(i % 6) * 3.3f;
        const float z = -31.0f - static_cast<float>(i / 6) * 3.2f;
        addObject({"classic_block_cluster_" + std::to_string(i), MeshType::Cube, m_block, {x, 2.0f + 0.65f * static_cast<float>(i % 2), z}, {0.0f, 0.0f, 0.0f}, {1.1f, 1.1f, 1.1f}});
    }

    for (int i = 0; i < 8; ++i) {
        const float x = (i % 2 == 0) ? -15.5f : 15.5f;
        const float z = 5.5f - static_cast<float>(i) * 6.0f;
        addTorch({x, 2.7f, z}, "wall_torch_" + std::to_string(i));
    }

    for (int i = 0; i < 10; ++i) {
        const float x = -21.0f + static_cast<float>(i % 5) * 10.5f;
        const float z = -44.0f + static_cast<float>(i / 5) * 10.0f;
        addObject({"volcanic_spire_" + std::to_string(i), MeshType::Cone, m_volcanic, {x, -0.15f, z}, {0.0f, static_cast<float>(i) * 17.0f, 0.0f}, {2.8f, 5.5f + static_cast<float>(i % 3), 2.8f}, false, false, GameplaySurface::Decoration});
    }

    addObject({"front_gate_lintel", MeshType::Cube, m_trim, {0.0f, 5.9f, 11.4f}, {0.0f, 0.0f, 0.0f}, {12.0f, 1.1f, 1.4f}, false, false, GameplaySurface::Decoration});
    addObject({"left_gate_tower", MeshType::Cylinder, m_darkStone, {-7.0f, 2.6f, 11.8f}, {0.0f, 0.0f, 0.0f}, {2.3f, 6.4f, 2.3f}, false, false, GameplaySurface::Decoration});
    addObject({"right_gate_tower", MeshType::Cylinder, m_darkStone, {7.0f, 2.6f, 11.8f}, {0.0f, 0.0f, 0.0f}, {2.3f, 6.4f, 2.3f}, false, false, GameplaySurface::Decoration});
    addObject({"far_boss_door", MeshType::Cube, m_metal, {0.0f, 2.7f, -39.35f}, {0.0f, 0.0f, 0.0f}, {5.0f, 4.6f, 0.55f}, false, false, GameplaySurface::Decoration});

    m_lights.push_back({{0.0f, 1.2f, -16.0f}, {1.0f, 0.24f, 0.04f}, 3.4f, 31.0f});
    m_lights.push_back({{-8.0f, 3.0f, -26.0f}, {1.0f, 0.35f, 0.10f}, 1.8f, 18.0f});
    m_lights.push_back({{8.0f, 3.0f, -26.0f}, {1.0f, 0.35f, 0.10f}, 1.8f, 18.0f});
}

bool Environment::loadReferenceLevel() {
    std::vector<std::filesystem::path> candidates;
    const std::filesystem::path sourcePath = std::filesystem::path(m_levelDirectory);
    const std::filesystem::path worldDirectory = std::filesystem::is_regular_file(sourcePath) ? sourcePath.parent_path() : sourcePath;
    const std::filesystem::path parentWorldDirectory = std::filesystem::path("..") / ".." / worldDirectory;
    const std::filesystem::path roots[] = {
        worldDirectory,
        parentWorldDirectory
    };
    const std::string preferredExtensions[] = {
        ".glb",
        ".gltf",
        ".obj",
        ".dae",
        ".fbx"
    };

    if (std::filesystem::exists(sourcePath) && std::filesystem::is_regular_file(sourcePath)) {
        candidates.push_back(sourcePath);
    }

    for (const auto& root : roots) {
        if (!std::filesystem::exists(root)) {
            continue;
        }

        for (const auto& extension : preferredExtensions) {
            for (const auto& entry : std::filesystem::directory_iterator(root)) {
                if (entry.is_regular_file() && lowerExtension(entry.path()) == extension) {
                    candidates.push_back(entry.path());
                }
            }
        }
    }

    const bool rawMapamianPath = m_levelDirectory.find("mapamian") != std::string::npos;
    const bool marioMap4Path =
        rawMapamianPath ||
        m_levelDirectory.find("mapa verde") != std::string::npos ||
        m_levelDirectory.find("mapa 4") != std::string::npos;

    LoadedModel selected;
    int selectedTextureCount = -1;

    if (marioMap4Path) {
        for (const auto& candidate : candidates) {
            if (!std::filesystem::exists(candidate)) {
                continue;
            }

            selected = ModelLoader::loadModel(candidate.string());
            if (!selected.meshes.empty()) {
                selectedTextureCount = static_cast<int>(std::count_if(selected.materials.begin(), selected.materials.end(), [](const LoadedMaterial& material) {
                    return !material.diffuseTexturePath.empty();
                }));
                break;
            }
        }
    } else {
        for (const auto& candidate : candidates) {
            if (!std::filesystem::exists(candidate)) {
                continue;
            }

            LoadedModel model = ModelLoader::loadModel(candidate.string());
            if (model.meshes.empty()) {
                continue;
            }

            const int textureCount = static_cast<int>(std::count_if(model.materials.begin(), model.materials.end(), [](const LoadedMaterial& material) {
                return !material.diffuseTexturePath.empty();
            }));

            if (selected.meshes.empty() || textureCount > selectedTextureCount) {
                selected = std::move(model);
                selectedTextureCount = textureCount;
            }
        }
    }

    if (selected.meshes.empty()) {
        return false;
    }

    const bool rawMapamian = rawMapamianPath;
    const bool marioMap4 = marioMap4Path;

    if (rawMapamian) {
        const std::filesystem::path mainPath = std::filesystem::path(selected.sourcePath);
        const std::filesystem::path rootDirectory = mainPath.parent_path();
        auto appendSiblingModel = [&](const char* siblingName) {
            const std::filesystem::path siblingPath = rootDirectory / siblingName;
            if (!std::filesystem::exists(siblingPath) || siblingPath == mainPath) {
                return;
            }

            LoadedModel sibling = ModelLoader::loadModel(siblingPath.string());
            if (sibling.meshes.empty()) {
                return;
            }

            const unsigned int materialOffset = static_cast<unsigned int>(selected.materials.size());
            selected.materials.insert(selected.materials.end(), sibling.materials.begin(), sibling.materials.end());
            for (LoadedMesh& mesh : sibling.meshes) {
                mesh.materialIndex += materialOffset;
                selected.meshes.push_back(std::move(mesh));
            }
            selected.minBounds = glm::min(selected.minBounds, sibling.minBounds);
            selected.maxBounds = glm::max(selected.maxBounds, sibling.maxBounds);
        };

        appendSiblingModel("CourseSelectWallW1.dae");
        appendSiblingModel("CourseSelectWaveW1.dae");
    }

    m_levelMin = selected.minBounds;
    m_levelMax = selected.maxBounds;
    m_worldMin = glm::vec3(std::numeric_limits<float>::max());
    m_worldMax = glm::vec3(std::numeric_limits<float>::lowest());
    const glm::vec3 center = (selected.minBounds + selected.maxBounds) * 0.5f;
    const float maxExtent = std::max({
        selected.maxBounds.x - selected.minBounds.x,
        selected.maxBounds.y - selected.minBounds.y,
        selected.maxBounds.z - selected.minBounds.z
    });
    const float scale = maxExtent > 0.0f ? 36.0f / maxExtent : 1.0f;
    m_levelTransform = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
    m_levelTransform = m_levelTransform * glm::translate(glm::mat4(1.0f), {-center.x, -selected.minBounds.y, -center.z});

    m_levelParts.reserve(selected.meshes.size());
    float bestSpawnScore = std::numeric_limits<float>::lowest();
    for (LoadedMesh& mesh : selected.meshes) {
        Material material;
        std::string importedMaterialName;
        std::string importedTexturePath;
        if (mesh.materialIndex < selected.materials.size()) {
            const LoadedMaterial& imported = selected.materials[mesh.materialIndex];
            importedMaterialName = imported.name;
            importedTexturePath = imported.diffuseTexturePath;
            material.baseColor = imported.diffuseColor;
            material.opacity = imported.opacity;
            if (m_loadImportedTextures) {
                material.texture = textureForPath(imported.diffuseTexturePath);
                if ((!material.texture || !material.texture->valid()) && !imported.name.empty()) {
                    const std::filesystem::path materialFallback =
                        std::filesystem::path("assets") / "mapa 4" / "textures" / (imported.name + "_baseColor.png");
                    if (std::filesystem::exists(materialFallback)) {
                        auto fallbackTexture = std::make_shared<Texture2D>();
                        if (fallbackTexture->loadFromFile(materialFallback.string(), false)) {
                            material.texture = fallbackTexture;
                            m_loadedTextures.push_back(fallbackTexture);
                        }
                    }
                }
                if ((!material.texture || !material.texture->valid()) && !imported.embeddedTextureData.empty()) {
                    auto embedded = std::make_shared<Texture2D>();
                    if (imported.embeddedTextureCompressed) {
                        if (embedded->loadFromMemory(imported.embeddedTextureData.data(), static_cast<int>(imported.embeddedTextureData.size()), false)) {
                            material.texture = embedded;
                            m_loadedTextures.push_back(embedded);
                        }
                    } else if (imported.embeddedTextureWidth > 0 && imported.embeddedTextureHeight > 0) {
                        embedded->createFromRGBA(imported.embeddedTextureWidth, imported.embeddedTextureHeight, imported.embeddedTextureData.data(), false);
                        material.texture = embedded;
                        m_loadedTextures.push_back(embedded);
                    }
                }
            }
        }
        material.roughness = rawMapamian ? 0.9f : (marioMap4 ? 0.82f : 0.9f);
        material.checkerStrength = 0.0f;
        material.fogAmount = rawMapamian ? 0.42f : (marioMap4 ? 0.18f : 0.42f);

        const std::string surfaceKey = lowerCopy(mesh.name + " " + importedMaterialName + " " + importedTexturePath);
        const bool waterLike = containsToken(surfaceKey, "sea");
        const bool shadowLike = containsToken(surfaceKey, "kage") || containsToken(surfaceKey, "shadow");
        const bool undersideLike = containsToken(surfaceKey, "under");
        const bool excludedFromGameplay = marioMap4 && (waterLike || shadowLike || undersideLike);

        ImportedLevelPart part;
        part.name = mesh.name;
        part.material = material;
        part.sourceBounds.center = (mesh.minBounds + mesh.maxBounds) * 0.5f;
        part.sourceBounds.halfExtent = (mesh.maxBounds - mesh.minBounds) * 0.5f;
        part.mesh = std::move(mesh.mesh);
        if (mesh.collisionBoxes.empty()) {
            const Bounds fallbackBounds = transformedLevelBounds(mesh.minBounds, mesh.maxBounds);
            if (!excludedFromGameplay) {
                m_collisionPreview.push_back(fallbackBounds);
            }
        } else {
            for (const auto& box : mesh.collisionBoxes) {
                const Bounds bounds = transformedLevelBounds(box.minBounds, box.maxBounds);
                if (!excludedFromGameplay) {
                    m_collisionPreview.push_back(bounds);
                }
                m_worldMin = glm::min(m_worldMin, bounds.center - bounds.halfExtent);
                m_worldMax = glm::max(m_worldMax, bounds.center + bounds.halfExtent);

                const float horizontalArea = (bounds.halfExtent.x * 2.0f) * (bounds.halfExtent.z * 2.0f);
                const float top = bounds.center.y + bounds.halfExtent.y;
                const float distanceFromCenter = glm::length(glm::vec2(bounds.center.x, bounds.center.z));
                const bool floorLike = std::abs(box.normal.y) > 0.55f && bounds.halfExtent.y < 0.22f && horizontalArea > 0.20f;
                if (!excludedFromGameplay && floorLike && top > 0.2f) {
                    const float score = horizontalArea - top * 0.22f - distanceFromCenter * 0.035f;
                    if (score > bestSpawnScore) {
                        bestSpawnScore = score;
                        m_recommendedSpawn = {bounds.center.x, top + 0.08f, bounds.center.z};
                    }
                }
            }
        }
        m_levelParts.push_back(std::move(part));
    }

    if (!std::isfinite(m_worldMin.x) || !std::isfinite(bestSpawnScore)) {
        const Bounds levelBounds = transformedLevelBounds(selected.minBounds, selected.maxBounds);
        m_worldMin = levelBounds.center - levelBounds.halfExtent;
        m_worldMax = levelBounds.center + levelBounds.halfExtent;
        m_recommendedSpawn = {levelBounds.center.x, m_worldMin.y + 5.0f, levelBounds.center.z};
    }

    if (!marioMap4) {
        m_lights.push_back({{0.0f, 11.0f, 7.0f}, {0.62f, 0.78f, 1.0f}, 1.6f, 34.0f});
        m_lights.push_back({{-12.0f, 6.5f, -8.0f}, {0.95f, 0.82f, 0.58f}, 1.1f, 18.0f});
        m_lights.push_back({{12.0f, 6.5f, -8.0f}, {0.95f, 0.82f, 0.58f}, 1.1f, 18.0f});
        m_lights.push_back({{0.0f, 5.5f, -18.0f}, {0.42f, 0.66f, 1.0f}, 0.9f, 22.0f});
    }

    m_hasReferenceLevel = true;
    std::cout << "Loaded world: " << selected.sourcePath << std::endl;
    std::cout << "Meshes: " << m_levelParts.size() << ", materials: " << selected.materials.size() << ", textured materials: " << selectedTextureCount << std::endl;
    return true;
}

void Environment::renderReferenceLevel(const Shader&) const {}

std::shared_ptr<Texture2D> Environment::textureForPath(const std::string& path) {
    if (path.empty()) {
        return nullptr;
    }

    const std::filesystem::path sourcePath = std::filesystem::path(m_levelDirectory);
    const std::filesystem::path worldDirectory = std::filesystem::is_regular_file(sourcePath) ? sourcePath.parent_path() : sourcePath;
    const std::filesystem::path parentWorldDirectory = std::filesystem::path("..") / ".." / worldDirectory;
    const std::filesystem::path resolvedPath = resolveTexturePath(path, worldDirectory, parentWorldDirectory);
    const std::string normalizedPath = resolvedPath.string();

    for (const auto& texture : m_loadedTextures) {
        if (texture && texture->sourcePath() == normalizedPath) {
            return texture;
        }
    }

    auto texture = std::make_shared<Texture2D>();
    if (!texture->loadFromFile(normalizedPath, false)) {
        std::cerr << "Could not load level texture: " << path << " resolved as " << normalizedPath << std::endl;
        return nullptr;
    }
    m_loadedTextures.push_back(texture);
    return texture;
}

void Environment::addObject(const SceneObject& object) {
    m_objects.push_back(object);
    if (object.collidable && object.surface != GameplaySurface::Decoration && !object.lava) {
        m_collisionPreview.push_back(worldBounds(object));
    }
}

void Environment::addBlockWall(const glm::vec3& start, int columns, int rows, const glm::vec3& blockSize, const Material& material, const std::string& prefix) {
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < columns; ++x) {
            const float offset = (y % 2 == 0) ? 0.0f : blockSize.x * 0.5f;
            SceneObject block;
            block.name = prefix + "_" + std::to_string(x) + "_" + std::to_string(y);
            block.mesh = MeshType::Cube;
            block.material = material;
            block.position = {start.x + static_cast<float>(x) * blockSize.x + offset, start.y + static_cast<float>(y) * blockSize.y, start.z};
            block.scale = blockSize;
            block.collidable = false;
            block.surface = GameplaySurface::Decoration;
            addObject(block);
        }
    }
}

void Environment::addBridge(const glm::vec3& start, int segments, float spacing, const Material& material, const std::string& prefix) {
    for (int i = 0; i < segments; ++i) {
        const float z = start.z - static_cast<float>(i) * spacing;
        addObject({prefix + "_plank_" + std::to_string(i), MeshType::Cube, material, {start.x, start.y, z}, {0.0f, 0.0f, 0.0f}, {3.7f, 0.38f, 1.05f}});
        addObject({prefix + "_chain_l_" + std::to_string(i), MeshType::Cube, m_metal, {start.x - 2.05f, start.y + 0.55f, z}, {0.0f, 0.0f, 0.0f}, {0.18f, 0.18f, 1.35f}, false, false, GameplaySurface::Decoration});
        addObject({prefix + "_chain_r_" + std::to_string(i), MeshType::Cube, m_metal, {start.x + 2.05f, start.y + 0.55f, z}, {0.0f, 0.0f, 0.0f}, {0.18f, 0.18f, 1.35f}, false, false, GameplaySurface::Decoration});
    }
}

void Environment::addStaircase(const glm::vec3& start, int steps, float direction, const Material& material, const std::string& prefix) {
    for (int i = 0; i < steps; ++i) {
        const float index = static_cast<float>(i);
        addObject({prefix + "_" + std::to_string(i), MeshType::Cube, material, {start.x + direction * index * 0.62f, start.y + index * 0.26f, start.z - index * 0.82f}, {0.0f, 0.0f, 0.0f}, {1.45f, 0.5f, 1.05f}});
    }
}

void Environment::addTorch(const glm::vec3& position, const std::string& name) {
    addObject({name + "_bracket", MeshType::Cube, m_metal, position + glm::vec3(0.0f, -0.35f, 0.0f), {0.0f, 0.0f, 0.0f}, {0.35f, 0.75f, 0.35f}, false, false, GameplaySurface::Decoration});
    addObject({name + "_flame", MeshType::Cone, m_flame, position + glm::vec3(0.0f, 0.28f, 0.0f), {0.0f, 0.0f, 0.0f}, {0.7f, 1.15f, 0.7f}, false, false, GameplaySurface::Decoration});
    m_lights.push_back({position + glm::vec3(0.0f, 0.4f, 0.0f), {1.0f, 0.48f, 0.10f}, 1.9f, 10.0f});
}

void Environment::addColumnPair(float z, float xDistance, const std::string& prefix) {
    addObject({prefix + "_left", MeshType::Cylinder, m_darkStone, {-xDistance, 2.65f, z}, {0.0f, 0.0f, 0.0f}, {1.45f, 6.5f, 1.45f}, false, false, GameplaySurface::Decoration});
    addObject({prefix + "_right", MeshType::Cylinder, m_darkStone, {xDistance, 2.65f, z}, {0.0f, 0.0f, 0.0f}, {1.45f, 6.5f, 1.45f}, false, false, GameplaySurface::Decoration});
    addObject({prefix + "_cap_left", MeshType::Cube, m_trim, {-xDistance, 5.95f, z}, {0.0f, 0.0f, 0.0f}, {2.6f, 0.55f, 2.6f}, false, false, GameplaySurface::Decoration});
    addObject({prefix + "_cap_right", MeshType::Cube, m_trim, {xDistance, 5.95f, z}, {0.0f, 0.0f, 0.0f}, {2.6f, 0.55f, 2.6f}, false, false, GameplaySurface::Decoration});
}

glm::mat4 Environment::modelMatrix(const SceneObject& object) const {
    return transform(object.position, object.rotation, object.scale);
}

Bounds Environment::worldBounds(const SceneObject& object) const {
    Bounds bounds;
    bounds.center = object.position;
    bounds.halfExtent = glm::abs(object.scale) * object.localBounds.halfExtent;
    return bounds;
}

Bounds Environment::transformedLevelBounds(const glm::vec3& minBounds, const glm::vec3& maxBounds) const {
    const glm::vec3 corners[] = {
        {minBounds.x, minBounds.y, minBounds.z},
        {maxBounds.x, minBounds.y, minBounds.z},
        {minBounds.x, maxBounds.y, minBounds.z},
        {maxBounds.x, maxBounds.y, minBounds.z},
        {minBounds.x, minBounds.y, maxBounds.z},
        {maxBounds.x, minBounds.y, maxBounds.z},
        {minBounds.x, maxBounds.y, maxBounds.z},
        {maxBounds.x, maxBounds.y, maxBounds.z}
    };

    glm::vec3 worldMin(std::numeric_limits<float>::max());
    glm::vec3 worldMax(std::numeric_limits<float>::lowest());
    for (const glm::vec3& corner : corners) {
        const glm::vec3 transformed = glm::vec3(m_levelTransform * glm::vec4(corner, 1.0f));
        worldMin = glm::min(worldMin, transformed);
        worldMax = glm::max(worldMax, transformed);
    }

    Bounds bounds;
    bounds.center = (worldMin + worldMax) * 0.5f;
    bounds.halfExtent = (worldMax - worldMin) * 0.5f;
    return bounds;
}

bool Environment::shouldRenderBounds(const Bounds& bounds, const glm::vec3& cameraPosition, float padding) const {
    const glm::vec3 allowed = bounds.halfExtent + glm::vec3(44.0f + padding, 26.0f + padding, 44.0f + padding);
    const glm::vec3 delta = glm::abs(bounds.center - cameraPosition);
    return delta.x <= allowed.x && delta.y <= allowed.y && delta.z <= allowed.z;
}
