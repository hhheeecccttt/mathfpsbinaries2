#version 330 core
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedo;
layout (location = 3) out vec4 gLightSpacePos;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;
in vec4 FragPosLightSpace;

uniform sampler2D tex;

void main()
{
    vec4 albedo = texture(tex, TexCoord);
    if (albedo.a < 0.1) discard;

    gPosition      = FragPos;
    gNormal        = normalize(Normal);
    gAlbedo        = albedo;
    gLightSpacePos = FragPosLightSpace;
}
