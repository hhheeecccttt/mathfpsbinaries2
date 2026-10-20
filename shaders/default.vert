#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTex;	

out vec3 normal;
out vec2 texCoord;
out vec3 FragPos;
out vec4 fragposlightspace;
uniform mat4 modl;	
uniform mat4 view;
uniform mat4 proj;

uniform mat4 lightSpaceMatrix;

void main()
{
    FragPos = vec3(modl * vec4(aPos, 1.0));
	normal = transpose(inverse(mat3(modl))) * aNormal;
    texCoord = aTex;
    fragposlightspace = lightSpaceMatrix * vec4(FragPos, 1.0);
    gl_Position = proj* view * modl * vec4(aPos,1.0f);
}