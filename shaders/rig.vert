#version 430 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 norm;
layout(location = 2) in vec2 tex;
layout(location = 3) in ivec4 boneIds; 
layout(location = 4) in vec4 weights;
	
uniform mat4 proj;
uniform mat4 view;
uniform mat4 modl;

uniform mat4 lightSpaceMatrix;

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];
	
out vec3 normal;
out vec2 texCoord;
out vec3 FragPos;
out vec4 fragposlightspace;
	
void main()
{
    vec4 totalPosition = vec4(0.0);
    vec3 totalNormal = vec3(0.0);

    int bonescontributed = 0;
    for(int i = 0; i < MAX_BONE_INFLUENCE; i++)
    {
        if(boneIds[i] == -1) continue;
        if(boneIds[i] >= MAX_BONES)
        {
            totalPosition = vec4(pos, 1.0);
            totalNormal = norm;
            break;
        }
        bonescontributed ++;
        totalPosition += (finalBonesMatrices[boneIds[i]] * vec4(pos, 1.0))* weights[i];
        totalNormal   += mat3(finalBonesMatrices[boneIds[i]]) * norm * weights[i];
    }
    FragPos          = vec3(modl * totalPosition);
    fragposlightspace = lightSpaceMatrix * vec4(FragPos, 1.0);
    gl_Position      = proj * view * modl * totalPosition;
    normal           = normalize(mat3(transpose(inverse(modl))) * totalNormal);
    texCoord         = tex;
}   