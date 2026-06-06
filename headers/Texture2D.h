#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <string>
#include <vector>

class Texture2D {
public:
    Texture2D() = default;
    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;
    Texture2D(Texture2D&& other) noexcept;
    Texture2D& operator=(Texture2D&& other) noexcept;
    ~Texture2D();

    bool loadFromFile(const std::string& path, bool srgb = false);
    bool loadFromMemory(const unsigned char* bytes, int length, bool srgb = false);
    void createFromRGBA(int width, int height, const unsigned char* pixels, bool srgb = false);
    void createChecker(int width, int height, const glm::vec3& a, const glm::vec3& b, int cellSize);
    void createLava(int width, int height);
    void bind(GLuint unit = 0) const;
    bool valid() const { return m_id != 0; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    const std::string& sourcePath() const { return m_sourcePath; }

private:
    void uploadRGBA(int width, int height, const unsigned char* pixels, bool srgb);
    void release();

    GLuint m_id{0};
    int m_width{0};
    int m_height{0};
    std::string m_sourcePath;
};
