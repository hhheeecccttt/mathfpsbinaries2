#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;
out vec4 FragPosLightSpace;

uniform mat4 proj;
uniform mat4 view;
uniform mat4 modl;
uniform mat4 lightSpaceMatrix;

void main()
{
    vec4 worldPos   = modl * vec4(aPos, 1.0);
    FragPos         = worldPos.xyz;
    Normal          = normalize(mat3(transpose(inverse(modl))) * aNormal);
    TexCoord        = aTexCoord;
    FragPosLightSpace = lightSpaceMatrix * worldPos;
    gl_Position     = proj * view * worldPos;
}
