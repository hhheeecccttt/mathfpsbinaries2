#ifndef LIGHTING_CLASS
#define LIGHTING_CLASS

#include "t.h"
#include "tFunctions.h"

class Lighting {
public:
	t dirlighting;

	Lighting() {
		dirlighting.RotateToQuaternion(glm::quat({ glm::radians(90.0f),0.0f,0.0f}));
		dirlighting.TranslateTo({0.0f,0.0f,0.0f});
	}


private:
	
};

#endif