#version 330 core
out vec4 FragColor;
  
in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform float brightness = 0.0f;
uniform float colorContrast = 0.0f;
uniform float saturation = 1.0f;

void main()
{ 
    vec3 originalcolor = texture(screenTexture, TexCoords).rgb;
    vec3 finalcolor = originalcolor;
    const float gamma = 2.2;
    finalcolor = pow(finalcolor, vec3(1.0 / gamma));

    vec3 contrast = vec3(1.0f) - finalcolor;

    finalcolor = mix(finalcolor,vec3(1.0f) - finalcolor,colorContrast);

    float average = (finalcolor[0]+finalcolor[1]+finalcolor[2])/3.0f;
    
    for (int i = 0; i < 3; i++) {
        finalcolor[i] = average + saturation * (finalcolor[i] - average);
    }

    FragColor = vec4(finalcolor,1.0f);
}