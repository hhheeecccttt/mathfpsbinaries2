#ifndef RIG_CLASS
#define RIG_CLASS

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include<assimp/quaternion.h>
#include<assimp/vector3.h>
#include<assimp/matrix4x4.h>

#include<glm/gtc/quaternion.hpp>

#include "Mesh.h"

class AssimpGLMHelpers
{
public:

	static inline glm::mat4 ConvertMatrixToGLMFormat(const aiMatrix4x4& from)
	{
		glm::mat4 to;
		//the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
		to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
		to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
		to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
		to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
		return to;
	}

	static inline glm::vec3 GetGLMVec(const aiVector3D& vec)
	{
		return glm::vec3(vec.x, vec.y, vec.z);
	}

	static inline glm::quat GetGLMQuat(const aiQuaternion& pOrientation)
	{
		return glm::quat(pOrientation.w, pOrientation.x, pOrientation.y, pOrientation.z);
	}
};

struct BoneInfo
{
	/*id is index in finalBoneMatrices*/
	int id;

	/*offset matrix transforms vertex from model space to bone space*/
	glm::mat4 offset;

};

struct KeyPosition
{
	glm::vec3 position;
	float timeStamp;
};

struct KeyRotation
{
	glm::quat orientation;
	float timeStamp;
};

struct KeyScale
{
	glm::vec3 scale;
	float timeStamp;
};

class Bone
{
private:
	std::vector<KeyPosition> m_Positions;
	std::vector<KeyRotation> m_Rotations;
	std::vector<KeyScale> m_Scales;
	int m_NumPositions;
	int m_NumRotations;
	int m_NumScalings;

	glm::mat4 m_LocalTransform;
	std::string m_Name;
	int m_ID;

public:

	/*reads keyframes from aiNodeAnim*/
	Bone(const std::string& name, int ID, const aiNodeAnim* channel)
		:
		m_Name(name),
		m_ID(ID),
		m_LocalTransform(1.0f)
	{
		m_NumPositions = channel->mNumPositionKeys;

		for (int positionIndex = 0; positionIndex < m_NumPositions; ++positionIndex)
		{
			aiVector3D aiPosition = channel->mPositionKeys[positionIndex].mValue;
			float timeStamp = channel->mPositionKeys[positionIndex].mTime;
			KeyPosition data;
			data.position = AssimpGLMHelpers::GetGLMVec(aiPosition);
			data.timeStamp = timeStamp;
			m_Positions.push_back(data);
		}

		m_NumRotations = channel->mNumRotationKeys;
		for (int rotationIndex = 0; rotationIndex < m_NumRotations; ++rotationIndex)
		{
			aiQuaternion aiOrientation = channel->mRotationKeys[rotationIndex].mValue;
			float timeStamp = channel->mRotationKeys[rotationIndex].mTime;
			KeyRotation data;
			data.orientation = AssimpGLMHelpers::GetGLMQuat(aiOrientation);
			data.timeStamp = timeStamp;
			m_Rotations.push_back(data);
		}

		m_NumScalings = channel->mNumScalingKeys;
		for (int keyIndex = 0; keyIndex < m_NumScalings; ++keyIndex)
		{
			aiVector3D scale = channel->mScalingKeys[keyIndex].mValue;
			float timeStamp = channel->mScalingKeys[keyIndex].mTime;
			KeyScale data;
			data.scale = AssimpGLMHelpers::GetGLMVec(scale);
			data.timeStamp = timeStamp;
			m_Scales.push_back(data);
		}
	}

	/*interpolates  b/w positions,rotations & scaling keys based on the curren time of
	the animation and prepares the local transformation matrix by combining all keys
	tranformations*/
	void Update(float animationTime)
	{
		glm::mat4 translation = InterpolatePosition(animationTime);
		glm::mat4 rotation = InterpolateRotation(animationTime);
		glm::mat4 scale = InterpolateScaling(animationTime);
		m_LocalTransform = translation * rotation;// *scale;
	}

	glm::mat4 GetLocalTransform() { return m_LocalTransform; }
	std::string GetBoneName() const { return m_Name; }
	int GetBoneID() { return m_ID; }


	/* Gets the current index on mKeyPositions to interpolate to based on
	the current animation time*/
	int GetPositionIndex(float animationTime)
	{
		for (int index = 0; index < m_NumPositions - 1; ++index)
		{
			if (animationTime < m_Positions[index + 1].timeStamp)
				return index;
		}
		assert(0);
	}

	/* Gets the current index on mKeyRotations to interpolate to based on the
	current animation time*/
	int GetRotationIndex(float animationTime)
	{
		for (int index = 0; index < m_NumRotations - 1; ++index)
		{
			if (animationTime < m_Rotations[index + 1].timeStamp)
				return index;
		}
		assert(0);
	}

	/* Gets the current index on mKeyScalings to interpolate to based on the
	current animation time */
	int GetScaleIndex(float animationTime)
	{
		for (int index = 0; index < m_NumScalings - 1; ++index)
		{
			if (animationTime < m_Scales[index + 1].timeStamp)
				return index;
		}
		assert(0);
	}

private:

	/* Gets normalized value for Lerp & Slerp*/
	float GetScaleFactor(float lastTimeStamp, float nextTimeStamp, float animationTime)
	{
		float scaleFactor = 0.0f;
		float midWayLength = animationTime - lastTimeStamp;
		float framesDiff = nextTimeStamp - lastTimeStamp;
		scaleFactor = midWayLength / framesDiff;
		return scaleFactor;
	}

	/*figures out which position keys to interpolate b/w and performs the interpolation
	and returns the translation matrix*/
	glm::mat4 InterpolatePosition(float animationTime)
	{
		if (1 == m_NumPositions)
			return glm::translate(glm::mat4(1.0f), m_Positions[0].position);

		int p0Index = GetPositionIndex(animationTime);
		int p1Index = p0Index + 1;
		float scaleFactor = GetScaleFactor(m_Positions[p0Index].timeStamp,
			m_Positions[p1Index].timeStamp, animationTime);
		glm::vec3 finalPosition = glm::mix(m_Positions[p0Index].position,
			m_Positions[p1Index].position, scaleFactor);
		return glm::translate(glm::mat4(1.0f), finalPosition);
	}

	/*figures out which rotations keys to interpolate b/w and performs the interpolation
	and returns the rotation matrix*/
	glm::mat4 InterpolateRotation(float animationTime)
	{
		if (1 == m_NumRotations)
		{
			auto rotation = glm::normalize(m_Rotations[0].orientation);
			return glm::mat4_cast(rotation);
		}

		int p0Index = GetRotationIndex(animationTime);
		int p1Index = p0Index + 1;
		float scaleFactor = GetScaleFactor(m_Rotations[p0Index].timeStamp,
			m_Rotations[p1Index].timeStamp, animationTime);
		glm::quat finalRotation = glm::slerp(m_Rotations[p0Index].orientation,
			m_Rotations[p1Index].orientation, scaleFactor);
		finalRotation = glm::normalize(finalRotation);
		return glm::mat4_cast(finalRotation);
	}

	/*figures out which scaling keys to interpolate b/w and performs the interpolation
	and returns the scale matrix*/
	glm::mat4 InterpolateScaling(float animationTime)
	{
		if (1 == m_NumScalings)
			return glm::scale(glm::mat4(1.0f), m_Scales[0].scale);

		int p0Index = GetScaleIndex(animationTime);
		int p1Index = p0Index + 1;
		float scaleFactor = GetScaleFactor(m_Scales[p0Index].timeStamp,
			m_Scales[p1Index].timeStamp, animationTime);
		glm::vec3 finalScale = glm::mix(m_Scales[p0Index].scale, m_Scales[p1Index].scale
			, scaleFactor);
		return glm::scale(glm::mat4(1.0f), finalScale);
	}

};

class RigMesh : public Object, public Renderable, public t_package, public RigMeshData {
	friend class Model;
protected:
public:
	VAO vao;
	VBO vbo;
	EBO ebo;

	RigMesh() {

	};

	RigMesh(const std::vector<RigVertex>& meshvertices, const std::vector<GLuint>& meshindices/*, std::vector<Texture> textures*/)
	{
		InitializeMesh(meshvertices, meshindices);
	};

	void InitializeMesh(const std::vector<RigVertex>& meshvertices, const std::vector<GLuint>& meshindices) {
		vertices = meshvertices;
		indices = meshindices;
		this->shadertype = MeshShader;
		GenerateRenderData();
		//NormalizeVertices();
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
			ebo.Bind();
			vbo.BufferData(&vertices[0], vertices.size() * sizeof(RigVertex));
			ebo.BufferData(&indices[0], indices.size() * sizeof(GLuint));

			vbo.Bind();
			ebo.Bind();

			vao.LinkVBO(vbo, 0, 3, GL_FLOAT, sizeof(RigVertex), (void*)0);
			vao.LinkVBO(vbo, 1, 3, GL_FLOAT, sizeof(RigVertex), (void*)offsetof(RigVertex, Normal));
			vao.LinkVBO(vbo, 2, 2, GL_FLOAT, sizeof(RigVertex), (void*)offsetof(RigVertex, TexCoords));
			vbo.Bind();
			glEnableVertexAttribArray(3);
			glVertexAttribIPointer(3, 4, GL_INT, sizeof(RigVertex), (void*)offsetof(RigVertex, m_BoneIDs));

			glEnableVertexAttribArray(4);
			glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(RigVertex), (void*)offsetof(RigVertex, m_Weights));

			vao.Unbind();
			vbo.Unbind();
			ebo.Unbind();
		}
	}

	virtual ~RigMesh() {
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

	virtual RigMesh* Clone() override {
		RigMesh* tr = new RigMesh(*this);
		tr->GenerateRenderData();
		return tr;
	};

	void Clear() {
		vertices = std::vector<RigVertex>();
		indices = std::vector<GLuint>();
	}

	glm::vec3 GetAABB() const {
		if (vertices.empty()) {
			return glm::vec3(0.0f);
		}

		glm::vec3 min(FLT_MAX);
		glm::vec3 max(-FLT_MAX);

		for (const RigVertex& vertex : vertices) {
			min = glm::min(min, vertex.Position);
			max = glm::max(max, vertex.Position);
		}

		return (max - min) * t.GetScale();
	}

	void SetVertices(std::vector<RigVertex>& e) {
		glBufferSubData(GL_ARRAY_BUFFER, NULL, sizeof(e), &e[0]);

		glBindBuffer(GL_ARRAY_BUFFER, vbo.ID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(e), &e[0], GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void UpdateVertices() {
		if (vertices.size() > 0) {

			glBufferSubData(GL_ARRAY_BUFFER, NULL, sizeof(vertices), &vertices[0]);
			glBindBuffer(GL_ARRAY_BUFFER, vbo.ID);
			glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(RigVertex), &vertices[0], GL_STATIC_DRAW);
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
	/*
	void NormalizeVertices() {
		if (vertices.size() == 0) {
			std::cout << "TRIED TO NORMALIZE ZERO VERTICES???\n";
			throw;
		}
		float biggestx = -FLT_MAX;
		float smallestx = FLT_MAX;
		float biggesty = -FLT_MAX;
		float smallesty = FLT_MAX;
		float biggestz = -FLT_MAX;
		float smallestz = FLT_MAX;
		for (RigVertex& vertex : vertices) {
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

		t.ScaleBy(glm::vec3(xf, yf, zf));
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

		for (RigVertex& vertex : vertices) {
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
	*/
	void Render(Shader& ShaderProgram, glm::mat4 modelmat) {
		glm::mat4 topass = modelmat * t.GetMatrix();
		ShaderProgram.SetMat4("modl", topass);
		vao.Bind();
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
		vao.Unbind();
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
	//random ass funcs

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

class Rig : public Renderable, public t_package {
public:
	std::vector<RigMesh*> meshes;
	std::vector<Bone> bones;
public:
	std::map<std::string, BoneInfo> m_BoneInfoMap;
	int m_BoneCounter = 0;
	std::string directory;

	Rig(std::string path) {
		shadertype = RigShader;
		LoadRig(path);
	}

	void Render(Shader& shader) override {
		shader.Activate();

		glm::mat4 rigmat = t.GetMatrix();
		for (auto p : meshes) {
			p->Render(shader, rigmat);
		}
	}

	void Render(Shader& shader, glm::mat4 mat) {
		shader.Activate();

		glm::mat4 rigmat = t.GetMatrix();
		for (auto p : meshes) {
			p->Render(shader, mat);
		}
	}

	void LoadRig(std::string path) {
		Assimp::Importer import;
		const aiScene* scene = import.ReadFile(path,
			aiProcess_Triangulate |
			aiProcess_FlipUVs
		);

		if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
		{
			std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << '\n';
			return;
		}
		directory = path.substr(0, path.find_last_of('/'));

		processNode(scene->mRootNode, scene);
	}
private:
	void processNode(aiNode* node, const aiScene* scene) {
		// process all the node's meshes (if any)
		for (unsigned int i = 0; i < node->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
			meshes.push_back(processRigMesh(mesh, scene));
		}
		// then do the same for each of its children
		for (unsigned int i = 0; i < node->mNumChildren; i++)
		{
			processNode(node->mChildren[i], scene);
		}
	};

	RigMesh* processRigMesh(aiMesh* mesh, const aiScene* scene)
	{
		std::vector<RigVertex> vertices;
		std::vector<unsigned int> indices;

		for (unsigned int i = 0; i < mesh->mNumVertices; i++)
		{
			RigVertex vertex;

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

			for (int i = 0; i < MAX_BONE_INFLUENCE; i++)
			{
				vertex.m_BoneIDs[i] = -1;
				vertex.m_Weights[i] = 0.0f;
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

		ExtractBoneWeightForVertices(vertices, mesh, scene);

		return new RigMesh(vertices, indices);
	}

	void SetVertexBoneData(RigVertex& vertex, int boneID, float weight)
	{
		for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
		{
			if (vertex.m_BoneIDs[i] < 0)
			{
				vertex.m_Weights[i] = weight;
				vertex.m_BoneIDs[i] = boneID;
				break;
			}
		}
	}


	void ExtractBoneWeightForVertices(std::vector<RigVertex>& vertices, aiMesh* mesh, const aiScene* scene)
	{
		auto& boneInfoMap = m_BoneInfoMap;
		int& boneCount = m_BoneCounter;

		for (int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex)
		{
			int boneID = -1;
			std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();
			if (boneInfoMap.find(boneName) == boneInfoMap.end())
			{
				BoneInfo newBoneInfo;
				newBoneInfo.id = boneCount;
				newBoneInfo.offset = AssimpGLMHelpers::ConvertMatrixToGLMFormat(mesh->mBones[boneIndex]->mOffsetMatrix);
				boneInfoMap[boneName] = newBoneInfo;
				boneID = boneCount;
				boneCount++;
			}
			else
			{
				boneID = boneInfoMap[boneName].id;
			}
			assert(boneID != -1);
			auto weights = mesh->mBones[boneIndex]->mWeights;
			int numWeights = mesh->mBones[boneIndex]->mNumWeights;

			for (int weightIndex = 0; weightIndex < numWeights; ++weightIndex)
			{
				int vertexId = weights[weightIndex].mVertexId;
				float weight = weights[weightIndex].mWeight;
				assert(vertexId <= vertices.size());
				SetVertexBoneData(vertices[vertexId], boneID, weight);
			}
		}
	}

};

#endif