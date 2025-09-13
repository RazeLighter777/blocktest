#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "gl_includes.h"
class Camera {
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;
    
    // Euler angles
    float yaw;
    float pitch;
    
    // Camera options
    float movementSpeed;
    float mouseSensitivity;
    float zoom;
    
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), 
           glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), 
           float yaw = -90.0f, 
           float pitch = 0.0f);
    
    glm::mat4 getViewMatrix();
    glm::mat4 getProjectionMatrix(float aspectRatio, float nearPlane = 0.1f, float farPlane = 100.0f);
    
    void processInput(GLFWwindow* window, float deltaTime);
    void processMouseMovement(float xOffset, float yOffset, bool constrainPitch = true);
    void processMouseScroll(float yOffset);
    
private:
    void updateCameraVectors();
};