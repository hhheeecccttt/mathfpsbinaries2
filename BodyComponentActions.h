#pragma once

#include "Character.h"

void Footstep(CharacterBody* body, BodyComponent* bc) {
	body->t.TranslateBy(body->t.GetFrontVector() * 0.1f);
}