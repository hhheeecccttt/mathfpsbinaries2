
#ifndef VERTEX_CLASS
#define VERTEX_CLASS

#define MAX_BONE_INFLUENCE 4

#include <glm/glm.hpp>

struct Vertex {
	glm::vec3 Position;
	glm::vec3 Normal;
	glm::vec2 TexCoords;
};

struct RigVertex {
	glm::vec3 Position;
	glm::vec3 Normal;
	glm::vec2 TexCoords;
	int m_BoneIDs[MAX_BONE_INFLUENCE];

	float m_Weights[MAX_BONE_INFLUENCE];
};

#endif