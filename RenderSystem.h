#ifndef RENDER_SYSTEM
#define RENDER_SYSTEM
#include <iostream>
#include <vector>
#include <map>
const float skyboxVertices[] = {
    -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f, -1.0f,  1.0f
};
class Camera;
class Shader;
class FBO;
class VBO;
class VAO;
class Cubemap;
class Lighting;
class Renderable {
public:
    enum ShaderType {
        RigShader,
        MeshShader,
        ImageBoxShader,
        BoxShader,
        TextShader,
        ParticleShader
    };
    ShaderType shadertype = ShaderType::MeshShader;
    bool opaque = true;
    bool castshadow = false;
    Renderable();
    ~Renderable();
    void BindToRenderSystem();
    virtual void Render(Shader& ShaderProgram) {}
    virtual glm::vec3 GetWorldPosition() const { return glm::vec3(0.0f); }
};
class RenderSystem {
public:
    Lighting* lighting = nullptr;
    FBO* screenfbo = nullptr;
    FBO* shadowdepthmapfbo = nullptr;
    unsigned int depthMaptexture = 0;
    unsigned int textureColorbuffer = 0;
    unsigned int rbo = 0;
    int screenWidth = 0;
    int screenHeight = 0;
    unsigned int SHADOW_RESOLUTION = 0;
    VAO* quadVAO = nullptr;  VBO* quadVBO = nullptr;
    VAO* skyboxVAO = nullptr;  VBO* skyboxVBO = nullptr;
    Shader* MeshShadowShader = nullptr;
    Shader* RigShadowShader = nullptr;
    Shader* SkyboxShader = nullptr;
    Shader* ScreenShader = nullptr;
    Shader* RigShader = nullptr;
    Shader* MeshShader = nullptr;
    Shader* ImageBoxShader = nullptr;
    Shader* BoxShader = nullptr;
    Shader* Text2DShader = nullptr;
    Shader* ParticleShader = nullptr;
    Cubemap* skybox = nullptr;
    RenderSystem() {}
    void Initialize();
    // Re-allocates the screen framebuffer's colour/depth attachments after a window resize.
    void ResizeScreen(int width, int height);
    void Render(Camera& camera);
    void AddRenderable(Renderable* r) { renderables.push_back(r); }
    bool quietRemovals = false;  // suppress the warning while bulk-loading models
    void RemoveRenderable(Renderable* r) {
        for (int i = 0; i < (int)renderables.size(); i++) {
            if (renderables[i] == r) { renderables.erase(renderables.begin() + i); return; }
        }
        if (!quietRemovals) std::cout << "didn't find renderable to remove\n";
    }
    void RenderRenderable(Renderable* renderable, Camera& camera);
    std::vector<Renderable*> renderables;
private:
    void PrepareRigShader(Camera& camera);
    void PrepareMeshShader(Camera& camera);
    void PrepareParticleShader(Camera& camera);
    void PrepareBoxShader(Camera& camera);
    void PrepareImageBoxShader(Camera& camera);
    void PrepareTextShader(Camera& camera);
    void RenderTranslucent(Camera& camera, const glm::mat4& lightSpaceMatrix, const glm::vec3& camPos);
};
#endif