#ifndef ANIMATOR_CLASS
#define ANIMATOR_CLASS

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <vector>
#include <string>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include "Rig.h"

// Scale of the "Head" bone (and what hangs off it), around the neck. 1 = as authored.
inline float HeadScale = 1.0f;

struct AssimpNodeData
{
    glm::mat4 transformation;
    std::string name;
    int childrenCount;
    std::vector<AssimpNodeData> children;
};

// ============================================================================
// Animation
// ============================================================================
class Animation
{
public:
    Animation() = default;

    Animation(const std::string& animationPath, Rig* rig = nullptr)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(animationPath, aiProcess_Triangulate);
        assert(scene && scene->mRootNode);

        const aiAnimation* anim = scene->mAnimations[0];
        m_Duration       = (float)anim->mDuration;
        m_TicksPerSecond = (int)(anim->mTicksPerSecond != 0 ? anim->mTicksPerSecond : 24);

        ReadHierarchyData(m_RootNode, scene->mRootNode);

        for (unsigned int i = 0; i < anim->mNumChannels; i++) {
            const aiNodeAnim* channel = anim->mChannels[i];
            m_Bones.emplace_back(channel->mNodeName.data, 0, channel);
            m_BoneNames.push_back(channel->mNodeName.data);
        }

        if (rig) RegisterBonesWithRig(*rig);
    }

    void RegisterBonesWithRig(Rig& rig)
    {
        for (const std::string& name : m_BoneNames) {
            if (rig.m_BoneInfoMap.find(name) == rig.m_BoneInfoMap.end()) {
                BoneInfo info;
                info.id     = rig.m_BoneCounter++;
                info.offset = glm::mat4(1.0f);
                rig.m_BoneInfoMap[name] = info;
            }
        }
    }

    Bone* FindBone(const std::string& name)
    {
        for (Bone& b : m_Bones)
            if (b.GetBoneName() == name) return &b;
        return nullptr;
    }

    float                 GetTicksPerSecond() const { return (float)m_TicksPerSecond; }
    float                 GetDuration()       const { return m_Duration; }
    const AssimpNodeData& GetRootNode()       const { return m_RootNode; }

public:
    float                    m_Duration       = 0.0f;
    int                      m_TicksPerSecond = 24;
    std::vector<Bone>        m_Bones;
    std::vector<std::string> m_BoneNames;
    AssimpNodeData           m_RootNode;

private:
    void ReadHierarchyData(AssimpNodeData& dest, const aiNode* src)
    {
        assert(src);
        dest.name           = src->mName.data;
        dest.transformation = AssimpGLMHelpers::ConvertMatrixToGLMFormat(src->mTransformation);
        dest.childrenCount  = (int)src->mNumChildren;
        for (unsigned int i = 0; i < src->mNumChildren; i++) {
            AssimpNodeData child;
            ReadHierarchyData(child, src->mChildren[i]);
            dest.children.push_back(child);
        }
    }
};

// ============================================================================
// Animator
// blendWeight ramps from 0?1 over blendDuration when PlayAnimation is called.
// CharacterBody::GetBlendedBoneMatrices() lerps all animators by blendWeight.
// ============================================================================
class Animator
{
public:
    int   weight        = 0;     // priority — higher wins
    float blendDuration = 0.15f; // seconds to blend in
    float blendWeight   = 1.0f;  // 0 = invisible, 1 = fully playing
    bool  loops         = true;
    bool  isPlaying     = true;

    Animator()
    {
        m_FinalBoneMatrices.assign(100, glm::mat4(1.0f));
    }

    void SetRig(Rig* rig)
    {
        m_Rig = rig;
        if (m_CurrentAnimation) RebuildBoneInfoMap();
    }

    void SetAnimation(Animation* animation)
    {
        m_CurrentAnimation = animation;
        m_CurrentTime      = 0.0f;
        if (m_Rig) RebuildBoneInfoMap();
    }

    void PlayAnimation()
    {
        m_CurrentTime = 0.0f;
        isPlaying     = true;
        blendWeight   = 0.0f;   // start blending in from 0
        _blendTimer   = 0.0f;
    }

    void StopAnimation()  { isPlaying = false; blendWeight = 0.0f; }
    void ClearAnimation() { m_CurrentAnimation = nullptr; m_CurrentTime = 0.0f; isPlaying = false; blendWeight = 0.0f; }

    bool IsFinished() const
    {
        if (!m_CurrentAnimation || !isPlaying) return true;
        return m_CurrentTime >= m_CurrentAnimation->GetDuration() - 1.0f;
    }

    void Step(float dt)
    {
        if (!m_CurrentAnimation || dt <= 0.0f || !isPlaying || !m_Rig) return;

        // Ramp blend weight in
        if (blendWeight < 1.0f) {
            _blendTimer += dt;
            blendWeight  = glm::min(1.0f, blendDuration > 0.0f ? _blendTimer / blendDuration : 1.0f);
        }

        m_CurrentTime += m_CurrentAnimation->GetTicksPerSecond() * dt;

        if (m_CurrentTime >= m_CurrentAnimation->GetDuration()) {
            if (loops) {
                m_CurrentTime = fmod(m_CurrentTime, m_CurrentAnimation->GetDuration());
            } else {
                m_CurrentTime = m_CurrentAnimation->GetDuration();
                isPlaying     = false;
                blendWeight   = 0.0f;
                return;
            }
        }

        CalculateBoneTransform(&m_CurrentAnimation->GetRootNode(), glm::mat4(1.0f));
    }

    const std::vector<glm::mat4>& GetFinalBoneMatrices() const { return m_FinalBoneMatrices; }

public:
    std::vector<glm::mat4> m_FinalBoneMatrices;
    Animation* m_CurrentAnimation = nullptr;
    Rig*       m_Rig              = nullptr;
    float      m_CurrentTime      = 0.0f;
    float      m_DeltaTime        = 0.0f;

private:
    std::map<std::string, BoneInfo> m_BoneInfoMap;
    float _blendTimer = 0.0f;

    void RebuildBoneInfoMap()
    {
        if (!m_CurrentAnimation || !m_Rig) return;
        m_BoneInfoMap = m_Rig->m_BoneInfoMap;
        m_CurrentAnimation->RegisterBonesWithRig(*m_Rig);
        m_BoneInfoMap = m_Rig->m_BoneInfoMap;
    }

    void CalculateBoneTransform(const AssimpNodeData* node, glm::mat4 parentTransform)
    {
        const std::string& nodeName     = node->name;
        glm::mat4          nodeTransform = node->transformation;

        Bone* bone = m_CurrentAnimation->FindBone(nodeName);
        if (bone) {
            bone->Update(m_CurrentTime);
            nodeTransform = bone->GetLocalTransform();
        }

        glm::mat4 globalTransform = parentTransform * nodeTransform;
        if (nodeName == "Head")
            globalTransform = glm::scale(globalTransform, glm::vec3(HeadScale));

        auto it = m_BoneInfoMap.find(nodeName);
        if (it != m_BoneInfoMap.end()) {
            int             index  = it->second.id;
            const glm::mat4 offset = it->second.offset;
            if (index >= 0 && index < (int)m_FinalBoneMatrices.size())
                m_FinalBoneMatrices[index] = globalTransform * offset;
        }

        for (int i = 0; i < node->childrenCount; i++)
            CalculateBoneTransform(&node->children[i], globalTransform);
    }
};

#endif