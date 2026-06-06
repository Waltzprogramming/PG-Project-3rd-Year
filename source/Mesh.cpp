#include "Mesh.h"

#include <glm/gtc/constants.hpp>

#include <cmath>
#include <cstddef>
#include <utility>

namespace {
Vertex makeVertex(glm::vec3 position, glm::vec3 normal, glm::vec2 uv, glm::vec4 color = glm::vec4(1.0f)) {
    return {position, normal, uv, color};
}

void addQuad(std::vector<Vertex>& vertices,
             std::vector<unsigned int>& indices,
             glm::vec3 a,
             glm::vec3 b,
             glm::vec3 c,
             glm::vec3 d,
             glm::vec3 normal) {
    const unsigned int start = static_cast<unsigned int>(vertices.size());
    vertices.push_back(makeVertex(a, normal, {0.0f, 0.0f}));
    vertices.push_back(makeVertex(b, normal, {1.0f, 0.0f}));
    vertices.push_back(makeVertex(c, normal, {1.0f, 1.0f}));
    vertices.push_back(makeVertex(d, normal, {0.0f, 1.0f}));
    indices.insert(indices.end(), {start, start + 1, start + 2, start, start + 2, start + 3});
}
}

Mesh::Mesh(Mesh&& other) noexcept {
    *this = std::move(other);
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        release();
        m_vao = other.m_vao;
        m_vbo = other.m_vbo;
        m_ebo = other.m_ebo;
        m_indexCount = other.m_indexCount;
        other.m_vao = 0;
        other.m_vbo = 0;
        other.m_ebo = 0;
        other.m_indexCount = 0;
    }
    return *this;
}

Mesh::~Mesh() {
    release();
}

void Mesh::upload(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
    release();
    m_indexCount = static_cast<GLsizei>(indices.size());

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glGenBuffers(1, &m_ebo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color)));

    glBindVertexArray(0);
}

void Mesh::draw() const {
    if (!valid()) {
        return;
    }
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

Mesh Mesh::cube() {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    const float h = 0.5f;

    addQuad(vertices, indices, {-h, -h, h}, {h, -h, h}, {h, h, h}, {-h, h, h}, {0.0f, 0.0f, 1.0f});
    addQuad(vertices, indices, {h, -h, -h}, {-h, -h, -h}, {-h, h, -h}, {h, h, -h}, {0.0f, 0.0f, -1.0f});
    addQuad(vertices, indices, {-h, -h, -h}, {-h, -h, h}, {-h, h, h}, {-h, h, -h}, {-1.0f, 0.0f, 0.0f});
    addQuad(vertices, indices, {h, -h, h}, {h, -h, -h}, {h, h, -h}, {h, h, h}, {1.0f, 0.0f, 0.0f});
    addQuad(vertices, indices, {-h, h, h}, {h, h, h}, {h, h, -h}, {-h, h, -h}, {0.0f, 1.0f, 0.0f});
    addQuad(vertices, indices, {-h, -h, -h}, {h, -h, -h}, {h, -h, h}, {-h, -h, h}, {0.0f, -1.0f, 0.0f});

    Mesh mesh;
    mesh.upload(vertices, indices);
    return mesh;
}

Mesh Mesh::plane() {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    addQuad(vertices, indices, {-0.5f, 0.0f, 0.5f}, {0.5f, 0.0f, 0.5f}, {0.5f, 0.0f, -0.5f}, {-0.5f, 0.0f, -0.5f}, {0.0f, 1.0f, 0.0f});

    Mesh mesh;
    mesh.upload(vertices, indices);
    return mesh;
}

Mesh Mesh::cylinder(int segments, float height, float radius) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    const float half = height * 0.5f;
    const unsigned int topCenter = 0;
    const unsigned int bottomCenter = 1;
    vertices.push_back(makeVertex({0.0f, half, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 0.5f}));
    vertices.push_back(makeVertex({0.0f, -half, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.5f}));

    for (int i = 0; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float a = t * glm::two_pi<float>();
        const float x = std::cos(a) * radius;
        const float z = std::sin(a) * radius;
        glm::vec3 normal = glm::normalize(glm::vec3(x, 0.0f, z));
        vertices.push_back(makeVertex({x, half, z}, normal, {t, 1.0f}));
        vertices.push_back(makeVertex({x, -half, z}, normal, {t, 0.0f}));
    }

    for (int i = 0; i < segments; ++i) {
        const unsigned int side = 2 + static_cast<unsigned int>(i) * 2;
        indices.insert(indices.end(), {side, side + 1, side + 3, side, side + 3, side + 2});
        indices.insert(indices.end(), {topCenter, side + 2, side});
        indices.insert(indices.end(), {bottomCenter, side + 1, side + 3});
    }

    Mesh mesh;
    mesh.upload(vertices, indices);
    return mesh;
}

Mesh Mesh::cone(int segments, float height, float radius) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    const float half = height * 0.5f;
    const unsigned int tip = 0;
    const unsigned int baseCenter = 1;
    vertices.push_back(makeVertex({0.0f, half, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.5f, 1.0f}));
    vertices.push_back(makeVertex({0.0f, -half, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.5f, 0.5f}));

    for (int i = 0; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float a = t * glm::two_pi<float>();
        const float x = std::cos(a) * radius;
        const float z = std::sin(a) * radius;
        glm::vec3 normal = glm::normalize(glm::vec3(x, radius / height, z));
        vertices.push_back(makeVertex({x, -half, z}, normal, {t, 0.0f}));
    }

    for (int i = 0; i < segments; ++i) {
        const unsigned int base = 2 + static_cast<unsigned int>(i);
        indices.insert(indices.end(), {tip, base, base + 1});
        indices.insert(indices.end(), {baseCenter, base + 1, base});
    }

    Mesh mesh;
    mesh.upload(vertices, indices);
    return mesh;
}

Mesh Mesh::sphere(int segments, int rings, float radius) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    segments = std::max(segments, 8);
    rings = std::max(rings, 4);

    for (int ring = 0; ring <= rings; ++ring) {
        const float v = static_cast<float>(ring) / static_cast<float>(rings);
        const float phi = v * glm::pi<float>();
        const float y = std::cos(phi) * radius;
        const float ringRadius = std::sin(phi) * radius;

        for (int segment = 0; segment <= segments; ++segment) {
            const float u = static_cast<float>(segment) / static_cast<float>(segments);
            const float theta = u * glm::two_pi<float>();
            const float x = std::cos(theta) * ringRadius;
            const float z = std::sin(theta) * ringRadius;
            const glm::vec3 position{x, y, z};
            const glm::vec3 normal = glm::length(position) > 0.0001f
                ? glm::normalize(position)
                : glm::vec3(0.0f, 1.0f, 0.0f);
            vertices.push_back(makeVertex(position, normal, {u, 1.0f - v}));
        }
    }

    const int stride = segments + 1;
    for (int ring = 0; ring < rings; ++ring) {
        for (int segment = 0; segment < segments; ++segment) {
            const unsigned int a = static_cast<unsigned int>(ring * stride + segment);
            const unsigned int b = a + 1;
            const unsigned int c = a + static_cast<unsigned int>(stride);
            const unsigned int d = c + 1;
            indices.insert(indices.end(), {a, c, b, b, c, d});
        }
    }

    Mesh mesh;
    mesh.upload(vertices, indices);
    return mesh;
}

Mesh Mesh::ramp() {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    const glm::vec3 p0{-0.5f, -0.5f, 0.5f};
    const glm::vec3 p1{0.5f, -0.5f, 0.5f};
    const glm::vec3 p2{-0.5f, -0.5f, -0.5f};
    const glm::vec3 p3{0.5f, -0.5f, -0.5f};
    const glm::vec3 p4{-0.5f, 0.5f, -0.5f};
    const glm::vec3 p5{0.5f, 0.5f, -0.5f};

    addQuad(vertices, indices, p0, p1, p5, p4, glm::normalize(glm::cross(p1 - p0, p4 - p0)));
    addQuad(vertices, indices, p2, p3, p1, p0, {0.0f, -1.0f, 0.0f});
    addQuad(vertices, indices, p2, p0, p4, p4, {-1.0f, 0.0f, 0.0f});
    addQuad(vertices, indices, p1, p3, p5, p5, {1.0f, 0.0f, 0.0f});
    addQuad(vertices, indices, p3, p2, p4, p5, {0.0f, 0.0f, -1.0f});

    Mesh mesh;
    mesh.upload(vertices, indices);
    return mesh;
}

void Mesh::release() {
    if (m_ebo != 0) {
        glDeleteBuffers(1, &m_ebo);
    }
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
    }
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
    }
    m_vao = 0;
    m_vbo = 0;
    m_ebo = 0;
    m_indexCount = 0;
}
