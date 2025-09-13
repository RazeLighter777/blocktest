#include "camera.h"
#include <algorithm>

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : position(position), worldUp(up), yaw(yaw), pitch(pitch),
      movementSpeed(2.5f), mouseSensitivity(0.1f), zoom(45.0f) {
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() {
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio, float nearPlane, float farPlane) {
    return glm::perspective(glm::radians(zoom), aspectRatio, nearPlane, farPlane);
}

void Camera::processInput(GLFWwindow* window, float deltaTime) {
    float velocity = movementSpeed * deltaTime;
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        position += front * velocity;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        position -= front * velocity;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        position -= right * velocity;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        position += right * velocity;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        position += up * velocity;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        position -= up * velocity;
}

void Camera::processMouseMovement(float xOffset, float yOffset, bool constrainPitch) {
    xOffset *= mouseSensitivity;
    yOffset *= mouseSensitivity;
    
    yaw += xOffset;
    pitch += yOffset;
    
    if (constrainPitch) {
        pitch = std::clamp(pitch, -89.0f, 89.0f);
    }
    
    updateCameraVectors();
}

void Camera::processMouseScroll(float yOffset) {
    zoom -= yOffset;
    zoom = std::clamp(zoom, 1.0f, 45.0f);
}

void Camera::updateCameraVectors() {
    // Calculate the new front vector
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);
    
    // Calculate right and up vectors
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}