
#ifndef TEXTURE_CLASS_H
#define TEXTURE_CLASS_H

#include "stb_image.h"
#include <glad/glad.h>

#include "shaderClass.h"

class Texture {
public:
	GLuint ID;
	std::string path;
	Texture(const char* filename);
	void texUnit(Shader shader, const char* uniform, GLuint unit);
	void Bind();
	void Unbind();
	void Delete();
};

#endif
