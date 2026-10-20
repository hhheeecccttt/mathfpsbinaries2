#ifndef MODEL_CLASS
#define MODEL_CLASS
//DEPRECATED
#include <iostream>
#include <random>
#include <vector>

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "RenderSystem.h"
#include "Object.h"
#include "VAO.h"
#include "VBO.h"
#include "EBO.h"
#include "shaderClass.h"
#include "t.h"
#include "Vertex.h"
#include "Mesh.h"
#include "Physics.h"

class Model : public Object, public Renderable, public t_package {
public:
	Model(const char* path) {
		LoadModel(path);
		//NormalizeVertices();
	}
	
	std::vector<Mesh*> ReturnMeshes() {
		return meshes;
	}
	
	float GetVolume() {
		float runningtotal = 0.0f;
		glm::vec3 modelscale = t.GetScale();
		for (const Mesh* mesh : meshes) {
			runningtotal += mesh->GetVolume() * modelscale.x * modelscale.y * modelscale.z;
		}
		return runningtotal;
	}

	void Render(Shader& shader) override
	{	
		glm::mat4 matrix = t.GetMatrix();
		for (unsigned int i = 0; i < meshes.size(); i++) {
			meshes[i]->Render(shader);
		}
	}
	void RenderWireframe(Shader& shader)
	{
		for (unsigned int i = 0; i < meshes.size(); i++) {
			meshes[i]->RenderWireframe(shader);
		}
	}

	~Model() {
		for (Mesh* mesh : meshes) {
			mesh->~Mesh();
		}
	}

	std::vector<Mesh*> meshes;
	void NormalizeVertices() {
		float biggestx;
		float smallestx;
		float biggesty;
		float smallesty;
		float biggestz;
		float smallestz;
		bool initalized = false;
		for (Mesh* mesh : meshes) {
			for (Vertex& vertex : mesh->vertices) {
				if (not initalized) {
					biggestx = vertex.Position.x;
					smallestx = vertex.Position.x;
					biggesty = vertex.Position.y;
					smallesty = vertex.Position.y;
					biggestz = vertex.Position.z;
					smallestz = vertex.Position.z;

					initalized = true;
				}
				else {
					biggestx = std::max(biggestx, vertex.Position.x);
					smallestx = std::min(smallestx, vertex.Position.x);
					biggesty = std::max(biggesty, vertex.Position.y);
					smallesty = std::min(smallesty, vertex.Position.y);
					biggestz = std::max(biggestz, vertex.Position.z);
					smallestz = std::min(smallestz, vertex.Position.z);
				}
			}
		}

		if (not initalized) {
			std::cout << "NO VERTICES";
		}
		else {
			t.ScaleBy(glm::vec3(biggestx-smallestx,biggesty-smallesty,biggestz-smallestz));
			float midx = (biggestx + smallestx) / 2.0f;
			float midy = (biggesty + smallesty) / 2.0f;
			float midz = (biggestz + smallestz) / 2.0f;
			float xf = biggestx - smallestx;
			float yf = biggesty - smallesty;
			float zf = biggestz - smallestz;

			for (Mesh* mesh : meshes) {
				for (Vertex& vertex : mesh->vertices) {
					vertex.Position = glm::vec3(
						(vertex.Position.x - midx)/xf,
						(vertex.Position.y - midy)/yf,
						(vertex.Position.z - midz)/zf
					);
				}
				mesh->UpdateVertices();
			}
		
		}

	}
private:
	//IMPORT STUFF
	std::string directory;

	void LoadModel(std::string path) {
		Assimp::Importer import;
		const aiScene* scene = import.ReadFile(path, aiProcess_Triangulate);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << '\n';
			return;
		}
		directory = path.substr(0, path.find_last_of('/'));

		processNode(scene->mRootNode, scene);
	}

	void processNode(aiNode* node, const aiScene* scene) {
		// process all the node's meshes (if any)
		for (unsigned int i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			meshes.push_back(processMesh(mesh, scene));
		}
		// then do the same for each of its children
		for (unsigned int i = 0; i < node->mNumChildren; i++)
		{
			processNode(node->mChildren[i], scene);
		}
	};

	Mesh* processMesh(aiMesh* mesh, const aiScene* scene)
	{
		std::vector<Vertex> vertices;
		std::vector<unsigned int> indices;

		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			Vertex vertex;

			glm::vec3 vector;
			vector.x = mesh->mVertices[i].x;
			vector.y = mesh->mVertices[i].y;
			vector.z = mesh->mVertices[i].z;
			vertex.Position = vector;

			vector.x = mesh->mNormals[i].x;
			vector.y = mesh->mNormals[i].y;
			vector.z = mesh->mNormals[i].z;
			vertex.Normal = vector;

			// process vertex positions, normals and texture coordinates

			if (mesh->mTextureCoords[0]) // does the mesh contain texture coordinates?
			{
				glm::vec2 vec;
				vec.x = mesh->mTextureCoords[0][i].x;
				vec.y = mesh->mTextureCoords[0][i].y;
				vertex.TexCoords = vec;
			}
			else {
				vertex.TexCoords = glm::vec2(0.0f, 0.0f);
			}

			vertices.push_back(vertex);
		}
		// process indices
		for (unsigned int i = 0; i < mesh->mNumFaces; i++)
		{
			aiFace face = mesh->mFaces[i];
			for (unsigned int j = 0; j < face.mNumIndices; j++)
				indices.push_back(face.mIndices[j]);
		}

		return new Mesh(vertices, indices);
	}
};

#endif