
#ifndef VBO_CLASS_H
#define VBO_CLASS_H

#include <glad/glad.h>

#include "Vertex.h"

class VBO {
public:
	GLuint ID;
	VBO();
	void GenerateID() {
		glGenBuffers(1, &ID);
	};
	void BufferData(void* vertices, GLsizeiptr size);
	void Bind();
	void Unbind();
	void Delete();
};

#endif