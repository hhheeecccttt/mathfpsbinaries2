

#ifndef ENGINE_CLASS
#define ENGINE_CLASS

//OVERARCHING LIBRARIES
#include <iostream>
#include <vector>

//OPENGL LIBRARIES
#include<glad/glad.h>
#include<GLFW/glfw3.h>

//MANMADE LIBRARIES
#include "common.h"

inline int fps = 60;

void framebuffer_size_callback(GLFWwindow* windowe, int w, int h);

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

void mouse_callback(GLFWwindow* windowe, double xposIn, double yposIn);

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);

class Physics;
class RenderSystem;
class SoundSystem;
class Folder;
class ParticleEmitter;
class Camera;
class Animator;

class Engine {
public:
    //SYSTEMS
    Physics* physicsengine;
    RenderSystem* rendersystem;
public:
    //METADATA
    int frame = 0;
public:
    //DEBUG
    bool renderwireframe = false;
public:
    //RESOURCE MANAGEMENT
    Folder* mainf;
public:
    //CAMERA
    Camera* camera;
public:

    Engine();

    void Initiate();
};

#endif
