
#ifndef FBO_CLASS_H
#define FBO_CLASS_H

#include <glad/glad.h>

// cube

class FBO {
public:
	GLuint ID;
	FBO() {
		glGenFramebuffers(1, &ID);
	};
	void Bind() {
		glBindFramebuffer(GL_FRAMEBUFFER, ID);
	};
	void Unbind() {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	};
	void Delete() {
		glDeleteFramebuffers(1, &ID);
	};
};

#endif