#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <string>
#include <unordered_map>

class Shader {
public:
    Shader() = default;
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;
    ~Shader();

    bool load(const std::string& vertexPath, const std::string& fragmentPath);
    void use() const;
    GLuint id() const { return m_program; }

    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec4(const std::string& name, const glm::vec4& value) const;
    void setMat4(const std::string& name, const glm::mat4& value) const;

private:
    static std::string readTextFile(const std::string& path);
    static GLuint compileStage(GLenum stage, const std::string& source, const std::string& label);
    GLint uniformLocation(const std::string& name) const;
    void release();

    GLuint m_program{0};
    mutable std::unordered_map<std::string, GLint> m_uniformLocations;
};
