#version 330 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;

void main()
{    
    float gamma = 2.2;
    FragColor = vec4(pow(texture(skybox, TexCoords).rgb, vec3(gamma)),1.0f);
}