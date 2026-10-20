#ifndef PHYSICS_CLASS
#define PHYSICS_CLASS
#include <iostream>
#include <random>
#include <vector>
#include <memory>
#include <cmath>
#include <optional>
#include <algorithm>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "common.h"
#include "Engine.h"
#include "Camera.h"
#include "t.h"
#include "tFunctions.h"

// ============================================================================
// Tuneable constants
// ============================================================================
static constexpr float PHYSICS_LINEAR_DAMPING = 0.995f;
static constexpr float PHYSICS_ANGULAR_DAMPING = 0.990f;
static constexpr float PHYSICS_BAUMGARTE_FACTOR = 0.1f;
static constexpr float PHYSICS_BAUMGARTE_SLOP = 0.02f;
static constexpr float PHYSICS_SLEEP_LINEAR_SQ = 0.01f * 0.01f;
static constexpr float PHYSICS_SLEEP_ANGULAR_SQ = 0.01f * 0.01f;
static constexpr float PHYSICS_SLEEP_TIME_THRESHOLD = 0.5f;
static constexpr float PHYSICS_RESTITUTION_THRESHOLD = 1.0f;

// ============================================================================
// Physics body
// ============================================================================
class p {
    friend class Physics;
public:
    p() {
        pointer = nullptr;
        std::cout << "PHYSICS OBJECT INITIALIZED WITHOUT POINTER IN CONSTRUCTOR!!!\n";
    }
    p(t_package* t) : pointer(t) {}

    void SetLinearVelocity(glm::vec3 nf) { linearvelocity = nf; }
    void SetMass(float newmass) { mass = newmass; }

    void EnableCollision() { collision = true; }
    void DisableCollision() { collision = false; }
    void EnableVelocity() { velocity = true; }
    void DisableVelocity() { velocity = false; }

    void AddForce(const glm::vec3& f) { if (velocity) { force += f; WakeUp(); } }
    void AddTorque(const glm::vec3& t) { if (velocity) { torque += t; WakeUp(); } }

    float GetMass()    const noexcept { return mass; }
    float GetInvMass() const noexcept { return velocity ? 1.0f / mass : 0.0f; }

    void WakeUp() { isSleeping = false; sleepTimer = 0.0f; }

public:
    t_package* pointer = nullptr;

    bool collision = true;
    bool velocity = true;
    bool isSleeping = false;
    bool lockRotation = false;

    float mass = 1.0f;
    float restitution = 0.2f;
    float friction = 0.5f;

    glm::vec3 linearvelocity = { 0.0f, 0.0f, 0.0f };
    glm::vec3 angularvelocity = { 0.0f, 0.0f, 0.0f };
    glm::vec3 force = { 0.0f, 0.0f, 0.0f };
    glm::vec3 torque = { 0.0f, 0.0f, 0.0f };

    float sleepTimer = 0.0f;
};

// ============================================================================
// Collision manifold
// ============================================================================
struct Collision {
    glm::vec3 POI;
    glm::vec3 CN;
    float overlap = 0.0f;
    p* ObjA = nullptr;
    p* ObjB = nullptr;

    Collision(p* a, p* b) : ObjA(a), ObjB(b) {}
};

// ============================================================================
// Physics world
// ============================================================================
class Physics {
private:
    std::vector<p*> objects;
    glm::vec3 gravity = glm::vec3(0.0f, -9.81f, 0.0f);

    static glm::mat3 ComputeInvInertiaTensor(const p* body);
    static bool      UpdateSleep(p* body, float dt);
    void             Resolve(Collision* c);

public:
    Physics() {}

    const glm::vec3& GetGravity() const noexcept { return gravity; }
    void SetGravity(const glm::vec3& g) noexcept { gravity = g; }

    int  GetObjectAmount() const noexcept { return (int)objects.size(); }
    void AddObject(p* obj) { objects.push_back(obj); }
    const std::vector<p*>& GetObjects() const { return objects; }

    p* PPointerOfObject(t_package* pointer) const {
        for (p* obj : objects)
            if (obj->pointer == pointer) return obj;
        return nullptr;
    }

    void RemoveP(p* obj) {
        objects.erase(std::find(objects.begin(), objects.end(), obj));
    }

    void RemoveObject(t_package* pointer) {
        for (int i = 0; i < (int)objects.size(); i++) {
            if (objects[i]->pointer == pointer) {
                delete objects[i];
                objects.erase(objects.begin() + i);
                return;
            }
        }
    }

    void Step(float dt);

    void SolveCollisions(std::vector<Collision>& collisions) {
        for (Collision& col : collisions)
            Resolve(&col);
    }

    void ResolveCollisions(float dt) {
        std::vector<Collision> collisions;

        for (p* a : objects) {
            if (!a->collision) continue;
            for (p* b : objects) {
                if (b <= a || !b->collision)              continue;
                if (!a->velocity && !b->velocity)         continue;
                if (a->isSleeping && b->isSleeping)       continue;

                if (TNearT(a->pointer->t, b->pointer->t) &&
                    BoundingBoxInBoundingBox(a->pointer->t.GetAABB(), b->pointer->t.GetAABB()))
                {
                    std::optional<TInTInfo> e = TInT(a->pointer->t, b->pointer->t);
                    if (e.has_value()) {
                        Collision tp(a, b);
                        tp.POI = e.value().POI;
                        tp.CN = e.value().CN;
                        tp.overlap = e.value().overlap;
                        collisions.push_back(tp);
                    }
                }
            }
        }
        SolveCollisions(collisions);
    }
};

#endif