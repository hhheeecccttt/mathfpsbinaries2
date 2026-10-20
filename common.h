#ifndef  COMMON_H
#define COMMON_H

#include<glad/glad.h>
#include<GLFW/glfw3.h>

class Engine;
class Window;
class Mouse;
class Keyboard;
class SoundSystem;
class ResourceManager;

inline Window* window;
inline Engine* engine;
inline Keyboard* keyboard;
inline Mouse* mouse;
inline SoundSystem* soundsystem;
inline ResourceManager* resourcemanager;

#endif // ! COMMON_H