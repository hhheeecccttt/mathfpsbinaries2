#ifndef T_FUNCTIONS
#define T_FUNCTIONS

#include <iostream>
#include <random>
#include <vector>
#include <optional>

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>

#include "t.h"
#include "Structures.h"

struct TInTInfo {
	glm::vec3 POI;
	glm::vec3 CN;
	float overlap;
};

struct AxisCollisionCN {
	float overlap;
	bool infront;
	glm::vec3 POI;
};

glm::vec3 CalculateTriangleNormal(Triangle t);

float Magnitude2(const glm::vec3& v);

float Magnitude(const glm::vec3& v);

float SquaredPerpendicularMagnitude(glm::vec3 off, glm::vec3 line);

float VolumeOfTriangle(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3);
std::optional<glm::vec3> RayIntersectsTriangle(const Ray& ray,
	const Triangle triangle);

std::optional<glm::vec3> IsRayInT(const Ray& ray, t& t);
bool IsRayInTBool(const Ray& ray, t& t);

bool BoundingBoxInBoundingBox(const BoundingBox bb1, const BoundingBox bb2);
bool BoundingAxisInBoundingAxis(BoundingAxis ba1, BoundingAxis ba2);

bool TAxisCollidesTNoInfo(std::vector < glm::vec3 >& t1, std::vector < glm::vec3 >& t2, glm::vec3 axis);
std::optional<std::pair<float, glm::vec3>> TAxisCollidesT(std::vector < glm::vec3 >& t1, std::vector < glm::vec3 >& t2, glm::vec3 axis);
std::optional<AxisCollisionCN> TAxisCollidesTCN(std::vector < glm::vec3 >& t1, std::vector < glm::vec3 >& t2, glm::vec3 axis);

bool TNearT(t& t1, t& t22);
std::optional<TInTInfo> TInT(t& t1, t& t2);
bool TInTNoInfo(t& t1, t& t2);

glm::quat AngleAxis(glm::vec3 axis, float angle);
glm::vec3 LookAtVector(glm::vec3 start, glm::vec3 end);

glm::quat RotationBetweenVectors(glm::vec3 start, glm::vec3 dest);
glm::quat LookAt(glm::vec3 direction);

glm::quat LookAt(glm::vec3 start, glm::vec3 end);

#endif