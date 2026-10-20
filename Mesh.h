#ifndef MESH_CLASS
#define MESH_CLASS

#include <iostream>
#include <random>
#include <vector>
#include <map>
#include <algorithm>

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>

#include "common.h"
#include "Engine.h"
#include "RenderSystem.h"
#include "Object.h"
#include "VAO.h"
#include "VBO.h"
#include "EBO.h"
#include "shaderClass.h"
#include "t.h"
#include "tFunctions.h"
#include "Vertex.h"
#include "texture.h"
#include "MeshData.h"

struct IntersectionData {
	glm::vec3 intersection;
	int face;
};

class Mesh : public Object , public Renderable, public t_package , public MeshData {
	friend class Model;
protected:
public:
	Texture* meshTexture = nullptr;
	glm::vec4 colorOverride = glm::vec4(0.0f);  // rgb + strength; 0 = use the texture's own colour

	VAO vao;
	VBO vbo;
	EBO ebo;

	Mesh() {

	};

	Mesh(const std::vector<Vertex>& meshvertices, const std::vector<GLuint>& meshindices/*, std::vector<Texture> textures*/)
	{
		InitializeMesh(meshvertices,meshindices);
	};

	void InitializeMesh(const std::vector<Vertex>& meshvertices, const std::vector<GLuint>& meshindices) {
		vertices = meshvertices;
		indices = meshindices;
		this->shadertype = ShaderType::MeshShader;
		GenerateRenderData();
		NormalizeVertices();
		UpdateVertices();
	}

	void GenerateRenderData() {
		vbo.GenerateID();
		vao.GenerateID();
		ebo.GenerateID();

		if (vertices.size() <= 0 or indices.size() <= 0) {
			std::cout << "NO VERTICES OR INDICES DURING RENDER DATA INTIALIZATION\n";

		}
		else {

			vao.Bind();
			vbo.BufferData(&vertices[0], vertices.size() * sizeof(Vertex));
			ebo.BufferData(&indices[0], indices.size() * sizeof(GLuint));

			vbo.Bind();
			ebo.Bind();

			vao.LinkVBO(vbo, 0, 3, GL_FLOAT, sizeof(Vertex), (void*)0);
			vao.LinkVBO(vbo, 1, 3, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
			vao.LinkVBO(vbo, 2, 2, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

			vao.Unbind();
			vbo.Unbind();
			ebo.Unbind();
		}
	}

	virtual ~Mesh() {
		vao.Delete();
		vbo.Delete();
		ebo.Delete();
		//delete this;
	}

	float GetVolume() const {
		float runningtotal = 0.0f;

		for (int i = 0; i < indices.size() / 3; i++) {
			runningtotal += VolumeOfTriangle(vertices[indices[i * 3]].Position, vertices[indices[i * 3 + 1]].Position, vertices[indices[i * 3 + 2]].Position);
		}
		
		return runningtotal * t.GetScale().x * t.GetScale().y * t.GetScale().z;
	}

	void Slice(glm::vec3 origin, glm::quat slicequat) {
		
		glm::vec3 pos = glm::conjugate(slicequat) * (t.GetTranslation() - origin);
		float relativeslicepos = -pos.y;
		std::vector<glm::vec3> rotatedvertices;
		std::vector<bool> vstates;

		if (pos.y * pos.y <= t.ScaleMagnitude2()) {
			bool posfound = false;
			bool negfound = false;
			for (int i = 0; i < vertices.size(); i++) {
				glm::vec3 vpos = glm::conjugate(slicequat) * (vertices[i].Position * t.GetScale());
				rotatedvertices.push_back(vpos);

				if (vpos.y> relativeslicepos) {
					posfound = true;
					vstates.push_back(true);
				}
				else {
					negfound = true;
					vstates.push_back(false);
				}
			}

			if (posfound and negfound) {
				std::vector<Vertex> posvertices;
				std::vector<Vertex> negvertices;
				std::vector<GLuint> posindices;
				std::vector<GLuint> negindices;

				std::vector<int> posnewvertices;
				std::vector<int> negnewvertices;

				glm::vec3 center;
				glm::vec3 negnormal = slicequat * glm::vec3(0.0f,1.0f,0.0f);
				glm::vec3 posnormal = -negnormal;

				for (int i = 0; i < indices.size()/3; i++) {
					//I IS THE CURRENT TRIANGLE

					//CONVERT TO VERTICES BY MULTI 3 + whatever

					int ind1 = indices[i * 3 + 0];
					int ind2 = indices[i * 3 + 1];
					int ind3 = indices[i * 3 + 2];

					bool vstate1 = vstates[ind1];
					bool vstate2 = vstates[ind2];
					bool vstate3 = vstates[ind3];

					if (vstate1 and vstate2 and vstate3) {

						posvertices.push_back(vertices[ind1]);
						posvertices.push_back(vertices[ind2]);
						posvertices.push_back(vertices[ind3]);

						posindices.push_back(posvertices.size() -3);
						posindices.push_back(posvertices.size() - 2);
						posindices.push_back(posvertices.size() - 1);
					}
					else if (not vstate1 and not vstate2 and not vstate3) {
						negvertices.push_back(vertices[ind1]);
						negvertices.push_back(vertices[ind2]);
						negvertices.push_back(vertices[ind3]);

						negindices.push_back(negvertices.size() - 3);
						negindices.push_back(negvertices.size() - 2);
						negindices.push_back(negvertices.size() - 1);
					}
					else {
						/**/
						int niso1;
						int niso2;
						int iso;
						Vertex v1;
						Vertex v2;

						bool isopos = false;
						if (vstate1 != vstate2 and vstate1 != vstate3) {
							//STATE1 IS THE ISO
							niso1 = ind2;
							niso2 = ind3;
							iso = ind1;
							isopos = vstate1;
						}
						else if (vstate2 != vstate1 and vstate2 != vstate3) {
							//STATE 2 IS THE ISO
							niso1 = ind3;
							niso2 = ind1;
							iso = ind2;
							isopos = vstate2;
						}
						else {
							//STATE 3 IS THE ISO
							niso1 = ind1;
							niso2 = ind2;
							iso = ind3;
							isopos = vstate3;
						}

						float v1factor = (rotatedvertices[niso1].y - relativeslicepos) / (rotatedvertices[niso1].y - rotatedvertices[iso].y);
						float v2factor = (rotatedvertices[niso2].y - relativeslicepos) / (rotatedvertices[niso2].y - rotatedvertices[iso].y);

						glm::vec3 v1pos = glm::mix(rotatedvertices[niso1], rotatedvertices[iso], v1factor);
						glm::vec3 v2pos = glm::mix(rotatedvertices[niso2], rotatedvertices[iso], v2factor);
						v1.Position = (slicequat * v1pos) / t.GetScale();
						v2.Position = (slicequat * v2pos) / t.GetScale();
						v1.Normal = glm::mix(vertices[niso1].Normal, vertices[iso].Normal, v1factor);
						v2.Normal = glm::mix(vertices[niso2].Normal, vertices[iso].Normal, v2factor);
						v1.TexCoords = glm::mix(vertices[niso1].TexCoords, vertices[iso].TexCoords, v1factor);
						v2.TexCoords = glm::mix(vertices[niso2].TexCoords, vertices[iso].TexCoords, v2factor);

						if (isopos) {
							//THE ISOLATED VERTEX IS IN THE POS MESH
							posvertices.push_back(v1);
							posvertices.push_back(v2);
							posvertices.push_back(vertices[iso]);
							posnewvertices.push_back(posvertices.size() - 3);
							posnewvertices.push_back(posvertices.size() - 2);

							posindices.push_back(posvertices.size() - 3);
							posindices.push_back(posvertices.size() - 2);
							posindices.push_back(posvertices.size() - 1);

							negvertices.push_back(vertices[niso1]);
							negvertices.push_back(vertices[niso2]);
							negvertices.push_back(v1);
							negvertices.push_back(v2);

							negindices.push_back(negvertices.size() - 4);
							negindices.push_back(negvertices.size() - 3);
							negindices.push_back(negvertices.size() - 1);

							negindices.push_back(negvertices.size() - 1);
							negindices.push_back(negvertices.size() - 2);
							negindices.push_back(negvertices.size() - 4);

							negnewvertices.push_back(negvertices.size() - 2);
							negnewvertices.push_back(negvertices.size() - 1);
						}
						else {
							//THE ISOLATED VERTEX IS IN THE NEG MESH
							negvertices.push_back(v1);
							negvertices.push_back(v2);
							negvertices.push_back(vertices[iso]);

							negindices.push_back(negvertices.size() - 3);
							negindices.push_back(negvertices.size() - 2);
							negindices.push_back(negvertices.size() - 1);

							negnewvertices.push_back(negvertices.size() - 3);
							negnewvertices.push_back(negvertices.size() - 2);

							posvertices.push_back(vertices[niso1]);
							posvertices.push_back(vertices[niso2]);
							posvertices.push_back(v1);
							posvertices.push_back(v2);

							posindices.push_back(posvertices.size() - 4);
							posindices.push_back(posvertices.size() - 3);
							posindices.push_back(posvertices.size() - 1);

							posindices.push_back(posvertices.size() - 1);
							posindices.push_back(posvertices.size() - 2);
							posindices.push_back(posvertices.size() - 4);

							posnewvertices.push_back(posvertices.size() - 2);
							posnewvertices.push_back(posvertices.size() - 1);
						}
					}
				}

				if (negnewvertices.size() >= 3) {

					std::sort(negnewvertices.begin(), negnewvertices.end(), [&negvertices](int& a, int& b) {
						return negvertices[a].Position.x > negvertices[b].Position.x;
					});

					for (int i = 2; i < negnewvertices.size(); i++)
					{
						int index0 = negnewvertices[i];
						int index1 = negnewvertices[i - 1];
						int index2 = negnewvertices[i - 2];
						
						negvertices.push_back(negvertices[index0]);
						if ((i+1) % 2 == 0) {
							negvertices.push_back(negvertices[index2]);
							
							negvertices.push_back(negvertices[index1]);
						}
						else {
							negvertices.push_back(negvertices[index1]);
							negvertices.push_back(negvertices[index2]);
						}
						
						negindices.push_back(negvertices.size()-3);
						negindices.push_back(negvertices.size() - 2);
						negindices.push_back(negvertices.size() - 1);
					}
				}
				if (posnewvertices.size() >= 3) {

					for (int i = 2; i < posnewvertices.size(); i++)
					{
						int index0 = posnewvertices[i];
						int index1 = posnewvertices[i - 1];
						int index2 = posnewvertices[i - 2];
						posvertices.push_back(posvertices[index0]);
						if (i% 2 == 0) {
							posvertices.push_back(posvertices[index2]);
							posvertices.push_back(posvertices[index1]);
						}
						else {
							posvertices.push_back(posvertices[index1]);
							posvertices.push_back(posvertices[index2]);
						}

						posindices.push_back(posvertices.size() - 3);
						posindices.push_back(posvertices.size() - 2);
						posindices.push_back(posvertices.size() - 1);
					}
				}

				if (posindices.size() > 0) {
					Mesh* posmesh = Clone();
					posmesh->vertices = posvertices;
					posmesh->indices = posindices;
					posmesh->t.TranslateBy(slicequat* glm::vec3(0.0f, 0.1f, 0.0f));
					posmesh->NormalizeVertices();
					posmesh->UpdateVertices();
					posmesh->UpdateIndices();
					GetParent()->AddChild(posmesh);
				}
				if (negindices.size() > 0) {
					Mesh* negmesh = Clone();
					negmesh->indices = negindices;
					negmesh->vertices = negvertices;
					negmesh->t.TranslateBy(slicequat* glm::vec3(0.0f, -0.1f, 0.0f));
					negmesh->NormalizeVertices();
					negmesh->UpdateVertices();
					negmesh->UpdateIndices();
					GetParent()->AddChild(negmesh);
				}
				
				Delete();
			}
		}
	}

	virtual Mesh* Clone() override {
		Mesh* tr = new Mesh(*this);
		tr->GenerateRenderData();
		return tr;
	};

	void Clear() {
		vertices = std::vector<Vertex>();
		indices = std::vector<GLuint>();
	}

	void SetVertices(std::vector<Vertex>& e) {
		glBufferSubData(GL_ARRAY_BUFFER, NULL, sizeof(e), &e[0]);

		glBindBuffer(GL_ARRAY_BUFFER, vbo.ID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(e), &e[0], GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void UpdateVertices() {
		if (vertices.size() > 0) {

			glBufferSubData(GL_ARRAY_BUFFER, NULL, sizeof(vertices), &vertices[0]);
			glBindBuffer(GL_ARRAY_BUFFER, vbo.ID);
			glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
		else std::cout << "NO VERTICES IN THIS MESH!!!!\n";
	}

	void UpdateIndices() {
		if (ebo.ID == 0) {
			std::cout << "EBO NOT INITIALIZED\m";
			return;
		}
		if (indices.size() > 0) {
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, NULL, sizeof(indices), &indices[0]);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo.ID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), &indices[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
		else std::cout << "NO INDICES IN THIS MESH!!!!\n";
	}

	void NormalizeVertices() {
		if (vertices.size() == 0) {
			std::cout << "TRIED TO NORMALIZED ZERO VERTICES???\n";
		}
		float biggestx = -FLT_MAX;
		float smallestx = FLT_MAX;
		float biggesty = -FLT_MAX;
		float smallesty = FLT_MAX;
		float biggestz = -FLT_MAX;
		float smallestz = FLT_MAX;
		for (Vertex& vertex : vertices) {
			biggestx = glm::max(biggestx, vertex.Position.x);
			smallestx = glm::min(smallestx, vertex.Position.x);
			biggesty = glm::max(biggesty, vertex.Position.y);
			smallesty = glm::min(smallesty, vertex.Position.y);
			biggestz = glm::max(biggestz, vertex.Position.z);
			smallestz = glm::min(smallestz, vertex.Position.z);
		}

		float xf = biggestx - smallestx;
		float yf = biggesty - smallesty;
		float zf = biggestz - smallestz;

		float midx = (biggestx + smallestx) / 2.0f;
		float midy = (biggesty + smallesty) / 2.0f;
		float midz = (biggestz + smallestz) / 2.0f;
		t.TranslateBy(glm::vec3(midx, midy, midz) * t.GetScale());

		t.ScaleBy(glm::vec3(xf,yf,zf));
		glm::vec3 scale = t.GetScale();
		if (scale.x == 0.0f) {
			t.ScaleToX(xf);
		}
		if (scale.y == 0.0f) {
			t.ScaleToY(yf);
		}
		if (scale.z == 0.0f) {
			t.ScaleToZ(zf);
		}

		for (Vertex& vertex : vertices) {
			vertex.Position = glm::vec3(
				(vertex.Position.x - midx) / xf,
				(vertex.Position.y - midy) / yf,
				(vertex.Position.z - midz) / zf
			);

			if (xf == 0.0f) {
				vertex.Position.x = 0.0f;
			}
			if (yf == 0.0f) {
				vertex.Position.y = 0.0f;
			}
			if (zf == 0.0f) {
				vertex.Position.z = 0.0f;
			}
		}
	}

	void Render(Shader& ShaderProgram) override {
		glm::mat4 topass = t.GetMatrix();

		ShaderProgram.Activate();
		glActiveTexture(GL_TEXTURE1);
		if (meshTexture != nullptr) {
			glBindTexture(GL_TEXTURE_2D, meshTexture->ID);
			ShaderProgram.SetInt("tex", 1);
		}
		else glBindTexture(GL_TEXTURE_2D, 0);

		ShaderProgram.SetMat4("modl",  topass);
		ShaderProgram.Set4F("colorOverride", colorOverride);
		vao.Bind();
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
		vao.Unbind();
	};

	virtual void RenderWireframe(Shader& ShaderProgram) {
		glDisable(GL_CULL_FACE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		Render(ShaderProgram);
		glEnable(GL_CULL_FACE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	};

	void DeleteRenderData() {
		vbo.Delete();
		ebo.Delete();
		vao.Delete();
	}

	virtual void Delete() override {
		DeleteRenderData();
		Object::Delete();
	}
public:
	//random ass functions
	void DivideFace(int face, glm::vec3 centerlocalspace) {

		int v1 = indices[face * 3];
		int v2 = indices[face * 3 + 1];
		int v3 = indices[face * 3 + 2];

		Vertex newvertex;
		newvertex.Position = centerlocalspace;
		newvertex.TexCoords = { 0.0f,0.0f };
		newvertex.Normal = CalculateTriangleNormal({ vertices[v1].Position ,vertices[v2].Position ,vertices[v3].Position });
		vertices.push_back(newvertex);
		int nvp = vertices.size() - 1;

		indices.erase(indices.begin() + face * 3, indices.begin() + face * 3 + 3);



		indices.push_back(v1);
		indices.push_back(v2);
		indices.push_back(nvp);

		indices.push_back(v2);
		indices.push_back(v3);
		indices.push_back(nvp);

		indices.push_back(v3);
		indices.push_back(v1);
		indices.push_back(nvp);

		UpdateIndices();
		UpdateVertices();
	}

	bool RayIntersectsMeshNoInfo(const Ray& ray)
	{
		glm::mat4 inverse = glm::inverse(t.GetMatrix());
		glm::vec3 ray_origin = glm::vec3(inverse * glm::vec4(ray.origin, 1.0f));
		glm::vec3 ray_direction = glm::vec3(inverse * glm::vec4(ray.direction, 0.0f));

		for (int i = 0; i < indices.size() / 3; i++) {
			std::optional<glm::vec3> intersection = RayIntersectsTriangle({ ray_origin,ray_direction }, { vertices[indices[i * 3]].Position, vertices[indices[i * 3 + 1]].Position,vertices[indices[i * 3 + 2]].Position });
			if (intersection.has_value()) {
				return true;
			}
		}
		return false;
	}

	std::optional<std::vector<glm::vec3>> RayIntersectsMesh(const Ray& ray, Mesh* mesh)
	{
		//USE THIS AFTER A CHEAPER CHECK
		std::vector<glm::vec3> intersections = {};

		glm::mat4 inverse = glm::inverse(t.GetMatrix());
		glm::vec3 ray_origin = glm::vec3(inverse * glm::vec4(ray.origin, 1.0f));
		glm::vec3 ray_direction = glm::vec3(inverse * glm::vec4(ray.direction, 0.0f));


		std::map<float, IntersectionData> sorted;

		for (int i = 0; i < indices.size() / 3; i++) {
			std::optional<glm::vec3> intersection = RayIntersectsTriangle({ ray_origin,ray_direction }, { vertices[indices[i * 3]].Position, vertices[indices[i * 3 + 1]].Position,vertices[indices[i * 3 + 2]].Position });
			if (intersection.has_value()) {
				sorted[Magnitude2(intersection.value() - ray_origin)] = { intersection.value(),i };

				//intersections.push_back(intersection.value());
			}
		}

		if (intersections.size() > 0) return intersections;
		else return {};
	}

public:

private:

};

#endif