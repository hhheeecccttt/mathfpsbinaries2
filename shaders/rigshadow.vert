#version 430 core
layout(location = 0) in vec3 pos;
layout(location = 3) in ivec4 boneIds;
layout(location = 4) in vec4 weights;

uniform mat4 modl;
uniform mat4 lightSpaceMatrix;

const int MAX_BONES          = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];

void main()
{
    vec4 totalPosition = vec4(0.0);
    for (int i = 0; i < MAX_BONE_INFLUENCE; i++)
    {
        if (boneIds[i] == -1) continue;
        if (boneIds[i] >= MAX_BONES)
        {
            totalPosition = vec4(pos, 1.0);
            break;
        }
        totalPosition += finalBonesMatrices[boneIds[i]] * vec4(pos, 1.0) * weights[i];
    }
    gl_Position = lightSpaceMatrix * modl * totalPosition;
}