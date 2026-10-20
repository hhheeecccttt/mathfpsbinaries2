
#ifndef BOUNDING_BOX_CLASS
#define BOUNDING_BOX_CLASS

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>

struct Ray {
	glm::vec3 origin;
	glm::vec3 direction;
};

struct Triangle {
	glm::vec3 a;
	glm::vec3 b;
	glm::vec3 c;
};

struct BoundingAxis {
	float min;
	float max;
};

struct BoundingBox {
	glm::vec3 min;
	glm::vec3 max;
};

#endif