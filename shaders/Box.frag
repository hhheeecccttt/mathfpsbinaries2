#version 330 core
out vec4 FragColor;
  
in vec2 TexCoords;

uniform float rounding = 0.0f;

uniform vec2 screenSize;
uniform vec2 normalizedCenterPos;
uniform vec2 normalizedCenterScale;
uniform vec2 pixelScale;
uniform vec2 pixelPos;

uniform vec4 Color = {0.0f,0.0f,0.0f,1.0f};
uniform vec4 BackgroundColor = {0.0f,0.0f,0.0f,1.0f};

void main()
{ 
    FragColor = Color;
    

    float rounding = clamp(rounding,0.0f,1.0f);
    if (rounding != 0.0f) {
        float roundingfactor = min(pixelScale.x,pixelScale.y)/2 * rounding;
        vec2 r1 = pixelScale/2 - vec2(roundingfactor);
        vec2 absrelcoord = abs(gl_FragCoord.xy - pixelScale/2 -  pixelPos);
    
        if (!(absrelcoord.x < r1.x || absrelcoord.y < r1.y)) {
            if (pow(absrelcoord.y-r1.y,2) > pow(roundingfactor,2) - pow((absrelcoord.x-r1.x),2)) {
                discard;
        }
    }
    }
    
}
