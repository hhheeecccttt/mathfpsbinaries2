#ifndef CAMERA_CLASS_H
#define CAMERA_CLASS_H
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdlib>
#include "common.h"
#include "Window.h"
#include "t.h"
#include "tFunctions.h"
#include "shaderClass.h"

const glm::vec3 worldUp = { 0.0f, 1.0f, 0.0f };

// Mouse sensitivity asked at startup (see main.cpp). 0 = keep the camera's built-in default.
inline float StartupCmPer360 = 0.0f;
inline float StartupMouseDpi = 800.0f;

class Camera {
public:
    bool  lockedcursor = true;
    t     t;
    float yaw = 0.0f;
    float pitch = 0.0f;
    const float originalspeed = 0.08f;
    float speed = 0.08f;
    float sensitivity = 0.001f;
    float FOV = 110.0f;
    float screenshake = 0.0f;

    Camera() {
        if (StartupCmPer360 > 0.0f) SetSensitivityCm(StartupCmPer360, StartupMouseDpi);
    }

    // Turn sensitivity into "this many cm of mouse travel per full 360 degree turn" for a mouse
    // of the given DPI (mouse counts per inch).
    void SetSensitivityCm(float cmPer360, float dpi) {
        sensitivity = 6.2831853f / (cmPer360 / 2.54f * dpi);
    }

    void AddScreenshake(float amount) {
        screenshake += amount;
        float s = screenshake;
        shakeTarget = glm::vec3(
            ((float)(rand() % 2000 - 1000) / 1000.0f) * s,
            ((float)(rand() % 2000 - 1000) / 1000.0f) * s,
            ((float)(rand() % 2000 - 1000) / 1000.0f) * s * 0.5f
        );
        shakeTimer = 0.05f;
    }

    void Matrix(float nearPlane, float farPlane, Shader& shader) {
        shader.SetMat4("proj", GetProjectionMatrix(nearPlane, farPlane));
        shader.SetMat4("view", GetViewMatrix());
        shader.Set3F("viewPos", t.GetTranslation());
    }

    glm::mat4 GetViewMatrix() {
        glm::vec3 front = t.GetFrontVector();
        glm::vec3 up = t.GetUpVector();

        if (screenshake > 0.0f) {
            // Apply smooth shake offset stored in shakeOffset
            glm::quat offsetRot = glm::quat(glm::vec3(
                shakeOffset.x,
                shakeOffset.y,
                shakeOffset.z   // roll
            ));
            front = glm::normalize(offsetRot * front);
            up = glm::normalize(offsetRot * up);
        }

        return glm::lookAt(t.GetTranslation(), t.GetTranslation() + front, up);
    }

    glm::mat4 GetProjectionMatrix(float nearPlane, float farPlane) {
        // Quake Pro style: FOV is the horizontal angle on a 4:3 screen, so the vertical angle is fixed
        // and wider windows/fullscreen reveal more to the sides instead of zooming in.
        const float aspect = (float)window->width / (float)window->height;
        const float vfov = 2.0f * atan(tan(glm::radians(FOV) * 0.5f) * 3.0f / 4.0f);
        return glm::perspective(vfov, aspect, nearPlane, farPlane);
    }

    void Step(float dt) {
        if (screenshake <= 0.0f) {
            shakeOffset = glm::vec3(0.0f);
            shakeTarget = glm::vec3(0.0f);
            return;
        }

        // Pick a new random target periodically, but fade target to zero as shake ends
        shakeTimer -= dt;
        if (shakeTimer <= 0.0f) {
            float s = screenshake;
            shakeTarget = glm::vec3(
                ((float)(rand() % 2000 - 1000) / 1000.0f) * s,
                ((float)(rand() % 2000 - 1000) / 1000.0f) * s,
                ((float)(rand() % 2000 - 1000) / 1000.0f) * s * 0.5f
            );
            shakeTimer = 0.05f;
        }

        // As shake decays below threshold, pull target back to zero so we land clean
        if (screenshake < 0.2f)
            shakeTarget = glm::vec3(0.0f);

        // Smoothly interpolate current offset toward target
        float smoothSpeed = 20.0f;
        shakeOffset = glm::mix(shakeOffset, shakeTarget, glm::min(1.0f, smoothSpeed * dt));

        // Decay
        screenshake -= dt * 3.0f;
        if (screenshake < 0.0f) screenshake = 0.0f;
    }

    void ProcessMouseMovement(float xoffset, float yoffset) {
        xoffset *= sensitivity;
        yoffset *= sensitivity;
        yaw += xoffset;
        pitch += yoffset;
        pitch = glm::clamp(pitch, -glm::radians(89.0f), glm::radians(89.0f));
        t.RotateToEulerAngles({ pitch, yaw, 0.0f });
        t.NormalizeRotation();
    }

private:
    glm::vec3 shakeOffset = glm::vec3(0.0f);  // current smoothed offset (pitch, yaw, roll)
    glm::vec3 shakeTarget = glm::vec3(0.0f);  // target we're interpolating toward
    float     shakeTimer = 0.0f;             // time until next target pick
};

#endif