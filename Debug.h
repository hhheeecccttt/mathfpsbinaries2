
#ifndef DEBUG_CLASS
#define DEBUG_CLASS

#include <iostream>
#include <chrono>
#include <string>

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>

#include "tFunctions.h"
#include "Mesh.h"
#include "Structures.h"

const std::vector<Vertex> CUBEVERTICES = {
{{0.5f,0.5f,0.5f}, {1.0f,0.0f,0.0f},{ 0.0f,0.0f} },
{{0.5f,0.5f,-0.5f}, {1.0f,0.0f,0.0f},{ 0.0f,1.0f}},
{{0.5f,-0.5f,-0.5f}, {1.0f,0.0f,0.0f},{ 1.0f,1.0f}},
{{0.5f,-0.5f,0.5f}, {1.0f,0.0f,0.0f}, {1.0f,0.0f}},

{{-0.5f,0.5f,0.5f}, {-1.0f,0.0f,0.0f}, {0.0f,0.0f}},
{{-0.5f,0.5f,-0.5f}, {-1.0f,0.0f,0.0f}, {0.0f,1.0f}},
{{-0.5f,-0.5f,-0.5f}, {-1.0f,0.0f,0.0f}, {1.0f,1.0f}},
{{-0.5f,-0.5f,0.5f},{ -1.0f,0.0f,0.0f}, {1.0f,0.0f}},

{{0.5f,0.5f,0.5f}, {0.0f,1.0f,0.0f},{ 0.0f,0.0f} },
{{0.5f,0.5f,-0.5f}, {0.0f,1.0f,0.0f},{ 0.0f,1.0f}},
{{-0.5f,0.5f,-0.5f}, {0.0f,1.0f,0.0f},{ 1.0f,1.0f}},
{{-0.5f,0.5f,0.5f}, {0.0f,1.0f,0.0f}, {1.0f,0.0f}},

{{0.5f,-0.5f,0.5f}, {0.0f,-1.0f,0.0f}, {0.0f,0.0f}},
{{0.5f,-0.5f,-0.5f}, {0.0f,-1.0f,0.0f}, {0.0f,1.0f}},
{{-0.5f,-0.5f,-0.5f}, {0.0f,-1.0f,0.0f}, {1.0f,1.0f}},
{{-0.5f,-0.5f,0.5f},{ 0.0f,-1.0f,0.0f}, {1.0f,0.0f}},

{{0.5f,0.5f,0.5f}, {0.0f,0.0f,1.0f},{ 0.0f,0.0f} },
{{-0.5f,0.5f,0.5f}, {0.0f,0.0f,1.0f},{ 0.0f,1.0f}},
{{-0.5f,-0.5f,0.5f}, {0.0f,0.0f,1.0f},{ 1.0f,1.0f}},
{{0.5f,-0.5f,0.5f}, {0.0f,0.0f,1.0f}, {1.0f,0.0f}},

{{0.5f,0.5f,-0.5f}, {0.0f,0.0f,-1.0f}, {0.0f,0.0f}},
{{-0.5f,0.5f,-0.5f}, {0.0f,0.0f,-1.0f}, {0.0f,1.0f}},
{{-0.5f,-0.5f,-0.5f}, {0.0f,0.0f,-1.0f}, {1.0f,1.0f}},
{{0.5f,-0.5f,-0.5f},{ 0.0f,0.0f,-1.0f}, {1.0f,0.0f}}
};

const std::vector<unsigned int> CUBEINDICES = {
    0, 3, 1,  3, 2, 1,
    4, 5, 7,  5, 6, 7,

    8, 9, 11,  9, 10, 11,
    12, 15, 13,  15, 14, 13,

    16, 17, 19,  17, 18, 19,
    20, 23, 21,  23, 22, 21
};

const std::vector<Vertex> PLANEVERTICES = {
{{0.5f,0.0f,0.5f}, {0.0f,1.0f,0.0f},{ 0.0f,0.0f} },
{{-0.5f,0.0f,0.5f}, {0.0f,1.0f,0.0f},{ 1.0f,0.0f}},
{{-0.5f,0.0f,-0.5f}, {0.0f,1.0f,0.0f},{ 1.0f,1.0f}},
{{0.5f,0.0f,-0.5f}, {0.0f,1.0f,0.0f}, {0.0f,1.0f}},
};

const std::vector<unsigned int> PLANEINDICES = {
    0,1,2,
    2,3,0,

    2,1,0,
    0,3,2,
};

inline void Output(std::string& to) {
    std::cout << to << '\n';
}

inline void Output(float& to) {
    std::cout << to << '\n';
}

inline void Output(int& to) {
    std::cout << to << '\n';
}

inline void Output(glm::vec3 tooutput) {
	std::cout << tooutput.x << ' ' << tooutput.y << ' ' << tooutput.z << '\n';
}

inline void Output(BoundingBox tooutput) {
    std::cout << tooutput.min.x << ' ' << tooutput.min.y << ' ' << tooutput.min.z << ' ' << tooutput.max.x << ' ' << tooutput.max.y << ' ' << tooutput.max.z << '\n';
}

inline void Output(glm::mat4 tooutput) {
    std::cout << '[' << tooutput[0][0] << ',' << tooutput[0][1] << ',' << tooutput[0][2] << ',' << tooutput[0][3] << ',' << '\n'
        << tooutput[1][0] << ',' << tooutput[1][1] << ',' << tooutput[1][2] << ',' << tooutput[1][3] << ',' << '\n'
        << tooutput[2][0] << ',' << tooutput[2][1] << ',' << tooutput[2][2] << ',' << tooutput[2][3] << ',' << '\n'
        << tooutput[3][0] << ',' << tooutput[3][1] << ',' << tooutput[3][2] << ',' << tooutput[3][3] << ',' << '\n' << ']' << '\n';
}

inline Mesh* CreateCubeMesh() {
    return new Mesh(CUBEVERTICES, CUBEINDICES);
}
inline Mesh* CreatePlaneMesh() {
    return new Mesh(PLANEVERTICES, PLANEINDICES);
}

#endif