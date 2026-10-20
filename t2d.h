#ifndef T2D_CLASS
#define T2D_CLASS

#include <iostream>
#include <random>
#include <vector>

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include "Structures.h"

class t2d_package {

public:

    glm::vec4 position;

    glm::vec4 size;
    glm::vec2 center = { 0.0f,0.0f };
    glm::vec2 pixelposition;
    glm::vec2 pixelsize;
    glm::vec2 NormalizedCenterPos;
    glm::vec2 NormalizedCenterScale;

    void Recalculate() {
        //WRITE SETTING FUNCTIONS THAT CALL THIS INSTEAD OF CALLING EVERY FRAME
        float fheight = (float)window->height;
        float fwidth = (float)window->width;

        pixelsize = {size.z + size.x * fwidth, size.w + size.y * fheight };
        pixelposition = { position.z + position.x * fwidth - center.x * pixelsize.x, position.w + position.y * fheight - center.y * pixelsize.y};

        NormalizedCenterPos = { 2.0f * (pixelposition.x+ pixelsize.x/2.0f) / fwidth - 1.0f,2.0f * (pixelposition.y + pixelsize.y / 2.0f) / fheight - 1.0f };
        NormalizedCenterScale = {pixelsize.x/ fwidth,pixelsize.y/ fheight};
    }


private:
};

#endif
