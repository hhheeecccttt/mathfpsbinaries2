#ifndef  SHADER_CLASS_H
#define SHADER_CLASS_H

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>

#include<string>
#include<fstream>
#include  <sstream>
#include <iostream>
#include <cerrno>

std::string get_file_contents(const char* filename);

class Shader {
public:
	GLuint ID;
	Shader(const char* vertexFile, const char* fragFile);
	
	void Set1F(const char* uniform, float value) {
		GLint location = glGetUniformLocation(Shader::ID, uniform);
		glUniform1f(location, value);
	};

	void Set2F(const char* uniform, glm::vec2 value) {
		GLint location = glGetUniformLocation(Shader::ID, uniform);
		glUniform2f(location, value.x,value.y);
	};

	void Set3F(const char* uniform, glm::vec3 value) {
		GLint location = glGetUniformLocation(Shader::ID, uniform);
		glUniform3f(location, value.x,value.y,value.z);
	};
	void Set4F(const char* uniform, glm::vec4 value) {
		GLint location = glGetUniformLocation(Shader::ID, uniform);
		glUniform4f(location, value.x, value.y, value.z, value.w);
	};
	void SetMat4(const char* uniform, glm::mat4 value) {
		GLint location = glGetUniformLocation(Shader::ID, uniform);
		glUniformMatrix4fv(location,1,GL_FALSE,&value[0][0]);
	};

	void SetInt(const char* uniform, GLuint value) {
		GLint location = glGetUniformLocation(Shader::ID, uniform);
		glUniform1i(location, value);
	};

	void Activate();
	void Delete();
};

#endif