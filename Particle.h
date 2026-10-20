#ifndef PARTICLE_CLASS
#define PARTICLE_CLASS

#include <random>
#include <vector>
#include <map>

#include<glm/glm.hpp>

#include "common.h"
#include "Engine.h"
#include "Object.h"
#include "Mesh.h"
#include "QuadVertices.h"
#include "t.h"
#include "VAO.h"
#include "VBO.h"
#include "EBO.h"
#include "shaderClass.h"
#include "Vertex.h"
#include "texture.h"
#include "Camera.h"
#include "RenderSystem.h"

class Particle : public t_package {
public:

	Particle() {

	}

	void Render() {

	}
	
	void Step(float dt) {
		t.RotateByEulerAnglesCumulate(angularvelocity * dt);
		t.TranslateBy(linearvelocity*dt);
	}
	glm::vec3 linearvelocity;
	glm::vec3 angularvelocity;
	float lifespan;
private:
};

class ParticleEmitter : public Renderable, public t_package, public Object {
public:

	enum EmitDirection {
		Perpendicular,
		Aligned,
		Outward
	};

	EmitDirection emitdirection = Outward;
	//PARTICLE SETTINGS
	glm::vec3 size = {1.0f,1.0f,1.0f};
	glm::vec3 emitangle = {0.0f,0.0f,0.0f};
	float lifespan = 5.0f;
	float speed = 1.0f;
	glm::vec3 angularvelocity = {0.0f,0.0f,0.0f};
	bool facecamera = false;

	glm::vec4 color = {1.0f,1.0f,1.0f,1.0f};

public:
	std::vector<Particle*> particles;

	Texture* tex;

	Mesh* mesh;
	
	ParticleEmitter() {
		shadertype = ParticleShader;
		mesh = new Mesh(quadVertices3D, quadIndices3D);
	}

	ParticleEmitter(const std::vector<Vertex>& meshvertices, const std::vector<GLuint>& meshindices) {
		shadertype = ParticleShader;
		mesh = new Mesh(meshvertices, meshindices);
	}

	~ParticleEmitter() {
		mesh->Delete();
	}

	void Emit() {
		Particle* np = new Particle();
		glm::quat rot = glm::quat(glm::vec3({ (float)rand() / (float)RAND_MAX * emitangle[0] - emitangle[0] / 2.0f,(float)rand() / (float)RAND_MAX * emitangle[1] - emitangle[1] / 2.0f,(float)rand() / RAND_MAX * emitangle[2] - emitangle[2] / 2.0f }));
		np->t.TranslateTo(t.GetTranslation());
		np->linearvelocity = (t.GetFrontVector() * speed) * rot;	
		np->angularvelocity = angularvelocity;
		np->t.ScaleTo(size);

		if (emitdirection == Perpendicular) {
			np->t.RotateToQuaternion(glm::conjugate(rot) * glm::quat({ glm::radians(90.0f),0.0f,0.0f }) * t.GetRotationQuaternion());
		}
		else if (emitdirection == Outward) {
			np->t.RotateToQuaternion(glm::conjugate(rot) * t.GetRotationQuaternion());
		}
		else {
			np->t.RotateToQuaternion(t.GetRotationQuaternion());
		}

		np->lifespan = lifespan;

		particles.push_back(np);
	}

	void Step(float dt) {
		for (auto it = particles.begin(); it != particles.end(); ) {
			Particle* p = *it;
			p->lifespan -= dt;

			if (p->lifespan <= 0.0f) {
				delete p;
				it = particles.erase(it);
			}
			else {
				p->Step(dt);
				++it;
			}
		}
	}

	void Render(Shader& ShaderProgram) override {
		if (particles.size() <= 0) return;
		ShaderProgram.Activate();

		mesh->vao.Bind();

		ShaderProgram.Set4F("color",color);

		glActiveTexture(0);

		if (tex != nullptr) {
			glBindTexture(GL_TEXTURE_2D, tex->ID);
			ShaderProgram.SetInt("tex", 0);
		}
		else glBindTexture(GL_TEXTURE_2D, 0);
		std::vector<glm::mat4> matrices;

		if (color.w != 1.0f) {
			std::map<float, Particle*> sorted;
			for (unsigned int i = 0; i < particles.size(); i++)
			{
				sorted[Magnitude2(engine->camera->t.GetTranslation() - particles[i]->t.GetTranslation())] = particles[i];
			}
			for (std::map<float, Particle*>::reverse_iterator it = sorted.rbegin(); it != sorted.rend(); ++it)
			{
				if (facecamera) {
					matrices.emplace_back(it->second->t.GetTranslationMatrix() * engine->camera->t.GetRotationMatrix() * it->second->t.GetScaleMatrix());
				}
				else {
					matrices.emplace_back(it->second->t.GetMatrix());
				}
			}
		}
		else {
			for (int i = 0; i < particles.size(); i++)
			{
				if (facecamera) {
					matrices.emplace_back(particles[i]->t.GetTranslationMatrix() * engine->camera->t.GetRotationMatrix() * particles[i]->t.GetScaleMatrix());
				}
				else {
					matrices.emplace_back(particles[i]->t.GetMatrix());
				}
			}
		}
		unsigned int buffer;
		glGenBuffers(1, &buffer);
		glBindBuffer(GL_ARRAY_BUFFER, buffer);
		glBufferData(GL_ARRAY_BUFFER, particles.size() * sizeof(glm::mat4), &matrices[0], GL_STATIC_DRAW);

		mesh->vao.Bind();
		// vertex attributes
		std::size_t vec4Size = sizeof(glm::vec4);
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)0);
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(1 * vec4Size));
		glEnableVertexAttribArray(5);
		glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(2 * vec4Size));
		glEnableVertexAttribArray(6);
		glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(3 * vec4Size));

		glVertexAttribDivisor(3, 1);
		glVertexAttribDivisor(4, 1);
		glVertexAttribDivisor(5, 1);
		glVertexAttribDivisor(6, 1);

		glDrawElementsInstanced(GL_TRIANGLES, mesh->indices.size(), GL_UNSIGNED_INT, 0, particles.size());

		mesh->vao.Unbind();
	}

private:

};

#endif