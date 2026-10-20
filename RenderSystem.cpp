#include "Debug.h"
#include "RenderSystem.h"
#include "common.h"
#include "VBO.h"
#include "VAO.h"
#include "EBO.h"
#include "FBO.h"
#include "shaderClass.h"
#include "Object.h"
#include "Camera.h"
#include "Cubemap.h"
#include "QuadVertices.h"
#include "Lighting.h"
#include "Animator.h"

// ============================================================================
// Initialize
// ============================================================================
void RenderSystem::Initialize()
{
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, window->width, window->height);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Screen quad
    screenfbo = new FBO();
    skyboxVAO = new VAO();
    skyboxVBO = new VBO();
    quadVAO = new VAO();
    quadVBO = new VBO();

    skyboxVAO->GenerateID();
    skyboxVBO->GenerateID();
    skyboxVAO->Bind();
    skyboxVBO->Bind();
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    quadVAO->GenerateID();
    quadVBO->GenerateID();
    quadVAO->Bind();
    quadVBO->Bind();
    quadVBO->BufferData(quadVertices, sizeof(quadVertices));
    quadVAO->LinkVBO(*quadVBO, 0, 2, GL_FLOAT, 4 * sizeof(float), (void*)0);
    quadVAO->LinkVBO(*quadVBO, 1, 2, GL_FLOAT, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    std::vector<std::string> faces = {
        "assets/miramar_ft.tga", "assets/miramar_bk.tga",
        "assets/miramar_up.tga", "assets/miramar_dn.tga",
        "assets/miramar_rt.tga", "assets/miramar_lf.tga"
    };
    Cubemap* skyboxcm = new Cubemap();
    skyboxcm->FillCubemap(faces);
    skybox = skyboxcm;

    // Screen framebuffer
    screenfbo->Bind();
    glGenTextures(1, &textureColorbuffer);
    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, window->width, window->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);

    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, window->width, window->height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);
    screenWidth = window->width;
    screenHeight = window->height;

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n";
    screenfbo->Unbind();

    // Shadow framebuffer
    shadowdepthmapfbo = new FBO();
    SHADOW_RESOLUTION = 4096;
    glGenTextures(1, &depthMaptexture);
    glBindTexture(GL_TEXTURE_2D, depthMaptexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
        SHADOW_RESOLUTION, SHADOW_RESOLUTION, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    shadowdepthmapfbo->Bind();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMaptexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    lighting = new Lighting();
}

// ============================================================================
// ResizeScreen
// The scene renders into an offscreen framebuffer; it has to match the window
// or the final blit is stretched/cropped after a resize.
// ============================================================================
void RenderSystem::ResizeScreen(int width, int height)
{
    if (width <= 0 || height <= 0) return;
    if (textureColorbuffer == 0 || rbo == 0) return;  // not initialized yet
    if (width == screenWidth && height == screenHeight) return;
    screenWidth = width;
    screenHeight = height;

    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

// ============================================================================
// RenderTranslucent
// Collects all non-opaque 3D renderables (mesh + particle), sorts back-to-front,
// then renders each with the correct shader. Blending stays on throughout.
// ============================================================================
void RenderSystem::RenderTranslucent(Camera& camera, const glm::mat4& lightSpaceMatrix,
    const glm::vec3& camPos)
{
    // Collect translucent 3D renderables
    std::vector<std::pair<float, Renderable*>> sorted;
    for (Renderable* r : renderables) {
        if (r->opaque) continue;
        if (r->shadertype != Renderable::MeshShader &&
            r->shadertype != Renderable::ParticleShader &&
            r->shadertype != Renderable::RigShader) continue;

        glm::vec3 pos = r->GetWorldPosition();
        float     dist = glm::length(pos - camPos);
        sorted.push_back({ dist, r });
    }

    // Sort back-to-front
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);   // write to color but not depth � prevents z-fighting between translucents

    Renderable::ShaderType lastShader = Renderable::ShaderType::MeshShader;
    bool shaderSet = false;

    for (auto& [dist, r] : sorted) {
        // Only re-bind shader when it changes
        if (!shaderSet || r->shadertype != lastShader) {
            if (r->shadertype == Renderable::MeshShader) {
                PrepareMeshShader(camera);
                MeshShader->SetMat4("lightSpaceMatrix", lightSpaceMatrix);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, depthMaptexture);
                MeshShader->Set3F("dirLight.ambient", glm::vec3(0.3f));
                MeshShader->Set3F("dirLight.diffuse", glm::vec3(0.4f));
                MeshShader->Set3F("dirLight.specular", glm::vec3(0.3f));
                MeshShader->Set3F("dirLight.direction", lighting->dirlighting.GetFrontVector());
            }
            else if (r->shadertype == Renderable::ParticleShader) {
                PrepareParticleShader(camera);
            }
            else if (r->shadertype == Renderable::RigShader) {
                PrepareRigShader(camera);
                RigShader->SetMat4("lightSpaceMatrix", lightSpaceMatrix);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, depthMaptexture);
                RigShader->Set3F("dirLight.ambient", glm::vec3(0.3f));
                RigShader->Set3F("dirLight.diffuse", glm::vec3(0.4f));
                RigShader->Set3F("dirLight.specular", glm::vec3(0.3f));
                RigShader->Set3F("dirLight.direction", lighting->dirlighting.GetFrontVector());
            }
            lastShader = r->shadertype;
            shaderSet = true;
        }

        Shader* sh = (r->shadertype == Renderable::MeshShader) ? MeshShader
            : (r->shadertype == Renderable::ParticleShader) ? ParticleShader
            : RigShader;
        r->Render(*sh);
    }

    glDepthMask(GL_TRUE);   // restore depth writing for subsequent passes
}

// ============================================================================
// Render
// ============================================================================
void RenderSystem::Render(Camera& camera)
{
    lighting->dirlighting.RotateByEulerAngles({ glm::radians(0.05f), 0.0f, 0.0f });

    const glm::mat4 proj = camera.GetProjectionMatrix(0.05f, 2000.0f);
    const glm::mat4 view = camera.GetViewMatrix();
    const glm::vec3 camPos = camera.t.GetTranslation();

    const float near_plane = 0.1f, far_plane = 100.0f;
    const glm::mat4 lightSpaceMatrix =
        glm::ortho(-32.0f, 32.0f, -32.0f, 32.0f, near_plane, far_plane)
        * glm::lookAt(camPos - lighting->dirlighting.GetFrontVector() * 10.0f,
            camPos, camera.t.GetUpVector());

    // ---- Shadow pass ----
    glCullFace(GL_FRONT);
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, SHADOW_RESOLUTION, SHADOW_RESOLUTION);
    shadowdepthmapfbo->Bind();
    glClear(GL_DEPTH_BUFFER_BIT);

    MeshShadowShader->Activate();
    MeshShadowShader->SetMat4("lightSpaceMatrix", lightSpaceMatrix);
    for (Renderable* r : renderables) {
        if (!r->castshadow || r->shadertype != Renderable::MeshShader) continue;
        r->Render(*MeshShadowShader);
    }

    RigShadowShader->Activate();
    RigShadowShader->SetMat4("lightSpaceMatrix", lightSpaceMatrix);
    for (Renderable* r : renderables) {
        if (!r->castshadow || r->shadertype != Renderable::RigShader) continue;
        r->Render(*RigShadowShader);
    }

    glCullFace(GL_BACK);

    // ---- Main scene ----
    screenfbo->Bind();
    glViewport(0, 0, window->width, window->height);
    glEnable(GL_DEPTH_TEST);
    glClearColor(212.f / 255.f, 223.f / 255.f, 232.f / 255.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Skybox
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    SkyboxShader->Activate();
    SkyboxShader->SetMat4("proj", proj);
    SkyboxShader->SetMat4("view", glm::mat4(glm::mat3(view)));
    skyboxVAO->Bind();
    glActiveTexture(GL_TEXTURE0);
    skybox->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);

    // ---- Opaque meshes ----
    PrepareMeshShader(camera);
    MeshShader->SetMat4("lightSpaceMatrix", lightSpaceMatrix);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthMaptexture);
    MeshShader->Set3F("dirLight.ambient", glm::vec3(0.3f));
    MeshShader->Set3F("dirLight.diffuse", glm::vec3(0.4f));
    MeshShader->Set3F("dirLight.specular", glm::vec3(0.3f));
    MeshShader->Set3F("dirLight.direction", lighting->dirlighting.GetFrontVector());
    for (Renderable* r : renderables) {
        if (r->shadertype == Renderable::MeshShader && r->opaque)
            r->Render(*MeshShader);
    }

    // ---- Opaque rigs ----
    PrepareRigShader(camera);
    RigShader->SetMat4("lightSpaceMatrix", lightSpaceMatrix);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthMaptexture);
    RigShader->Set3F("dirLight.ambient", glm::vec3(0.3f));
    RigShader->Set3F("dirLight.diffuse", glm::vec3(0.4f));
    RigShader->Set3F("dirLight.specular", glm::vec3(0.3f));
    RigShader->Set3F("dirLight.direction", lighting->dirlighting.GetFrontVector());
    for (Renderable* r : renderables) {
        if (r->shadertype == Renderable::RigShader && r->opaque)
            r->Render(*RigShader);
    }

    // ---- Translucent pass (meshes + particles + rigs, sorted back-to-front) ----
    // Disable face culling so thin translucent quads render both sides
    glDisable(GL_CULL_FACE);
    RenderTranslucent(camera, lightSpaceMatrix, camPos);
    glEnable(GL_CULL_FACE);

    // ---- Framebuffer blit to screen ----
    screenfbo->Unbind();
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ScreenShader->Activate();
    quadVAO->Bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // ---- GUI (always on top, no depth) ----
    PrepareBoxShader(camera);
    for (Renderable* r : renderables) {
        if (r->shadertype == Renderable::BoxShader)
            r->Render(*BoxShader);
    }

    PrepareImageBoxShader(camera);
    for (Renderable* r : renderables) {
        if (r->shadertype == Renderable::ImageBoxShader)
            r->Render(*ImageBoxShader);
    }

    PrepareTextShader(camera);
    for (Renderable* r : renderables) {
        if (r->shadertype == Renderable::TextShader)
            r->Render(*Text2DShader);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glfwSwapBuffers(window->handle);
}

// ============================================================================
// Shader preparation helpers
// ============================================================================
void RenderSystem::RenderRenderable(Renderable* renderable, Camera& camera)
{
    Shader* shader = (renderable->shadertype == Renderable::MeshShader) ? MeshShader
        : (renderable->shadertype == Renderable::ParticleShader) ? ParticleShader
        : RigShader;
    shader->Activate();
    renderable->Render(*shader);
}

void RenderSystem::PrepareMeshShader(Camera& camera)
{
    MeshShader->Activate();
    MeshShader->SetMat4("proj", camera.GetProjectionMatrix(0.05f, 2000.0f));
    MeshShader->SetMat4("view", camera.GetViewMatrix());
    MeshShader->Set3F("viewPos", camera.t.GetTranslation());
}

void RenderSystem::PrepareRigShader(Camera& camera)
{
    RigShader->Activate();
    RigShader->SetMat4("proj", camera.GetProjectionMatrix(0.05f, 2000.0f));
    RigShader->SetMat4("view", camera.GetViewMatrix());
    RigShader->Set3F("viewPos", camera.t.GetTranslation());
}

void RenderSystem::PrepareParticleShader(Camera& camera)
{
    ParticleShader->Activate();
    ParticleShader->SetMat4("proj", camera.GetProjectionMatrix(0.05f, 2000.0f));
    ParticleShader->SetMat4("view", camera.GetViewMatrix());
}

void RenderSystem::PrepareBoxShader(Camera& camera)
{
    BoxShader->Activate();
    BoxShader->Set2F("screenSize", { (float)window->width, (float)window->height });
}

void RenderSystem::PrepareImageBoxShader(Camera& camera)
{
    ImageBoxShader->Activate();
    ImageBoxShader->Set2F("screenSize", { (float)window->width, (float)window->height });
}

void RenderSystem::PrepareTextShader(Camera& camera)
{
    Text2DShader->Activate();
    Text2DShader->Set2F("screenSize", { (float)window->width, (float)window->height });
}

// ============================================================================
// Renderable base
// ============================================================================
Renderable::Renderable() {}

Renderable::~Renderable()
{
    engine->rendersystem->RemoveRenderable(this);
}

void Renderable::BindToRenderSystem()
{
    engine->rendersystem->AddRenderable(this);
}