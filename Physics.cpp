#include "Physics.h"

// ============================================================================
// ComputeInvInertiaTensor
// Returns zero matrix for static or rotation-locked bodies.
// ============================================================================
glm::mat3 Physics::ComputeInvInertiaTensor(const p* body)
{
    if (!body->velocity || body->lockRotation)
        return glm::mat3(0.0f);

    constexpr float k = 1.0f / 12.0f;
    const glm::vec3 s = body->pointer->t.GetScale();
    const glm::vec3 sq = s * s;

    glm::mat3 localInvI(0.0f);
    localInvI[0][0] = 1.0f / (k * body->mass * (sq.y + sq.z));
    localInvI[1][1] = 1.0f / (k * body->mass * (sq.x + sq.z));
    localInvI[2][2] = 1.0f / (k * body->mass * (sq.x + sq.y));

    const glm::mat3 R = glm::mat3(body->pointer->t.GetRotationMatrix());
    return R * localInvI * glm::transpose(R);
}

// ============================================================================
// UpdateSleep
// ============================================================================
bool Physics::UpdateSleep(p* body, float dt)
{
    if (!body->velocity) return true;

    const float linSq = glm::dot(body->linearvelocity, body->linearvelocity);
    const float angSq = glm::dot(body->angularvelocity, body->angularvelocity);

    if (linSq < PHYSICS_SLEEP_LINEAR_SQ && angSq < PHYSICS_SLEEP_ANGULAR_SQ) {
        body->sleepTimer += dt;
        if (body->sleepTimer >= PHYSICS_SLEEP_TIME_THRESHOLD) {
            body->isSleeping = true;
            body->linearvelocity = glm::vec3(0.0f);
            body->angularvelocity = glm::vec3(0.0f);
            return true;
        }
    }
    else {
        body->sleepTimer = 0.0f;
        body->isSleeping = false;
    }
    return false;
}

// ============================================================================
// Step
// ============================================================================
void Physics::Step(float dt)
{
    // Integrate forces ? velocities
    for (p* body : objects) {
        if (!body->velocity || body->isSleeping) continue;

        body->force += body->mass * gravity;

        body->linearvelocity += (body->force / body->mass) * dt;

        if (!body->lockRotation)
            body->angularvelocity += (ComputeInvInertiaTensor(body) * body->torque) * dt;

        // The damping constants are per 1/60 s step; rescale so drag doesn't depend on frame rate
        const float steps = dt * 60.0f;
        body->linearvelocity *= std::pow(PHYSICS_LINEAR_DAMPING, steps);
        body->angularvelocity *= std::pow(PHYSICS_ANGULAR_DAMPING, steps);

        body->force = glm::vec3(0.0f);
        body->torque = glm::vec3(0.0f);
    }

    ResolveCollisions(dt);

    // Integrate velocities ? positions
    for (p* body : objects) {
        if (!body->velocity) continue;
        if (UpdateSleep(body, dt)) continue;

        body->pointer->t.TranslateBy(body->linearvelocity * dt);

        if (!body->lockRotation)
            body->pointer->t.RotateByQuaternion(glm::quat(body->angularvelocity * dt));
    }
}

// ============================================================================
// Resolve
// ============================================================================
void Physics::Resolve(Collision* c)
{
    p* a = c->ObjA;
    p* b = c->ObjB;

    a->WakeUp();
    b->WakeUp();

    const glm::vec3 n = c->CN;
    const glm::vec3 rA = c->POI - a->pointer->t.GetTranslation();
    const glm::vec3 rB = c->POI - b->pointer->t.GetTranslation();

    const glm::mat3 invIA = ComputeInvInertiaTensor(a);
    const glm::mat3 invIB = ComputeInvInertiaTensor(b);

    const float invMassA = a->GetInvMass();
    const float invMassB = b->GetInvMass();

    const glm::vec3 velA = a->linearvelocity + glm::cross(a->angularvelocity, rA);
    const glm::vec3 velB = b->linearvelocity + glm::cross(b->angularvelocity, rB);
    const glm::vec3 relVel = velA - velB;

    const float relVelN = glm::dot(relVel, n);
    if (relVelN > 0.0f) return;

    const float e = (glm::abs(relVelN) < PHYSICS_RESTITUTION_THRESHOLD)
        ? 0.0f
        : glm::min(a->restitution, b->restitution);

    auto angDenom = [](const glm::mat3& invI, const glm::vec3& r, const glm::vec3& d) {
        return glm::dot(glm::cross(invI * glm::cross(r, d), r), d);
        };

    // ---- Normal impulse ----
    const float denomN = invMassA + invMassB + angDenom(invIA, rA, n) + angDenom(invIB, rB, n);
    const float jN = -(1.0f + e) * relVelN / denomN;

    const glm::vec3 impulseN = jN * n;
    if (a->velocity) {
        a->linearvelocity += invMassA * impulseN;
        if (!a->lockRotation)
            a->angularvelocity += invIA * glm::cross(rA, impulseN);
    }
    if (b->velocity) {
        b->linearvelocity -= invMassB * impulseN;
        if (!b->lockRotation)
            b->angularvelocity -= invIB * glm::cross(rB, impulseN);
    }

    // ---- Friction impulse ----
    glm::vec3 tangent = relVel - relVelN * n;
    const float tangLen = glm::length(tangent);
    if (tangLen > 1e-6f) {
        tangent /= tangLen;

        const float denomT = invMassA + invMassB + angDenom(invIA, rA, tangent) + angDenom(invIB, rB, tangent);
        float jT = -glm::dot(relVel, tangent) / denomT;

        const float mu = (a->friction + b->friction) * 0.5f;
        jT = glm::clamp(jT, -mu * jN, mu * jN);

        const glm::vec3 impulseT = jT * tangent;
        if (a->velocity) {
            a->linearvelocity += invMassA * impulseT;
            if (!a->lockRotation)
                a->angularvelocity += invIA * glm::cross(rA, impulseT);
        }
        if (b->velocity) {
            b->linearvelocity -= invMassB * impulseT;
            if (!b->lockRotation)
                b->angularvelocity -= invIB * glm::cross(rB, impulseT);
        }
    }

    // ---- Baumgarte positional correction ----
    const float correction = glm::max(c->overlap - PHYSICS_BAUMGARTE_SLOP, 0.0f)
        / (invMassA + invMassB)
        * PHYSICS_BAUMGARTE_FACTOR;
    const glm::vec3 corrVec = correction * n;
    if (a->velocity) a->pointer->t.TranslateBy(invMassA * corrVec);
    if (b->velocity) b->pointer->t.TranslateBy(-invMassB * corrVec);
}