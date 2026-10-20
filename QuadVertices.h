
#ifndef QUAD_VERTICES
#define QUAD_VERTICES

#include <vector>
#include "Vertex.h"

inline float quadVertices[] = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
	// positions   // texCoords
	-1.0f,  1.0f,  0.0f, 1.0f,
	-1.0f, -1.0f,  0.0f, 0.0f,
	 1.0f, -1.0f,  1.0f, 0.0f,

	-1.0f,  1.0f,  0.0f, 1.0f,
	 1.0f, -1.0f,  1.0f, 0.0f,
	 1.0f,  1.0f,  1.0f, 1.0f
};

inline std::vector<Vertex> quadVertices3D = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
	// positions   //normals         // texCoords
	{{-1.0f,  1.0f,0.0f},  {0.0f,0.0f,1.0f},{0.0f, 1.0f}},
	{{-1.0f, -1.0f, 0.0f}, {0.0f,0.0f,1.0f},{0.0f, 0.0f}},
	{{1.0f, -1.0f, 0.0f}, {0.0f,0.0f,1.0f},{1.0f, 0.0f}},
	{{1.0f,  1.0f, 0.0f},{0.0f,0.0f,1.0f}, {1.0f, 1.0f}}
};

inline std::vector<unsigned int> quadIndices3D = {
	0,1,2,
	0,2,3,
};

inline float quadVerticesNoTexCoords[] = {
	// positions  
	-1.0f,  1.0f, 
	-1.0f, -1.0f,
	 1.0f, -1.0f, 

	-1.0f,  1.0f, 
	 1.0f, -1.0f, 
	 1.0f,  1.0f, 
};

#endif