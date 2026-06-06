#include "Shader.h"

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath) {
    load(vertexPath, fragmentPath);
}

Shader::Shader(Shader&& other) noexcept {
    *this = std::move(other);
}

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        release();
        m_program = other.m_program;
        m_uniformLocations = std::move(other.m_uniformLocations);
        other.m_program = 0;
        other.m_uniformLocations.clear();
    }
    return *this;
}

Shader::~Shader() {
    release();
}

bool Shader::load(const std::string& vertexPath, const std::string& fragmentPath) {
    release();
    const std::string vertexSource = readTextFile(vertexPath);
    const std::string fragmentSource = readTextFile(fragmentPath);
    if (vertexSource.empty() || fragmentSource.empty()) {
        return false;
    }

    const GLuint vertex = compileStage(GL_VERTEX_SHADER, vertexSource, vertexPath);
    const GLuint fragment = compileStage(GL_FRAGMENT_SHADER, fragmentSource, fragmentPath);
    if (vertex == 0 || fragment == 0) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return false;
    }

    m_program = glCreateProgram();
    glAttachShader(m_program, vertex);
    glAttachShader(m_program, fragment);
    glLinkProgram(m_program);

    GLint linked = GL_FALSE;
    glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        GLint length = 0;
        glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<size_t>(length), '\0');
        glGetProgramInfoLog(m_program, length, nullptr, log.data());
        std::cerr << "Shader link failed: " << log << std::endl;
        release();
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return m_program != 0;
}

void Shader::use() const {
    glUseProgram(m_program);
}

void Shader::setBool(const std::string& name, bool value) const {
    glUniform1i(uniformLocation(name), value ? 1 : 0);
}

void Shader::setInt(const std::string& name, int value) const {
    glUniform1i(uniformLocation(name), value);
}

void Shader::setFloat(const std::string& name, float value) const {
    glUniform1f(uniformLocation(name), value);
}

void Shader::setVec3(const std::string& name, const glm::vec3& value) const {
    glUniform3fv(uniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setVec4(const std::string& name, const glm::vec4& value) const {
    glUniform4fv(uniformLocation(name), 1, glm::value_ptr(value));
}

void Shader::setMat4(const std::string& name, const glm::mat4& value) const {
    glUniformMatrix4fv(uniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

std::string Shader::readTextFile(const std::string& path) {
    std::ifstream file(path, std::ios::in);
    if (!file) {
        std::cerr << "Could not open shader file: " << path << std::endl;
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint Shader::compileStage(GLenum stage, const std::string& source, const std::string& label) {
    const GLuint shader = glCreateShader(stage);
    const char* raw = source.c_str();
    glShaderSource(shader, 1, &raw, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<size_t>(length), '\0');
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        std::cerr << "Shader compile failed (" << label << "): " << log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLint Shader::uniformLocation(const std::string& name) const {
    const auto found = m_uniformLocations.find(name);
    if (found != m_uniformLocations.end()) {
        return found->second;
    }

    const GLint location = glGetUniformLocation(m_program, name.c_str());
    m_uniformLocations.emplace(name, location);
    return location;
}

void Shader::release() {
    if (m_program != 0) {
        glDeleteProgram(m_program);
        m_program = 0;
    }
    m_uniformLocations.clear();
}
