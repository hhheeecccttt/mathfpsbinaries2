#include "Character.h"
#include "Animator.h"

// -------------------------------------------------------
// BodyComponent
// -------------------------------------------------------

BodyComponent* CharacterBody::GetComponentOfName(std::string name) {
    for (BodyComponent* C : components) {
        if (C->name == name)
            return C;
    }
    return nullptr;
}

void CharacterBody::ExecuteAction(InputAction* action) {
    for (BodyComponent* C : components) {
        if (C->name == action->BodyComponentName) {
            for (Action* a : C->Actions) {
                if (a->Name == action->ActionName) {
                    a->FunctionPointer(this, C);
                    break;
                }
            }
            break;
        }
    }
}

// -------------------------------------------------------
// CharacterMind
// -------------------------------------------------------

InputAction* CharacterMind::ProduceAction() {
    // TODO: implement
    return nullptr;
}

// -------------------------------------------------------
// Character
// -------------------------------------------------------

Character::Character() {
    body = new CharacterBody();
    mind = nullptr;
    shadertype = ShaderType::RigShader;
}

void Character::Render(Shader& shader) {
    if (!body || !body->rig || !body->animator) return;
    if (body->animator->m_CurrentAnimation == nullptr) {
        for (int i = 0; i < 100; i++)
            shader.SetMat4(("finalBonesMatrices[" + std::to_string(i) + "]").c_str(), glm::mat4(1.0f));
    }
    else {
        std::vector<glm::mat4> transforms = body->GetBlendedBoneMatrices();

        for (int i = 0; i < transforms.size(); i++)
            shader.SetMat4(("finalBonesMatrices[" + std::to_string(i) + "]").c_str(), transforms[i]);
    }
    shader.Set4F("colorOverride", colorOverride);
    // body->t's scale is the hitbox only; the rig is drawn at visualScale so it fills that hitbox
    body->rig->Render(shader, body->t.GetTranslationMatrix() * body->t.GetRotationMatrix()
        * glm::scale(glm::mat4(1.0f), glm::vec3(visualScale)));
    shader.Set4F("colorOverride", glm::vec4(0.0f));  // don't leak into other rig draws
}