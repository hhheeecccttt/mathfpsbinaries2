#version 330 core
out vec4 FragColor;

in vec3 normal;
in vec2 texCoord;
in vec3 FragPos;

uniform sampler2D tex;
uniform vec4 color;

void main()
{
    vec4 base = texture(tex,texCoord) * color;
    if (base.w == 0.0f) {
        discard;
    }
    FragColor = base;
}