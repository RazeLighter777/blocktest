#include "shader.h"
#include <glm/gtc/type_ptr.hpp>

Shader::Shader(const std::string& vertexSource, const std::string& fragmentSource) {
    GLuint vertex = compileShader(vertexSource.c_str(), GL_VERTEX_SHADER);
    GLuint fragment = compileShader(fragmentSource.c_str(), GL_FRAGMENT_SHADER);
    
    // Create shader program
    programId = glCreateProgram();
    glAttachShader(programId, vertex);
    glAttachShader(programId, fragment);
    glLinkProgram(programId);
    
    // Check for linking errors
    checkCompileErrors(programId, "PROGRAM");
    
    // Clean up shaders
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

Shader::~Shader() {
    glDeleteProgram(programId);
}

void Shader::use() {
    glUseProgram(programId);
}

void Shader::setMat4(const std::string& name, const glm::mat4& matrix) {
    GLint location = glGetUniformLocation(programId, name.c_str());
    glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}

void Shader::setVec3(const std::string& name, const glm::vec3& vector) {
    GLint location = glGetUniformLocation(programId, name.c_str());
    glUniform3fv(location, 1, glm::value_ptr(vector));
}

void Shader::setFloat(const std::string& name, float value) {
    GLint location = glGetUniformLocation(programId, name.c_str());
    glUniform1f(location, value);
}

void Shader::setInt(const std::string& name, int value) {
    GLint location = glGetUniformLocation(programId, name.c_str());
    glUniform1i(location, value);
}

GLuint Shader::compileShader(const char* source, GLenum type) {
    const char* const* src = &source;
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, src, NULL);
    glCompileShader(shader);
    
    checkCompileErrors(shader, type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT");
    return shader;
}

void Shader::checkCompileErrors(GLuint shader, const std::string& type) {
    GLint success;
    GLchar infoLog[1024];
    
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << std::endl;
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << std::endl;
        }
    }
}