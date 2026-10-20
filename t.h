#ifndef T_CLASS
#define T_CLASS

#include <iostream>
#include <random>
#include <vector>
#include <cmath>

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include "Structures.h"

class t {

public:

	t() {
		pos = glm::vec3(0.0f);
		scale = glm::vec3(1.0f);
		orient = glm::quat();
	}

	BoundingBox GetRotationlessAABB() {
		BoundingBox tr;
		tr.min = pos - scale/2.0f;
		tr.max = pos + scale/2.0f;
		return tr;
	}

	BoundingBox GetAABB() {
		std::vector<glm::vec3> corners = {
			{0.5f,0.5f,0.5f},
			{0.5f,-0.5f,0.5f},
			{0.5f,-0.5f,-0.5f},
			{0.5f,0.5f,-0.5f},
			{-0.5f,0.5f,0.5f},
			{-0.5f,-0.5f,0.5f},
			{-0.5f,0.5f,-0.5f},
			{-0.5f,-0.5f,-0.5f},
		};

		glm::vec3 min(FLT_MAX);
		glm::vec3 max(-FLT_MAX);
		for (glm::vec3& corner : corners) {
			corner *= scale;
			corner = orient * corner;
			corner += pos;
			min = { glm::min(min.x, corner.x) ,glm::min(min.y, corner.y), glm::min(min.z, corner.z) };
			max = { glm::max(max.x, corner.x) ,glm::max(max.y, corner.y), glm::max(max.z, corner.z) };
		}
		BoundingBox tr;
		tr.min = min;
		tr.max = max;
		//std::cout << tr.min.x << ' ' << tr.min.y << ' ' << tr.min.z << ' ' << tr.max.x << ' ' << tr.max.y << ' ' << tr.max.z << '\n';
		return tr;
	}

	float ScaleMagnitude2() {
		return scale.x* scale.x + scale.y* scale.y + scale.z* scale.z;
	}

	BoundingAxis GetBoundingAxis(int axis) {
		std::vector<glm::vec3> corners = {
		{0.5f,0.5f,0.5f},
		{0.5f,-0.5f,0.5f},
		{0.5f,-0.5f,-0.5f},
		{0.5f,0.5f,-0.5f},
		{-0.5f,0.5f,0.5f},
		{-0.5f,-0.5f,0.5f},
		{-0.5f,0.5f,-0.5f},
		{-0.5f,-0.5f,-0.5f},
		};

		float min = FLT_MAX;
		float max = -FLT_MAX;
		bool init = false;
		for (glm::vec3& corner : corners) {
			corner *= scale;
			corner = orient * corner;
			corner += pos;
			min = std::min(min, corner[axis]);
			max = std::max(max, corner[axis]);
		}
		BoundingAxis tr;
		tr.min = min;
		tr.max = max;
		return tr;
	}

	void LookAt(glm::vec3 eye, glm::vec3 target) noexcept {
		TranslateTo(eye);
		orient = -orient * target;
	}

	void TranslateBy(glm::vec3 e) noexcept {
		pos += e;
	}

	void TranslateTo(glm::vec3 e) noexcept {
		pos = e;
	}

	void ScaleBy(glm::vec3 e) noexcept {
		scale *= e;
	}

	void ScaleBy(GLfloat e) noexcept {
		scale *= e;
	}

	void ScaleTo(glm::vec3 e) noexcept {
		scale = e;
	}

	void ScaleTo(GLfloat e) noexcept {
		scale = glm::vec3(e);
	}

	void ScaleToX(GLfloat e) noexcept {
		scale.x = e;
	}

	void ScaleToY(GLfloat e) noexcept {
		scale.y = e;
	}

	void ScaleToZ(GLfloat e) noexcept {
		scale.z = e;
	}

	void RotateToQuaternion(glm::quat q) noexcept {
		orient = q;
	}

	void RotateByQuaternion(glm::quat q) noexcept {
		orient = q * orient;
	}

	void NormalizeRotation() noexcept {
		orient = glm::normalize(orient);
	}

	void RotateByQuaternionCumulate(glm::quat q) noexcept {
		orient = orient * q;
	}

	void RotateByEulerAnglesCumulate(glm::vec3 e) noexcept {
		orient = orient * glm::quat(e);
	}
	void RotateToEulerAngles(glm::vec3 e) noexcept {
		orient = glm::quat(e);
	}
	void RotateByEulerAngles(glm::vec3 e) noexcept {
		RotateByQuaternion(glm::quat(e));
	}

	glm::vec3 GetUpVector() const noexcept {
		glm::vec3 uppoint(0.0f, 1.0f,0.0f);
		return orient * uppoint;
	}

	glm::vec3 GetRightVector() const noexcept {
		glm::vec3 rightpoint(1.0f,0.0f, 0.0f);
		return orient * rightpoint;
	}

	glm::vec3 GetFrontVector() const noexcept {
		
		glm::vec3 frontpoint(0.0f,0.0f,1.0f);
		return orient * frontpoint;
	}

	glm::vec3 GetScale() const noexcept {
		return scale;
	}

	glm::vec3 GetTranslation() const noexcept {
		return pos;
	};

	float GetTranslationX() const noexcept {
		return pos.x;
	}

	float GetTranslationY() const noexcept {
		return pos.y;
	}

	float GetTranslationZ() const noexcept {
		return pos.z;
	}

	void CopyMatrixTo(t& target) const noexcept {

		target.scale = scale;
		target.orient = orient;
		target.pos = pos;
	}

	glm::mat4 GetScaleMatrix() const noexcept {
		return glm::scale(glm::mat4(1.0f), scale);
	}

	glm::mat4 GetTranslationMatrix() const noexcept {
		return glm::translate(glm::mat4(1.0f), pos);
	}

	glm::mat4 GetRotationMatrix() const noexcept {
		return glm::mat4_cast(orient);
	}

	glm::mat4 GetMatrix() const noexcept {
		return GetTranslationMatrix() * GetRotationMatrix() * GetScaleMatrix();
	}

	glm::mat4 GetMatrixRotationless() const noexcept {
		glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);
		glm::mat4 translMat = glm::translate(glm::mat4(1.0f), pos);
		return translMat* scaleMat;
	}

	glm::quat GetRotationQuaternion() const noexcept {
		return orient;
	}

	glm::vec3 GetRotationEulerAngles() const noexcept {
		return glm::eulerAngles(orient);
	}

private:
	glm::vec3 pos;
	glm::vec3 scale;
	glm::quat orient;
};

class t_package {
public:
	t t;
};

#endif
