#ifndef DEFAULT_BODY_COMPONENTS
#define DEFAULT_BODY_COMPONENTS

#include <vector>

#include "Character.h"
#include "BodyComponentActions.h"

std::vector<BodyComponent*> GenerateDefaultBody() {
	std::vector<BodyComponent*> TR;
	BodyComponent* Head = new BodyComponent();
	BodyComponent* Torso = new BodyComponent();
	BodyComponent* RightArm = new BodyComponent();
	BodyComponent* LeftArm = new BodyComponent();
	BodyComponent* RightLeg = new BodyComponent();
	BodyComponent* LeftLeg = new BodyComponent();

	Head->name = "Head";
	Torso->name = "Torso";
	RightArm->name = "RightArm";
	LeftArm->name = "LeftArm";
	RightLeg->name = "RightLeg";
	LeftLeg->name = "LeftLeg";

	TR.push_back(Head);
	TR.push_back(Torso);
	TR.push_back(RightArm);
	TR.push_back(LeftArm);
	TR.push_back(RightLeg);
	TR.push_back(LeftLeg);

	return TR;
}


#endif