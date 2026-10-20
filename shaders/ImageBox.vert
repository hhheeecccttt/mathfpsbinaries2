#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

uniform vec2 normalizedCenterPos;
uniform vec2 normalizedCenterScale;

uniform float z;
uniform float rotation;

void main()
{
    gl_Position = vec4(normalizedCenterPos + aPos * normalizedCenterScale,0.0f,1.0f);

    TexCoords = aTexCoords;
}  