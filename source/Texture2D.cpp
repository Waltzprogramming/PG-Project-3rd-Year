#include "Texture2D.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
unsigned char toByte(float value) {
    return static_cast<unsigned char>(std::clamp(value, 0.0f, 1.0f) * 255.0f);
}
}

Texture2D::Texture2D(Texture2D&& other) noexcept {
    *this = std::move(other);
}

Texture2D& Texture2D::operator=(Texture2D&& other) noexcept {
    if (this != &other) {
        release();
        m_id = other.m_id;
        m_width = other.m_width;
        m_height = other.m_height;
        m_sourcePath = std::move(other.m_sourcePath);
        other.m_id = 0;
        other.m_width = 0;
        other.m_height = 0;
        other.m_sourcePath.clear();
    }
    return *this;
}

Texture2D::~Texture2D() {
    release();
}

bool Texture2D::loadFromFile(const std::string& path, bool srgb) {
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        return false;
    }
    uploadRGBA(width, height, pixels, srgb);
    m_sourcePath = path;
    stbi_image_free(pixels);
    return true;
}

bool Texture2D::loadFromMemory(const unsigned char* bytes, int length, bool srgb) {
    if (bytes == nullptr || length <= 0) {
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* pixels = stbi_load_from_memory(bytes, length, &width, &height, &channels, 4);
    if (!pixels) {
        return false;
    }
    uploadRGBA(width, height, pixels, srgb);
    m_sourcePath.clear();
    stbi_image_free(pixels);
    return true;
}

void Texture2D::createFromRGBA(int width, int height, const unsigned char* pixels, bool srgb) {
    uploadRGBA(width, height, pixels, srgb);
    m_sourcePath.clear();
}

void Texture2D::createChecker(int width, int height, const glm::vec3& a, const glm::vec3& b, int cellSize) {
    std::vector<unsigned char> pixels(static_cast<size_t>(width * height * 4));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool choose = ((x / cellSize) + (y / cellSize)) % 2 == 0;
            const glm::vec3 color = choose ? a : b;
            const size_t offset = static_cast<size_t>((y * width + x) * 4);
            pixels[offset + 0] = toByte(color.r);
            pixels[offset + 1] = toByte(color.g);
            pixels[offset + 2] = toByte(color.b);
            pixels[offset + 3] = 255;
        }
    }
    uploadRGBA(width, height, pixels.data(), false);
    m_sourcePath.clear();
}

void Texture2D::createLava(int width, int height) {
    std::vector<unsigned char> pixels(static_cast<size_t>(width * height * 4));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float fx = static_cast<float>(x) / static_cast<float>(width);
            const float fy = static_cast<float>(y) / static_cast<float>(height);
            const float wave = 0.5f + 0.5f * std::sin(fx * 32.0f + std::sin(fy * 19.0f) * 2.0f);
            const float vein = std::pow(0.5f + 0.5f * std::sin((fx + fy) * 42.0f), 6.0f);
            glm::vec3 color = glm::mix(glm::vec3(0.45f, 0.04f, 0.0f), glm::vec3(1.0f, 0.28f, 0.02f), wave);
            color = glm::mix(color, glm::vec3(1.0f, 0.88f, 0.18f), vein);
            const size_t offset = static_cast<size_t>((y * width + x) * 4);
            pixels[offset + 0] = toByte(color.r);
            pixels[offset + 1] = toByte(color.g);
            pixels[offset + 2] = toByte(color.b);
            pixels[offset + 3] = 255;
        }
    }
    uploadRGBA(width, height, pixels.data(), false);
    m_sourcePath.clear();
}

void Texture2D::bind(GLuint unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, m_id);
}

void Texture2D::uploadRGBA(int width, int height, const unsigned char* pixels, bool srgb) {
    release();
    m_width = width;
    m_height = height;
    glGenTextures(1, &m_id);
    glBindTexture(GL_TEXTURE_2D, m_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture2D::release() {
    if (m_id != 0) {
        glDeleteTextures(1, &m_id);
        m_id = 0;
    }
    m_width = 0;
    m_height = 0;
    m_sourcePath.clear();
}
