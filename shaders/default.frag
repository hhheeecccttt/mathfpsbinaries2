#version 330 core
out vec4 FragColor;

in vec3 normal;
in vec2 texCoord;
in vec3 FragPos;
in vec4 fragposlightspace;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;    
    float shininess;
};

struct PointLight {    
    vec3 position;
    
    float constant;
    float linear;
    float quadratic;  

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};  

struct DirLight {
    vec3 direction;
  
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};  

uniform DirLight dirLight;

uniform vec3 viewPos;

uniform sampler2DShadow shadowMap;
uniform sampler2D tex;
uniform vec4 colorOverride = vec4(0.0);  // rgb replaces the texture colour when w > 0

float ShadowCalculation(vec4 fragposlightspace)
{
    // perform perspective divide
    vec3 projCoords = fragposlightspace.xyz / fragposlightspace.w;
    projCoords = projCoords * 0.5 + 0.5; 
    if(projCoords.z > 1.0)
        return 0.0;
    float currentDepth = projCoords.z;  
    float bias = 0.01;
    float shadow = 0.0;
    return texture(shadowMap, vec3(projCoords.xy,projCoords.z));
}

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0f);
    // combine results
    vec3 ambient  = light.ambient;
    vec3 diffuse  = light.diffuse  * diff;
    vec3 specular = light.specular * spec; 
    return ambient + (diffuse + specular) * (ShadowCalculation(fragposlightspace));
}  

void main()
{

    float gamma = 2.2;

    vec4 lightless = texture(tex, texCoord);
    lightless = texture(tex, texCoord);
    if (lightless.a < 0.1) {
        discard;
    }

    lightless.rgb = mix(lightless.rgb, colorOverride.rgb, colorOverride.w);

    vec3 norm = normalize(normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    vec4 pregamma = (lightless) * vec4(CalcDirLight(dirLight,norm,viewDir),1.0f) * vec4(vec3(1.0f-1.0f/gl_FragCoord[3]/ 200.0f), 1.0f);

    FragColor = pregamma;

    //FragColor = vec4(1.0f,1.0f,1.0f,1.0f);
}   