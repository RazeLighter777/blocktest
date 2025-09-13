#pragma once

#define GL_GLEXT_PROTOTYPES
#include <glm/glm.hpp>
#include <string>
#include <iostream>
#include "gl_includes.h"
class Shader {
public:
    GLuint programId;
    
    Shader(const std::string& vertexSource, const std::string& fragmentSource);
    ~Shader();
    
    void use();
    void setMat4(const std::string& name, const glm::mat4& matrix);
    void setVec3(const std::string& name, const glm::vec3& vector);
    void setFloat(const std::string& name, float value);
    void setInt(const std::string& name, int value);
    
private:
    GLuint compileShader(const char* source, GLenum type);
    void checkCompileErrors(GLuint shader, const std::string& type);
};