#ifndef RESOURCE_MANAGER_CLASS
#define RESOURCE_MANAGER_CLASS

#include <vector>

#include "Folder.h"
//#include "font.h"
#include "Model.h"
#include "SoundSystem.h"

class ResourceManager {
public:
	//std::vector<Font*> fonts;
	std::vector<Model*> models;
	std::vector<SoundData*> sounds;
public:

};

#endif