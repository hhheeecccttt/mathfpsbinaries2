#ifndef CHARACTER_CLASS
#define CHARACTER_CLASS
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include "t.h"
#include "Rig.h"
#include "Object.h"
#include "Animator.h"

class CharacterBody;
class BodyComponent;

class Action {
public:
    std::string Name;
    std::function<void(CharacterBody*, BodyComponent*)> FunctionPointer;
};

class InputAction {
public:
    std::string BodyComponentName;
    std::string ActionName;
};

class BodyComponent {
public:
    std::string    name;
    BodyComponent* ParentComponent = nullptr;
    std::vector<Action*> Actions;
};

class CharacterBody : public t_package {
public:
    int  Health = 100;
    Rig* rig = nullptr;

    std::vector<Animator*> animators;
    Animator* animator = nullptr;  // back-compat: base animator at weight 0

    std::vector<BodyComponent*> components;
    BodyComponent* RootComponent = nullptr;

    CharacterBody() {
        animator = AddAnimator(0);
        animator->blendWeight = 1.0f;  // base always fully visible
    }

    Animator* AddAnimator(int weight) {
        Animator* a = new Animator();
        a->weight = weight;
        if (rig) a->SetRig(rig);
        animators.push_back(a);
        std::sort(animators.begin(), animators.end(),
            [](Animator* a, Animator* b) { return a->weight < b->weight; });
        return a;
    }

    // Returns weighted blend of all playing animators.
    // Base (weight 0) is always the foundation; higher-weight animators
    // blend in on top based on their blendWeight (0?1).
    std::vector<glm::mat4> GetBlendedBoneMatrices() const {
        // Start from the base animator
        std::vector<glm::mat4> result = animator->GetFinalBoneMatrices();
        const int count = (int)result.size();

        // Blend each higher-priority playing animator on top
        for (int i = 1; i < (int)animators.size(); i++) {
            Animator* a = animators[i];
            if (!a->isPlaying || !a->m_CurrentAnimation || a->blendWeight <= 0.0f) continue;
            const auto& matrices = a->GetFinalBoneMatrices();
            for (int j = 0; j < count && j < (int)matrices.size(); j++)
                result[j] = glm::mix(result[j], matrices[j], a->blendWeight);
        }
        return result;
    }

    // Returns the highest-weight playing animator (for back-compat)
    Animator* GetActiveAnimator() const {
        for (int i = (int)animators.size() - 1; i >= 0; i--)
            if (animators[i]->isPlaying && animators[i]->m_CurrentAnimation)
                return animators[i];
        return animator;
    }

    void SetRig(Rig* r) {
        rig = r;
        for (Animator* a : animators)
            a->SetRig(r);
    }

    void SetAnimation(Animation* anim) {
        animator->SetAnimation(anim);
        animator->PlayAnimation();
        animator->blendWeight = 1.0f;  // base never blends — always instant
    }

    void LoadAnimation(Animation* anim) { SetAnimation(anim); }

    void Step(float dt) {
        for (Animator* a : animators)
            a->Step(dt);
    }

    BodyComponent* GetComponentOfName(std::string name);
    void ExecuteAction(InputAction* action);
};

class CharacterMind {
public:
    InputAction* ProduceAction();
};

class Character : public Object, public Renderable {
public:
    CharacterMind* mind = nullptr;
    CharacterBody* body = nullptr;
    glm::vec4 colorOverride = glm::vec4(0.0f);  // rgb + strength (0 = keep the rig's own texture)
    float    visualScale = 1.4f;  // rig is ~1.8 tall natively; hitbox is 2.5

    Character();
    void Render(Shader& shader) override;

    void Step(float dt) {
        body->Step(dt);
    }
};

#endif