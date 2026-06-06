#include "ModelLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <filesystem>
#include <iostream>
#include <limits>
#include <cstdlib>
#include <unordered_map>

namespace {
glm::vec3 toGlm(const aiVector3D& value) {
    return {value.x, value.y, value.z};
}

glm::vec3 toGlm(const aiColor3D& value) {
    return {value.r, value.g, value.b};
}

glm::vec4 toGlm(const aiColor4D& value) {
    return {value.r, value.g, value.b, value.a};
}

std::string decodePath(std::string path) {
    size_t position = 0;
    while ((position = path.find("%20", position)) != std::string::npos) {
        path.replace(position, 3, " ");
        position += 1;
    }

    constexpr const char* filePrefix = "file://";
    if (path.rfind(filePrefix, 0) == 0) {
        path.erase(0, std::char_traits<char>::length(filePrefix));
    }
    return path;
}

std::string texturePathForMaterial(const aiMaterial* material, const std::filesystem::path& directory) {
    aiString texturePath;
    const aiTextureType types[] = {
        aiTextureType_DIFFUSE,
        aiTextureType_BASE_COLOR,
        aiTextureType_OPACITY
    };

    for (aiTextureType type : types) {
        if (material->GetTextureCount(type) > 0 && material->GetTexture(type, 0, &texturePath) == AI_SUCCESS) {
            const std::string decoded = decodePath(texturePath.C_Str());
            if (!decoded.empty() && decoded[0] == '*') {
                return decoded;
            }

            std::filesystem::path path(decoded);
            if (path.is_relative()) {
                path = directory / path;
            }
            return path.lexically_normal().string();
        }
    }
    return {};
}

void assignEmbeddedTexture(const aiScene* scene, LoadedMaterial& loaded) {
    if (scene == nullptr || loaded.diffuseTexturePath.empty() || loaded.diffuseTexturePath[0] != '*') {
        return;
    }

    const char* begin = loaded.diffuseTexturePath.c_str() + 1;
    char* end = nullptr;
    const long textureIndex = std::strtol(begin, &end, 10);
    if (end == begin || textureIndex < 0 || static_cast<unsigned int>(textureIndex) >= scene->mNumTextures) {
        return;
    }

    const aiTexture* texture = scene->mTextures[textureIndex];
    if (texture == nullptr) {
        return;
    }

    if (texture->mHeight == 0) {
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(texture->pcData);
        loaded.embeddedTextureData.assign(bytes, bytes + texture->mWidth);
        loaded.embeddedTextureCompressed = true;
    } else {
        loaded.embeddedTextureWidth = static_cast<int>(texture->mWidth);
        loaded.embeddedTextureHeight = static_cast<int>(texture->mHeight);
        loaded.embeddedTextureData.resize(static_cast<size_t>(texture->mWidth) * static_cast<size_t>(texture->mHeight) * 4u);
        for (unsigned int i = 0; i < texture->mWidth * texture->mHeight; ++i) {
            const aiTexel& texel = texture->pcData[i];
            const size_t offset = static_cast<size_t>(i) * 4u;
            loaded.embeddedTextureData[offset + 0] = texel.r;
            loaded.embeddedTextureData[offset + 1] = texel.g;
            loaded.embeddedTextureData[offset + 2] = texel.b;
            loaded.embeddedTextureData[offset + 3] = texel.a;
        }
        loaded.embeddedTextureCompressed = false;
    }
}

LoadedMaterial readMaterial(const aiScene* scene, const aiMaterial* material, const std::filesystem::path& directory) {
    LoadedMaterial loaded;

    aiString name;
    if (material->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
        loaded.name = name.C_Str();
    }

    aiColor3D diffuse(1.0f, 1.0f, 1.0f);
    if (material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse) == AI_SUCCESS) {
        loaded.diffuseColor = toGlm(diffuse);
    }

    aiColor4D baseColor(1.0f, 1.0f, 1.0f, 1.0f);
    if (material->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS) {
        loaded.diffuseColor = glm::vec3(baseColor.r, baseColor.g, baseColor.b);
        loaded.opacity = baseColor.a;
    }

    float opacity = 1.0f;
    if (material->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
        loaded.opacity = opacity;
    }

    loaded.diffuseTexturePath = texturePathForMaterial(material, directory);
    assignEmbeddedTexture(scene, loaded);
    if (loaded.opacity <= 0.001f) {
        loaded.opacity = 1.0f;
    }
    return loaded;
}

LoadedMesh buildMesh(const aiMesh* source, const aiMatrix4x4& transform) {
    LoadedMesh loaded;
    loaded.name = source->mName.C_Str();
    loaded.materialIndex = source->mMaterialIndex;
    loaded.minBounds = glm::vec3(std::numeric_limits<float>::max());
    loaded.maxBounds = glm::vec3(std::numeric_limits<float>::lowest());

    aiMatrix3x3 normalTransform(transform);
    normalTransform.Inverse();
    normalTransform.Transpose();

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    vertices.reserve(source->mNumVertices);

    for (unsigned int i = 0; i < source->mNumVertices; ++i) {
        const aiVector3D transformedPosition = transform * source->mVertices[i];
        Vertex vertex{};
        vertex.position = toGlm(transformedPosition);

        if (source->HasNormals()) {
            aiVector3D normal = normalTransform * source->mNormals[i];
            normal.Normalize();
            vertex.normal = toGlm(normal);
        } else {
            vertex.normal = {0.0f, 1.0f, 0.0f};
        }

        if (source->HasTextureCoords(0)) {
            vertex.uv = {source->mTextureCoords[0][i].x, source->mTextureCoords[0][i].y};
        } else {
            vertex.uv = {0.0f, 0.0f};
        }

        if (source->HasVertexColors(0)) {
            vertex.color = toGlm(source->mColors[0][i]);
        } else {
            vertex.color = {1.0f, 1.0f, 1.0f, 1.0f};
        }

        loaded.minBounds = glm::min(loaded.minBounds, vertex.position);
        loaded.maxBounds = glm::max(loaded.maxBounds, vertex.position);
        vertices.push_back(vertex);
    }

    for (unsigned int faceIndex = 0; faceIndex < source->mNumFaces; ++faceIndex) {
        const aiFace& face = source->mFaces[faceIndex];
        if (face.mNumIndices != 3) {
            continue;
        }
        indices.push_back(face.mIndices[0]);
        indices.push_back(face.mIndices[1]);
        indices.push_back(face.mIndices[2]);

        const glm::vec3& a = vertices[face.mIndices[0]].position;
        const glm::vec3& b = vertices[face.mIndices[1]].position;
        const glm::vec3& c = vertices[face.mIndices[2]].position;
        const glm::vec3 edgeA = b - a;
        const glm::vec3 edgeB = c - a;
        glm::vec3 normal = glm::cross(edgeA, edgeB);
        if (glm::dot(normal, normal) > 0.000001f) {
            normal = glm::normalize(normal);
        } else {
            normal = {0.0f, 1.0f, 0.0f};
        }

        LoadedMesh::CollisionBox box;
        box.minBounds = glm::min(glm::min(a, b), c);
        box.maxBounds = glm::max(glm::max(a, b), c);
        box.normal = normal;
        box.a = a;
        box.b = b;
        box.c = c;

        const glm::vec3 minSize(0.035f, 0.055f, 0.035f);
        for (int axis = 0; axis < 3; ++axis) {
            const float size = box.maxBounds[axis] - box.minBounds[axis];
            if (size < minSize[axis]) {
                const float expand = (minSize[axis] - size) * 0.5f;
                box.minBounds[axis] -= expand;
                box.maxBounds[axis] += expand;
            }
        }
        loaded.collisionBoxes.push_back(box);
    }

    loaded.mesh.upload(vertices, indices);
    return loaded;
}

void processNode(const aiScene* scene, const aiNode* node, const aiMatrix4x4& parentTransform, LoadedModel& model) {
    const aiMatrix4x4 transform = parentTransform * node->mTransformation;

    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* source = scene->mMeshes[node->mMeshes[i]];
        LoadedMesh mesh = buildMesh(source, transform);
        model.minBounds = glm::min(model.minBounds, mesh.minBounds);
        model.maxBounds = glm::max(model.maxBounds, mesh.maxBounds);
        model.meshes.push_back(std::move(mesh));
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        processNode(scene, node->mChildren[i], transform, model);
    }
}
}

LoadedModel ModelLoader::loadModel(const std::string& path) {
    LoadedModel model;
    model.sourcePath = path;
    model.minBounds = glm::vec3(std::numeric_limits<float>::max());
    model.maxBounds = glm::vec3(std::numeric_limits<float>::lowest());

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate |
            aiProcess_GenNormals |
            aiProcess_JoinIdenticalVertices |
            aiProcess_ImproveCacheLocality |
            aiProcess_RemoveRedundantMaterials |
            aiProcess_FindInvalidData |
            aiProcess_CalcTangentSpace);

    if (scene == nullptr || scene->mRootNode == nullptr) {
        std::cerr << "Assimp could not load model: " << path << " - " << importer.GetErrorString() << std::endl;
        return model;
    }

    const std::filesystem::path directory = std::filesystem::path(path).parent_path();
    model.materials.reserve(scene->mNumMaterials);
    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        model.materials.push_back(readMaterial(scene, scene->mMaterials[i], directory));
    }

    model.meshes.reserve(scene->mNumMeshes);
    processNode(scene, scene->mRootNode, aiMatrix4x4(), model);

    if (model.meshes.empty()) {
        model.minBounds = glm::vec3(0.0f);
        model.maxBounds = glm::vec3(0.0f);
    }

    return model;
}
