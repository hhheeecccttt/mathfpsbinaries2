#ifndef MESH_DATA_CLASS
#define MESH_DATA_CLASS

#include <vector>

#include <glm/glm.hpp>

#include "Vertex.h"

class MeshData {
public:
	std::vector<Vertex> vertices;
	std::vector<GLuint> indices;
};

class RigMeshData {
public:
	std::vector<RigVertex> vertices;
	std::vector<GLuint> indices;
};

#endif