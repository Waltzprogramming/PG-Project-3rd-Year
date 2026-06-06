#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <vector>

enum class MeshType {
    Cube,
    Plane,
    Cylinder,
    Cone,
    Ramp
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 color;
};

class Mesh {
public:
    Mesh() = default;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;
    ~Mesh();

    void upload(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    void draw() const;
    bool valid() const { return m_vao != 0 && m_indexCount > 0; }

    static Mesh cube();
    static Mesh plane();
    static Mesh cylinder(int segments, float height = 1.0f, float radius = 0.5f);
    static Mesh cone(int segments, float height = 1.0f, float radius = 0.5f);
    static Mesh sphere(int segments, int rings, float radius = 0.5f);
    static Mesh ramp();

private:
    void release();

    GLuint m_vao{0};
    GLuint m_vbo{0};
    GLuint m_ebo{0};
    GLsizei m_indexCount{0};
};
