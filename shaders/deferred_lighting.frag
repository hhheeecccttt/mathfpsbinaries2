#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D       gPosition;
uniform sampler2D       gNormal;
uniform sampler2D       gAlbedo;
uniform sampler2D       gLightSpacePos;
uniform sampler2DShadow shadowMap;

struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform DirLight dirLight;
uniform vec3     viewPos;

float ShadowCalculation(vec4 fragPosLightSpace)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords      = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;
    return texture(shadowMap, vec3(projCoords.xy, projCoords.z));
}

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec4 fragPosLS, vec3 albedo)
{
    vec3  lightDir   = normalize(-light.direction);
    float diff       = max(dot(normal, lightDir), 0.0);
    vec3  halfwayDir = normalize(lightDir + viewDir);
    float spec       = pow(max(dot(normal, halfwayDir), 0.0), 32.0);

    vec3 ambient  = light.ambient  * albedo;
    vec3 diffuse  = light.diffuse  * diff * albedo;
    vec3 specular = light.specular * spec;

    float shadow = ShadowCalculation(fragPosLS);
    return ambient + (diffuse + specular) * shadow;
}

void main()
{
    vec3 fragPos       = texture(gPosition,      TexCoord).rgb;
    vec3 normal        = normalize(texture(gNormal, TexCoord).rgb);
    vec4 albedaSample  = texture(gAlbedo,        TexCoord);
    vec4 fragPosLS     = texture(gLightSpacePos, TexCoord);

    // Sky (no geometry written) — output clear color
    if (length(normal) < 0.1) {
        FragColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 lit     = CalcDirLight(dirLight, normal, viewDir, fragPosLS, albedaSample.rgb);

    FragColor = vec4(lit, albedaSample.a);
}
