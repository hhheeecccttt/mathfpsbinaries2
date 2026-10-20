
#ifndef EBO_CLASS_H
#define EBO_CLASS_H

#include <glad/glad.h>

class EBO {
public:
	GLuint ID;
	EBO();
	void GenerateID() {
		glGenBuffers(1, &ID);
	};
	void BufferData(GLuint* indices, GLsizeiptr size);
	void Bind();
	void Unbind();
	void Delete();
};

#endif