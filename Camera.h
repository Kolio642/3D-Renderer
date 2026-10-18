#pragma once

#include "Types.h"

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

class Camera
{
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    float yaw;
    float pitch;
    float moveSpeed = app::CAMERA_SPEED;
    float mouseSens = app::MOUSE_SENSITIVITY;
    float fov = 45.0f;

    Camera(glm::vec3 startPos = glm::vec3(0.0f, 0.5f, 2.5f),
           glm::vec3 startUp = glm::vec3(0.0f, 1.0f, 0.0f),
           float startYaw = -90.0f,
           float startPitch = 0.0f)
        : position(startPos), worldUp(startUp), yaw(startYaw), pitch(startPitch)
    {
        UpdateVectors();
    }

    glm::mat4 GetViewMatrix() const
    {
        return glm::lookAt(position, position + front, up);
    }

    glm::mat4 GetProjectionMatrix(float aspect, float nearPlane = 0.05f, float farPlane = 100.0f) const
    {
        return glm::perspectiveZO(glm::radians(fov), aspect, nearPlane, farPlane);
    }

    void ProcessMouseMovement(float xOffset, float yOffset)
    {
        yaw += xOffset * mouseSens;
        pitch -= yOffset * mouseSens; // Subtraction is needed because y goes to to bottom

        pitch = glm::clamp(pitch, -89.f, 89.f);
        UpdateVectors();
    }

    void ProcessKeyboard(const bool *keys, float deltaTime)
    {
        float velocity = moveSpeed * deltaTime;

        // Sprint with Left Ctrl
        if (keys[SDL_SCANCODE_LCTRL])
            velocity *= 2.5f;

        if (keys[SDL_SCANCODE_W])
            position += front * velocity;
        if (keys[SDL_SCANCODE_S])
            position -= front * velocity;
        if (keys[SDL_SCANCODE_D])
            position += right * velocity;
        if (keys[SDL_SCANCODE_A])
            position -= right * velocity;

        // Fly Up / Down along World Up
        if (keys[SDL_SCANCODE_SPACE])
            position += worldUp * velocity;
        if (keys[SDL_SCANCODE_LSHIFT])
            position -= worldUp * velocity;
    }

private:
    void UpdateVectors()
    {
        glm::vec3 newFront;
        
        newFront.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        newFront.y = std::sin(glm::radians(pitch));
        newFront.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));

        front = glm::normalize(newFront);
        right = glm::normalize(glm::cross(front, worldUp));
        up    = glm::normalize(glm::cross(right, front));
    }
};