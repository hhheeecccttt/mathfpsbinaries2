
#ifndef OBJECTS_CLASS
#define OBJECTS_CLASS

#include <iostream>
#include <vector>
#include <string>

#include<glm/glm.hpp>

#include "t.h"
#include "Physics.h"

class Object {

public:
	std::string name;


	Object() {

	}

	~Object() {
		for (auto child : children) {
			child->Delete();
		}
	}
	
	Object(Object* parent) : parent(parent) {

	}

	std::vector<Object*> GetChildren() {
		return children;
	}

	Object* GetFirstChildOfName(std::string name) {
		for (int i = 0; i < children.size(); i++) {
			if (children[i]->name == name) {
				return children[i];
			}
		};
		return nullptr;
	}

	void RemoveFromParent() {
		if (parent != nullptr) {
			parent->RemoveChild(this);
		}
	}

	void AddChild(Object* obj) {
		obj->RemoveFromParent();
		obj->parent = this;
		children.push_back(obj);
	}

	void RemoveChild(Object* obj) {
		auto it = std::find(children.begin(), children.end(), obj);
		if (it != children.end()) {
			children.erase(it);
			obj->parent = nullptr;
		}
	}

	void DeleteChild(Object* obj) {
		auto it = std::find(children.begin(),children.end(),obj);
		if (it != children.end()) {
			children.erase(it);
			obj->parent = nullptr;
			obj->Delete();
		}		
	}

	void SetParent(Object* nparent) {
		nparent->AddChild(this);
	}

	Object* GetParent() {
		return parent;
	}

	int GetChildrenAmount() {
		return children.size();
	}

	void ClearChildren() {
		children = {};
	}

	virtual void Delete() {
		for (auto child : children) {
			child->Delete();
		}
		RemoveFromParent();
		
		delete this;
	}

	virtual Object* Clone() {
		return new Object(*this);
	}
	
private:
	std::vector<Object*> children = {};
	Object* parent = nullptr;
};

#endif