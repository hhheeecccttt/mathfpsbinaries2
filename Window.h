#ifndef WINDOW_CLASS
#define WINDOW_CLASS

#include <iostream>

#include<glad/glad.h>
#include<GLFW/glfw3.h>

class Window {
public:
	int height;
	int width;
	GLFWwindow* handle;
	Window(int width, int height, const char* name) : height(height) , width(width) {
		handle = glfwCreateWindow(width,height,name, NULL, NULL);
		if (handle == NULL) {
			std::cout << "WINDOW HANDLE IS NULL\n";
			glfwTerminate();
			throw - 1;
		}
		glfwMakeContextCurrent(handle);
	}
	
	~Window() {
		glfwDestroyWindow(handle);
	}

};

#endif