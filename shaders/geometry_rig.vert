#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in ivec4 boneIds;
layout (location = 4) in vec4 weights;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 FragPosLightSpace;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 modl;
uniform mat4 lightSpaceMatrix;
uniform mat4 finalBonesMatrices[100];

void main()
{
    mat4 boneTransform  = finalBonesMatrices[boneIds[0]] * weights[0];
         boneTransform += finalBonesMatrices[boneIds[1]] * weights[1];
         boneTransform += finalBonesMatrices[boneIds[2]] * weights[2];
         boneTransform += finalBonesMatrices[boneIds[3]] * weights[3];

    vec4 localPos     = boneTransform * vec4(aPos, 1.0);
    vec4 worldPos     = modl * localPos;

    FragPos           = worldPos.xyz;
    Normal            = normalize(mat3(transpose(inverse(modl * boneTransform))) * aNormal);
    TexCoord          = aTexCoord;
    FragPosLightSpace = lightSpaceMatrix * worldPos;
    gl_Position       = proj * view * worldPos;
}