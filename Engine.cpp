#include "Engine.h"

// ============================================================================
// Standard library
// ============================================================================
#include <iostream>
#include <vector>
#include <random>
#include <map>
#include <string>
#include <memory>
#include <cfloat>
#include <type_traits>

// ============================================================================
// OpenGL
// ============================================================================
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// ============================================================================
// Math
// ============================================================================
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

// ============================================================================
// Third party
// ============================================================================
#include <ft2build.h>
#include FT_FREETYPE_H

// ============================================================================
// Engine � core
// ============================================================================
#include "common.h"
#include "Window.h"
#include "Camera.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "Physics.h"
#include "RenderSystem.h"
#include "SoundSystem.h"
#include "ResourceManager.h"
#include "tFunctions.h"
#include "Debug.h"

// Rendering
#include "shaderClass.h"
#include "VBO.h"
#include "VAO.h"
#include "EBO.h"
#include "FBO.h"
#include "texture.h"
#include "Material.h"
#include "Mesh.h"
#include "Model.h"
#include "Cubemap.h"
#include "QuadVertices.h"
#include "Particle.h"
#include "Lighting.h"
#include "DirLight.h"

// Scene
#include "Folder.h"
#include "Sound.h"
#include "Gui.h"
#include "Net.h"
#include "font.h"
#include "Log.h"

// Character / animation
#include "Rig.h"
#include "Animator.h"
#include "Character.h"
#include "DefaultBodyComponents.h"
#include "BodyComponentActions.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// ============================================================================
// ComputeOrientedBox
// Some box-shaped pieces in the level are rotated (the slanted rail sections of
// 2fort). The loader bakes geometry into world space and normalises it, so the
// tight box has to be rebuilt from the faces: centre, size and rotation.
// Returns false for anything that is not exactly one box (those keep an AABB).
// The result is in model space, i.e. before the fort's uniform fortScale.
// ============================================================================
static bool ComputeOrientedBox(const Mesh* mesh, glm::vec3& centre, glm::vec3& size, glm::quat& rotation)
{
    // Undo the loader's per-axis normalisation so the face directions are the real ones.
    const glm::mat4 m = mesh->t.GetMatrix();
    std::vector<glm::vec3> p;
    p.reserve(mesh->vertices.size());
    for (const Vertex& v : mesh->vertices)
        p.push_back(glm::vec3(m * glm::vec4(v.Position, 1.0f)));

    // A box has 8 distinct corners...
    std::vector<glm::vec3> corners;
    for (const glm::vec3& v : p) {
        bool seen = false;
        for (const glm::vec3& c : corners)
            if (glm::distance(c, v) < 1e-3f) { seen = true; break; }
        if (!seen) corners.push_back(v);
    }
    if (corners.size() != 8) return false;

    // ...and exactly 3 distinct, mutually perpendicular face normals.
    std::vector<glm::vec3> axes;
    for (size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
        const glm::vec3 a = p[mesh->indices[i]];
        const glm::vec3 b = p[mesh->indices[i + 1]];
        const glm::vec3 c = p[mesh->indices[i + 2]];
        glm::vec3 n = glm::cross(b - a, c - a);
        if (glm::length(n) < 1e-8f) continue;
        n = glm::normalize(n);

        // Canonical sign so the two opposite faces dedupe to one axis.
        for (int k = 0; k < 3; ++k) {
            if (glm::abs(n[k]) > 1e-4f) { if (n[k] < 0.0f) n = -n; break; }
        }
        bool seen = false;
        for (const glm::vec3& ax : axes)
            if (glm::distance(ax, n) < 1e-3f) { seen = true; break; }
        if (!seen) axes.push_back(n);
    }
    if (axes.size() != 3) return false;
    for (int i = 0; i < 3; ++i)
        for (int j = i + 1; j < 3; ++j)
            if (glm::abs(glm::dot(axes[i], axes[j])) > 1e-3f) return false;

    // Right-handed basis for the rotation quaternion.
    glm::vec3 ax = axes[0], ay = axes[1], az = axes[2];
    if (glm::dot(glm::cross(ax, ay), az) < 0.0f) az = -az;

    // Project the corners onto the axes to get the tight centre and extents.
    const glm::vec3 basis[3] = { ax, ay, az };
    centre = glm::vec3(0.0f);
    for (int i = 0; i < 3; ++i) {
        float mn = FLT_MAX, mx = -FLT_MAX;
        for (const glm::vec3& v : p) {
            const float d = glm::dot(v, basis[i]);
            mn = glm::min(mn, d);
            mx = glm::max(mx, d);
        }
        size[i] = mx - mn;
        centre += basis[i] * ((mn + mx) * 0.5f);
    }
    rotation = glm::quat_cast(glm::mat3(ax, ay, az));
    return true;
}

// ============================================================================
// MapFaceUVs
// The 2fort export carries Blender's own UV layout, which spreads the test
// texture across islands. Instead give every face its own copy of the image:
// the four corners of the quad map to the four corners of the texture.
// The model is all quads and the importer triangulates each into two triangles
// in order, so every consecutive pair of triangles is restored to its quad.
// ============================================================================
static void MapFaceUVs(Mesh* mesh)
{
    // Work in real model space: the loader's per-axis normalisation would shear the faces.
    const glm::mat4 m = mesh->t.GetMatrix();
    std::vector<glm::vec3> p;
    p.reserve(mesh->vertices.size());
    for (const Vertex& v : mesh->vertices)
        p.push_back(glm::vec3(m * glm::vec4(v.Position, 1.0f)));

    for (size_t t = 0; t + 1 < mesh->indices.size() / 3; t += 2) {
        const GLuint ids[6] = {
            mesh->indices[t * 3 + 0], mesh->indices[t * 3 + 1], mesh->indices[t * 3 + 2],
            mesh->indices[t * 3 + 3], mesh->indices[t * 3 + 4], mesh->indices[t * 3 + 5]
        };

        // The quad's four distinct corners.
        glm::vec3 corner[4];
        int cornerCount = 0;
        for (GLuint id : ids) {
            bool seen = false;
            for (int k = 0; k < cornerCount; ++k)
                if (glm::distance(corner[k], p[id]) < 1e-3f) { seen = true; break; }
            if (!seen && cornerCount < 4) corner[cornerCount++] = p[id];
        }
        if (cornerCount != 4) continue;

        // Tangent frame on the face, right-handed with the face normal.
        glm::vec3 n = glm::cross(p[ids[1]] - p[ids[0]], p[ids[2]] - p[ids[0]]);
        if (glm::length(n) < 1e-8f) continue;
        n = glm::normalize(n);
        const glm::vec3 up = (glm::abs(n.y) < 0.9f) ? glm::vec3(0.0f, 1.0f, 0.0f)
                                                    : glm::vec3(0.0f, 0.0f, 1.0f);
        const glm::vec3 tu = glm::normalize(glm::cross(up, n));
        const glm::vec3 tv = glm::cross(n, tu);

        // Order the corners around the face centre (counter-clockwise seen from the front).
        const glm::vec3 c = (corner[0] + corner[1] + corner[2] + corner[3]) * 0.25f;
        int order[4] = { 0, 1, 2, 3 };
        float angle[4];
        for (int i = 0; i < 4; ++i) {
            const glm::vec3 d = corner[i] - c;
            angle[i] = std::atan2(glm::dot(d, tv), glm::dot(d, tu));
        }
        for (int i = 1; i < 4; ++i) {
            const int keep = order[i];
            const float a = angle[keep];
            int j = i - 1;
            while (j >= 0 && angle[order[j]] > a) { order[j + 1] = order[j]; --j; }
            order[j + 1] = keep;
        }

        // (0,0) -> (1,0) -> (1,1) -> (0,1): one full texture per quad.
        const glm::vec2 uv[4] = { {0,0}, {1,0}, {1,1}, {0,1} };
        for (GLuint id : ids) {
            for (int k = 0; k < 4; ++k)
                if (glm::distance(corner[order[k]], p[id]) < 1e-3f) {
                    mesh->vertices[id].TexCoords = uv[k];
                    break;
                }
        }
    }
}


// ============================================================================
// Constructor � window, callbacks, systems, shaders
// ============================================================================
Engine::Engine()
{
    engine = this;
    rendersystem = nullptr;

    glfwSetFramebufferSizeCallback(window->handle, framebuffer_size_callback);
    glfwSetCursorPosCallback(window->handle, mouse_callback);
    glfwSetScrollCallback(window->handle, scroll_callback);
    glfwSetKeyCallback(window->handle, key_callback);
    glfwSetMouseButtonCallback(window->handle, mouse_button_callback);
    glfwSwapInterval(1);

    // Start from the real framebuffer size (can differ from the requested size under DPI scaling)
    int fbw, fbh;
    glfwGetFramebufferSize(window->handle, &fbw, &fbh);
    framebuffer_size_callback(window->handle, fbw, fbh);

    rendersystem = new RenderSystem();
    rendersystem->Initialize();
    physicsengine = new Physics();
    physicsengine->SetGravity(physicsengine->GetGravity() * 1.25f);  // slightly stronger gravity to suit the larger map
    mainf = new Folder();
    camera = new Camera();
    camera->t.TranslateTo({ 0.0f, 5.0f, 0.0f });

    rendersystem->RigShader = new Shader("shaders/rig.vert", "shaders/default.frag");
    rendersystem->MeshShader = new Shader("shaders/default.vert", "shaders/default.frag");
    rendersystem->MeshShadowShader = new Shader("shaders/meshshadow.vert", "shaders/shadow.frag");
    rendersystem->RigShadowShader = new Shader("shaders/rigshadow.vert", "shaders/shadow.frag");
    rendersystem->SkyboxShader = new Shader("shaders/skybox.vert", "shaders/skybox.frag");
    rendersystem->ScreenShader = new Shader("shaders/ScreenShader.vert", "shaders/ScreenShader.frag");
    rendersystem->ImageBoxShader = new Shader("shaders/ImageBox.vert", "shaders/ImageBox.frag");
    rendersystem->BoxShader = new Shader("shaders/Box.vert", "shaders/Box.frag");
    rendersystem->Text2DShader = new Shader("shaders/2DText.vert", "shaders/2DText.frag");
    rendersystem->ParticleShader = new Shader("shaders/Particle.vert", "shaders/Particle.frag");
}

// ============================================================================
// Initiate � scene setup + game loop
// ============================================================================
void Engine::Initiate()
{
    // ------------------------------------------------------------------------
    // Assets
    // ------------------------------------------------------------------------
    Font* arial = new Font("assets/ft/arial.ttf");
    Texture* blanktex = new Texture("assets/blanktexture.png");
    Texture* magic = new Texture("assets/itsmagicbitch.jpg");
    Texture* snowflake = new Texture("assets/snowflake.png");
    Texture* shockwavetex = new Texture("assets/shockwave.png");
    Texture* leontex = new Texture("assets/revolvertex.png");
    Texture* orangetex = new Texture("assets/orangetex.png");  // every face of 2fort

    // ------------------------------------------------------------------------
    // Shared rig + animations
    // ------------------------------------------------------------------------
    Rig* testrig = new Rig("assets/rig.glb");
    Animation* testanimation = new Animation("assets/walkanim.glb");
    Animation* punchanim = new Animation("assets/punchanim.glb");

    // ------------------------------------------------------------------------
    // Player character
    // ------------------------------------------------------------------------
    Character* testcharacter = new Character();
    testcharacter->name = "character";
    testcharacter->castshadow = true;
    testcharacter->body->t.ScaleBy({ 0.7f, 2.5f, 0.7f });
    testcharacter->body->t.TranslateTo({ -20.25f, 4.0f, -33.0f });  // on 2fort's central courtyard floor
    testcharacter->body->SetRig(testrig);

    // Base animator (weight 0) � walk, looped
    testcharacter->body->SetAnimation(testanimation);
    testcharacter->body->animator->loops = true;

    // Punch animator (weight 1) � one-shot, starts stopped
    Animator* punchAnimator = testcharacter->body->AddAnimator(1);
    punchAnimator->loops = false;
    punchAnimator->SetAnimation(punchanim);
    punchAnimator->StopAnimation();

    // First person: don't draw our own model (other players still see it via the network)
    p* characterBody = new p(testcharacter->body);
    characterBody->mass = 80.0f;
    characterBody->lockRotation = true;
    characterBody->friction = 0.0f;
    physicsengine->AddObject(characterBody);

    // ------------------------------------------------------------------------
    // NPC
    // ------------------------------------------------------------------------
    Character* npc = new Character();
    npc->name = "npc";
    npc->castshadow = true;
    npc->body->t.ScaleBy({ 0.7f, 2.5f, 0.7f });
    npc->body->t.TranslateTo({ 3.0f, 1.25f, 3.0f });
    npc->body->SetRig(testrig);
    npc->body->SetAnimation(testanimation);
    npc->BindToRenderSystem();

    p* npcBody = new p(npc->body);
    npcBody->mass = 80.0f;
    npcBody->lockRotation = true;
    npcBody->friction = 0.0f;
    physicsengine->AddObject(npcBody);

    // ------------------------------------------------------------------------
    // Remote players (other PCs), keyed by network id. Not in physics: their transform
    // comes from the network. Created on first packet, removed when packets stop.
    // ------------------------------------------------------------------------
    struct RemotePlayer {
        Character* character = nullptr;
        double     lastSeen = 0.0;
        p*         body = nullptr;  // host only: lets the host's cubes collide with this player
        glm::vec3  targetPos = glm::vec3(0.0f);  // latest pose received; the model eases toward it every frame
        glm::quat  targetRot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    };
    std::map<int, RemotePlayer> remotes;
    auto SpawnRemote = [&]() -> Character* {
        Character* c = new Character();
        c->name = "remote";
        c->colorOverride = glm::vec4(0.02f, 0.08f, 0.5f, 1.0f);  // dark blue
        c->castshadow = true;
        c->body->t.ScaleBy({ 0.7f, 2.5f, 0.7f });
        c->body->SetRig(testrig);
        c->body->SetAnimation(testanimation);
        c->BindToRenderSystem();
        return c;
        };

    // In a LAN game, start at a random spot so players don't all spawn inside each other
    if (lan && lan->IsActive()) {
        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> spread(-8.0f, 8.0f);
        testcharacter->body->t.TranslateTo({ spread(rng), 10.0f, spread(rng) });
    }

    // ------------------------------------------------------------------------
    // Rain particles
    // ------------------------------------------------------------------------
    ParticleEmitter* rain = new ParticleEmitter();
    rain->name = "rain";
    rain->tex = snowflake;
    rain->opaque = false;
    rain->speed = 35.0f;
    rain->lifespan = 0.6f;
    rain->angularvelocity = { 0.0f, 0.0f, glm::radians(10.0f) };
    rain->size = { 0.04f, 0.4f, 0.0f };
    rain->emitangle = { glm::radians(60.0f), glm::radians(360.0f), glm::radians(60.0f) };
    rain->facecamera = false;
    rain->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    rain->emitdirection = ParticleEmitter::EmitDirection::Perpendicular;
    rain->t.TranslateTo({ 0.0f, 10.0f, 0.0f });
    rain->t.RotateToQuaternion(glm::quat(glm::vec3(glm::radians(85.0f), 0.0f, 0.0f)));
    rain->BindToRenderSystem();
    mainf->AddChild(rain);

    // ------------------------------------------------------------------------
    // 3D audio
    // ------------------------------------------------------------------------
    Sound* music = new Sound();
    music->name = "music";
    music->LoadSound("assets/fs.wav", true);
    music->Update3DPosition(0.0f, 0.0f, 0.0f);
    music->PlayTrack(true);
    mainf->AddChild(music);

    // ------------------------------------------------------------------------
    // GUI
    // ------------------------------------------------------------------------
    Crosshair* crosshair = new Crosshair();
    crosshair->Color = { 0.0f, 0.0f, 0.0f };
    crosshair->Dot = true;
    crosshair->Apply();

    // Hit marker: an X around the crosshair built from small squares (Box has no rotation),
    // white for a normal hit and red for a headshot / kill. Fades out after each hit.
    std::vector<Box*> hitMarker;
    for (int corner = 0; corner < 4; ++corner) {
        const float sx = (corner & 1) ? 1.0f : -1.0f, sy = (corner & 2) ? 1.0f : -1.0f;
        for (int step = 0; step < 3; ++step) {
            const float d = 7.0f + step * 3.5f;
            Box* b = new Box();
            b->Opacity = 0.0f;
            b->t2d.center = { 0.5f, 0.5f };
            b->t2d.position = { 0.5f, 0.5f, sx * d, sy * d };
            b->t2d.size = { 0.0f, 0.0f, 3.5f, 3.5f };
            b->BindToRenderSystem();
            hitMarker.push_back(b);
        }
    }
    float hitMarkerTime = 0.0f;
    const float hitMarkerDuration = 0.25f;
    auto ShowHitMarker = [&](bool strong) {
        hitMarkerTime = hitMarkerDuration;
        for (Box* b : hitMarker) b->Color = strong ? glm::vec3(1.0f, 0.15f, 0.15f) : glm::vec3(1.0f);
        };

    TextBox* fpsCounter = new TextBox();
    fpsCounter->font = arial;
    fpsCounter->text = "TEST1231231123";
    fpsCounter->fontsize = 0.5f;
    fpsCounter->Color = { 0.0f, 0.0f, 0.0f };
    fpsCounter->t2d.position = { 1.0f, 0.0f, 0.0f, 0.0f };
    fpsCounter->t2d.center = { 1.0f, 0.0f };
    fpsCounter->textCenter = { 0.0f, 0.5f };
    fpsCounter->t2d.size = { 0.1f, 0.1f, 0.0f, 0.0f };
    fpsCounter->BindToRenderSystem();

    TextBox* netText = new TextBox();
    netText->font = arial;
    netText->text = "";
    netText->fontsize = 0.5f;
    netText->Color = { 1.0f, 1.0f, 1.0f };
    netText->t2d.position = { 1.0f, 0.0f, 0.0f, 110.0f };
    netText->t2d.center = { 1.0f, 0.0f };
    netText->textCenter = { 1.0f, 0.5f };
    netText->t2d.size = { 0.0f, 0.0f, 360.0f, 40.0f };
    netText->BindToRenderSystem();

    // Console log drawn on the top-left (fed by the std::cout tee in Log.h)
    TextBox* logText = new TextBox();
    logText->font = arial;
    logText->Color = { 0.0f, 0.0f, 0.0f };
    logText->fontsize = 0.35f;
    logText->t2d.position = { 0.0f, 0.0f, 10.0f, 10.0f };
    logText->t2d.center = { 0.0f, 0.0f };
    logText->textCenter = { 0.0f, 0.0f };
    logText->t2d.size = { 0.0f, 0.0f, 780.0f, 360.0f };
    logText->BindToRenderSystem();

    // Full-screen green flash when the player takes damage (drawn before the HUD text)
    Box* damageFlash = new Box();
    damageFlash->Color = { 0.0f, 1.0f, 0.2f };
    damageFlash->Opacity = 0.0f;
    damageFlash->t2d.position = { 0.0f, 0.0f, 0.0f, 0.0f };
    damageFlash->t2d.center = { 0.0f, 0.0f };
    damageFlash->t2d.size = { 1.0f, 1.0f, 0.0f, 0.0f };
    damageFlash->BindToRenderSystem();
    int   lastPlayerHealth = 100;
    float damageFlashTime = 0.0f;
    const float damageFlashDuration = 0.35f;

    TextBox* hpText = new TextBox();
    hpText->font = arial;
    hpText->text = "100/100 HP";
    hpText->fontsize = 0.6f;
    hpText->Color = { 1.0f, 1.0f, 1.0f };
    hpText->t2d.position = { 0.0f, 1.0f, 20.0f, -20.0f };
    hpText->t2d.center = { 0.0f, 1.0f };
    hpText->textCenter = { 0.0f, 0.5f };
    hpText->t2d.size = { 0.0f, 0.0f, 400.0f, 90.0f };
    hpText->BindToRenderSystem();

    TextBox* grappleText = new TextBox();
    grappleText->font = arial;
    grappleText->text = "GRAPPLE READY";
    grappleText->fontsize = 0.5f;
    grappleText->Color = { 1.0f, 1.0f, 1.0f };
    grappleText->t2d.position = { 0.5f, 1.0f, 0.0f, -20.0f };
    grappleText->t2d.center = { 0.5f, 1.0f };
    grappleText->textCenter = { 0.5f, 0.5f };
    grappleText->t2d.size = { 0.0f, 0.0f, 1000.0f, 40.0f };
    grappleText->BindToRenderSystem();

    // Shot cooldown bar removed (it rendered at the top of the screen).

    // ------------------------------------------------------------------------
    // World meshes
    // ------------------------------------------------------------------------
    Model leonrevolver("assets/leonrevolver.obj");
    for (Mesh* mesh : leonrevolver.meshes) {
        auto newmesh = mesh->Clone();
        newmesh->name = "revolver";
        newmesh->castshadow = true;
        newmesh->meshTexture = leontex;
        newmesh->t.TranslateBy({ 4.0f, 4.0f, 4.0f });
        newmesh->BindToRenderSystem();
        mainf->AddChild(newmesh);
    }

    Mesh* debugcube = CreateCubeMesh();
    debugcube->name = "debugcube";
    debugcube->t.ScaleTo({ 0.8f, 0.8f, 0.8f });
    // Punch hitbox only: intentionally not bound to the render system, so it isn't drawn
    mainf->AddChild(debugcube);

    // The old arena floor was one of the two giant flat planes, so it is gone: 2fort itself
    // is the whole map now. No floor means no large horizontal plane to look at.
    const float mapScale = 1.5f;  // uniform scale used by the (commented-out) greybox props

    // Static map props: axis-aligned blocks (collision is AABB-based, so dome/cones/ramp are
    // built from stacked tiers and stairs). Footprints are recorded so respawns avoid them.
    struct Footprint { float minX, maxX, minZ, maxZ; };
    std::vector<Footprint> propFootprints;
    // pos = centre of the block's footprint on the floor; it sits on y = 0 and is `size.y` tall.
    // Walls are frictionless (player friction is 0 too) so you slide along them.
    auto AddBlock = [&](const char* name, glm::vec3 pos, glm::vec3 size, bool slippery = false) {
        pos *= mapScale;
        size *= mapScale;
        Mesh* m = CreateCubeMesh();
        m->name = name;
        m->meshTexture = blanktex;
        m->t.ScaleTo(size);
        m->t.TranslateTo({ pos.x, pos.y + size.y * 0.5f, pos.z });
        m->BindToRenderSystem();
        mainf->AddChild(m);
        p* body = new p(m);
        body->velocity = false;
        if (slippery) body->friction = 0.0f;
        physicsengine->AddObject(body);
        propFootprints.push_back({ pos.x - size.x * 0.5f, pos.x + size.x * 0.5f,
                                   pos.z - size.z * 0.5f, pos.z + size.z * 0.5f });
    };
    // Concentric stack of square tiers, widest at the bottom (stands in for dome / cone)
    // xStretch scales the stack along x only (left/right); height and depth are unchanged
    auto AddTiers = [&](const char* name, glm::vec2 pos, float baseWidth, float tierHeight, int tiers, float xStretch = 1.0f) {
        for (int i = 0; i < tiers; ++i) {
            const float w = baseWidth * (1.0f - float(i) / float(tiers));
            AddBlock(name, { pos.x, tierHeight * i, pos.y }, { w * xStretch, tierHeight, w });
        }
    };

    // Map is a rectangle. All of the old greybox level props are commented out below;
    // 2fort (loaded further down) is the map now.
    /*
    // Big cubes either side of the 2fort building
    AddBlock("cube", { -30.0f, 0.0f, 0.0f }, { 7.0f, 8.4f, 7.0f });
    AddBlock("cube", { 30.0f, 0.0f, 0.0f }, { 7.0f, 8.4f, 7.0f });
    // Stairs climbing to a tall back wall (the ramp), back right
    AddBlock("stair", { 29.0f, 0.0f, -12.0f }, { 2.5f, 1.0f, 10.0f });
    AddBlock("stair", { 31.5f, 0.0f, -12.0f }, { 2.5f, 2.0f, 10.0f });
    AddBlock("stair", { 34.0f, 0.0f, -12.0f }, { 2.5f, 3.0f, 10.0f });
    AddBlock("wall", { 36.5f, 0.0f, -12.0f }, { 1.0f, 9.0f, 10.0f }, true);
    // Stacked boxes, front right
    AddBlock("box", { 30.0f, 0.0f, 12.0f }, { 6.0f, 2.5f, 6.0f });
    AddBlock("box", { 34.5f, 0.0f, 15.0f }, { 5.0f, 5.0f, 5.0f });
    // Long wall with a small block at its end, left front
    AddBlock("wall", { -33.0f, 0.0f, 9.0f }, { 1.0f, 7.0f, 18.0f }, true);
    AddBlock("box", { -30.0f, 0.0f, 17.5f }, { 3.0f, 2.0f, 3.0f });
    // Small box cluster, back left
    AddBlock("box", { -26.0f, 0.0f, -12.0f }, { 6.0f, 2.0f, 6.0f });
    AddBlock("box", { -22.0f, 0.0f, -15.5f }, { 6.0f, 1.0f, 4.0f });
    */

    // ------------------------------------------------------------------------
    // 2fort (Blender export, assets/2fort.obj): one full copy of the test
    // texture per face. The OBJ's objects are simple boxes, so each one gets an
    // invisible box collider (slanted pieces get a rotated OBB instead of their
    // fat axis-aligned bounds); collision can only be boxes.
    // ------------------------------------------------------------------------
    {
        Model fort("assets/2fort.obj");
        engine->rendersystem->quietRemovals = true;  // ~Model tears down its source meshes below
        const float fortScale = 15.0f;  // 2.5x the previous 6.0 (the building is ~115 x 58 units)
        const float fortLift = 0.02f;   // small lift so the fort's bottom sits just above y=0

        // Ground offset: derived from the model itself so a new export (which may add
        // pieces lower than the last one, e.g. the courtyard ramps) always sits on y=0
        // instead of sinking. Flat quads (the removed ground plane) are ignored, exactly
        // as in the loop below.
        float modelMinY = FLT_MAX;
        for (Mesh* mesh : fort.meshes) {
            if (mesh->t.GetScale().y < 1e-4f) continue;
            modelMinY = std::min(modelMinY, mesh->t.GetTranslation().y - mesh->t.GetScale().y * 0.5f);
        }
        if (modelMinY == FLT_MAX) modelMinY = 0.0f;
        const float fortBaseY = -modelMinY;
        int orientedColliders = 0;
        for (Mesh* mesh : fort.meshes) {
            // The loader normalizes every mesh: its vertices span 1 unit around the mesh
            // origin and t holds the model-space centre + the object's real size. So the
            // world placement has to be built from t, not from the vertex positions.
            const glm::vec3 modelCentre = mesh->t.GetTranslation();
            const glm::vec3 modelSize   = mesh->t.GetScale();

            // Skip the model's giant flat ground plane (the second of the two planes).
            // It has zero height while every building piece is at least 0.03 thick.
            if (modelSize.y < 1e-4f) continue;

            const glm::vec3 worldCentre = modelCentre * fortScale
                + glm::vec3(0.0f, fortBaseY * fortScale + fortLift, 0.0f);
            const glm::vec3 worldSize   = modelSize * fortScale;

            // Model's destructor destroys its meshes, so clone (same reason the revolver does)
            Mesh* newmesh = mesh->Clone();
            newmesh->name = "2fort";
            newmesh->meshTexture = orangetex;
            newmesh->castshadow = true;
            MapFaceUVs(newmesh);  // one full copy of the texture per face
            newmesh->UpdateVertices();
            newmesh->t.ScaleTo(worldSize);
            newmesh->t.TranslateTo(worldCentre);
            newmesh->BindToRenderSystem();
            mainf->AddChild(newmesh);

            if (worldSize.x < 0.01f || worldSize.y < 0.01f || worldSize.z < 0.01f) continue;

            // Slanted pieces (the curved rail sections) get a rotated OBB collider so the
            // box hugs the angled geometry instead of using its fat axis-aligned bounds.
            glm::vec3 obbCentre = modelCentre;
            glm::vec3 obbSize   = modelSize;
            glm::quat obbRot    = glm::quat();
            if (ComputeOrientedBox(mesh, obbCentre, obbSize, obbRot)) {
                // Count only the ones that are actually rotated/tighter than their AABB.
                const float aabbVolume = modelSize.x * modelSize.y * modelSize.z;
                const float obbVolume  = obbSize.x * obbSize.y * obbSize.z;
                if (obbVolume < aabbVolume * 0.98f) ++orientedColliders;
            }

            Mesh* collider = CreateCubeMesh();
            collider->name = "fortcollider";
            collider->t.ScaleTo(obbSize * fortScale);
            collider->t.RotateToQuaternion(obbRot);
            collider->t.TranslateTo(obbCentre * fortScale
                + glm::vec3(0.0f, fortBaseY * fortScale + fortLift, 0.0f));
            mainf->AddChild(collider);  // not bound to the render system: invisible
            p* fortBody = new p(collider);
            fortBody->velocity = false;  // static world geometry
            physicsengine->AddObject(fortBody);
            // No footprint entry for the fort: the fort IS the map now, so respawns walk on it.
        }
        std::cout << "[map] 2fort loaded: " << fort.meshes.size() << " pieces ("
                  << orientedColliders << " oriented colliders, ground offset "
                  << fortBaseY * fortScale + fortLift << ")\n";
    }
    engine->rendersystem->quietRemovals = false;

    // Ground check
    auto IsGrounded = [&]() -> bool {
        Ray downRay;
        downRay.origin = testcharacter->body->t.GetTranslation();
        downRay.direction = glm::vec3(0.0f, -1.0f, 0.0f);
        const float halfHeight = testcharacter->body->t.GetScale().y * 0.5f;
        for (p* obj : physicsengine->GetObjects()) {
            if (obj->pointer == testcharacter->body) continue;
            if (!obj->collision) continue;
            auto hit = IsRayInT(downRay, obj->pointer->t);
            if (hit.has_value()) {
                if (glm::distance(downRay.origin, hit.value()) < halfHeight + 0.15f)
                    return true;
            }
        }
        return false;
        };

    // Hitscan shot from the camera. Head = top 20% of the target's box = instant kill,
    // body = 50 damage (two shots). Respawns the target when it dies.
    // Movement tuning
    const float airControl = 0.5f;           // fraction of ground acceleration available in the air
    const float shockedAirControl = 0.1f;    // air control after being hit by another player's shockwave
    const float shockedDuration = 2.0f;      // seconds the reduced air control lasts
    float       shockedTime = 0.0f;
    const float grappleRange = 30.0f;        // metres from the player
    const float grappleSpeed = 28.0f;        // launch speed toward the hit point
    // Visible grapple rope: a thin black box stretched from the hand to the latch point
    const float grappleLineShowTime = 0.5f;
    float       grappleLineTime = 0.0f;
    glm::vec3   grappleLineEnd(0.0f);
    Mesh* grappleLine = CreateCubeMesh();
    grappleLine->name = "grappleline";
    grappleLine->meshTexture = blanktex;
    grappleLine->colorOverride = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);  // black
    grappleLine->t.TranslateTo({ 0.0f, -1000.0f, 0.0f });  // hidden until a grapple latches
    grappleLine->BindToRenderSystem();
    mainf->AddChild(grappleLine);
    const float grappleLift = 10.0f;         // extra upward speed added on top of the pull
    const float grappleCooldownTime = 2.5f;  // seconds
    float       grappleCooldown = 0.0f;

    // Clawline (key E): a midair pull in whatever direction the camera is looking. Unlike
    // the R grapple, there are no hook points: the zip runs straight along the crosshair,
    // gravity is ignored while it runs and every bit of momentum is cancelled when it ends,
    // so you stop exactly where the pull put you. Ray-clamped so it never grinds you into a
    // wall. Only usable in the air, gated by a cooldown instead of silk.
    const float clawlineSpeed = 40.0f;       // speed of the pull
    const float clawlineDistance = 18.0f;    // how far one pull carries you
    const float clawlineStopDist = 1.5f;     // "arrived" once this close to the end point
    const float clawlineMaxPullTime = 1.5f;  // seconds; safety net if the pull gets stuck
    const float clawlineCooldownTime = 2.0f; // seconds
    float       clawlineCooldown = 0.0f;
    bool        clawlineAttached = false;    // true while the pull is running
    float       clawlineAttachTime = 0.0f;
    float       clawlineTravelled = 0.0f;    // distance covered by the current pull
    float       clawlinePullDist = 0.0f;     // distance this pull is aiming for
    glm::vec3   clawlineDirection(0.0f, 0.0f, 1.0f);
    glm::vec3   clawlineTarget(0.0f);
    const float clawlineLineShowTime = 0.6f; // how long the rope stays visible
    float       clawlineLineTime = 0.0f;
    glm::vec3   clawlineLineEnd(0.0f);
    Mesh* clawlineLine = CreateCubeMesh();
    clawlineLine->name = "clawlineline";
    clawlineLine->meshTexture = blanktex;
    clawlineLine->colorOverride = glm::vec4(0.25f, 0.9f, 1.0f, 1.0f);  // cyan: distinct from the grapple rope
    clawlineLine->t.TranslateTo({ 0.0f, -1000.0f, 0.0f });  // hidden until a clawline fires
    clawlineLine->BindToRenderSystem();
    mainf->AddChild(clawlineLine);

    // Slide (key Left Ctrl): locked direction, committed for slideDuration, speed eases from
    // slideBoost x ground top speed down to 1x by slideFalloffTime.
    const float slideBoost = 1.5f;
    const float slideDuration = 0.8f;
    const float slideFalloffTime = 0.7f;
    const float slideDropY = 0.65f;  // standing centre height (0.9) minus lying (0.25)
    const float standEyeOffset = 1.05f;  // eyes 1.05 above body centre (body centre 1.25, height 2.5)
    const float slideEyeOffset = 0.25f;  // lying down, eyes just above the floor
    float       eyeOffset = standEyeOffset;
    bool        sliding = false;
    float       slideTime = 0.0f;
    const float slideCooldownTime = 1.0f;  // seconds after a slide ends before the next
    float       slideCooldown = 0.0f;
    const float dashCooldownTime = 1.5f;  // seconds between dashes
    float       dashCooldown = 0.0f;
    const float shootCooldownTime = 0.5f;  // seconds between shots
    float       shootCooldown = 0.0f;

    // Cubes (E). Whoever hosts (or an offline player) owns and simulates them and decides when they
    // despawn; LAN clients only ask the host for one and draw the poses it streams back.
    const float  cubeCooldownTime = 0.1f;  // seconds between cubes per player
    const double cubeLifetime = 30.0;      // seconds before a cube despawns
    float        cubeCooldown = 0.0f;
    // Cubes also rain from the sky on their own (host / offline only; clients see them via the stream)
    const float  cubeRainInterval = 0.5f;  // seconds between rain cubes
    const float  cubeRainHeight = 45.0f;
    const size_t cubeRainMax = 200;        // stop raining while this many cubes exist
    float        cubeRainTimer = 0.0f;
    struct HostCube { uint32_t id; Mesh* mesh; double spawnTime; };
    std::vector<HostCube> hostCubes;
    uint32_t              nextCubeId = 0;
    std::map<uint8_t, double> lastCubeSpawn;  // host: when each client last spawned one (rate limit)
    struct PuppetCube { Mesh* mesh; double lastSeen; };
    std::map<uint32_t, PuppetCube> puppetCubes;  // client: the host's cubes, keyed by host id
    auto SpawnCube = [&](const glm::vec3& at) {
        Mesh* nc = CreateCubeMesh();
        nc->name = "cube!";
        nc->t.ScaleTo(1.0f);
        nc->t.TranslateTo(at);
        nc->BindToRenderSystem();
        mainf->AddChild(nc);
        physicsengine->AddObject(new p(nc));
        hostCubes.push_back({ ++nextCubeId, nc, glfwGetTime() });
        };
    glm::vec3   slideDir(0.0f);

    // Shockwave (key Q): projectile that explodes on the first surface it touches. The blast
    // launches the player, the NPC and loose props away from the impact point. No damage.
    const float shockSpeed = 30.0f;
    const float shockGravity = 6.0f;
    const float shockLifetime = 3.0f;
    const float shockRadius = 7.0f;
    const float shockPower = 26.0f;       // launch speed at the centre of the blast
    const float shockCooldownTime = 6.0f;
    const float blastShowTime = 0.25f;
    const glm::vec3 hiddenPos(0.0f, -1000.0f, 0.0f);
    float       shockCooldown = 0.0f;
    bool        shockActive = false;
    float       shockAge = 0.0f;
    float       blastTime = 0.0f;
    glm::vec3   shockVel(0.0f);

    Mesh* shockMesh = CreateCubeMesh();
    shockMesh->name = "shockwave";
    shockMesh->meshTexture = blanktex;
    shockMesh->t.ScaleTo({ 0.3f, 0.3f, 0.3f });
    shockMesh->t.TranslateTo(hiddenPos);
    shockMesh->BindToRenderSystem();
    mainf->AddChild(shockMesh);

    Mesh* blastMesh = CreateCubeMesh();
    blastMesh->name = "shockblast";
    blastMesh->meshTexture = shockwavetex;
    blastMesh->t.TranslateTo(hiddenPos);
    blastMesh->BindToRenderSystem();
    mainf->AddChild(blastMesh);

    // fromNet = another player's blast: we only launch ourselves (the other machines launch
    // themselves the same way, since remote players are just puppets on this machine).
    // (Lambda default arguments can't reference locals, so callers pass radius/power explicitly.)
    auto Explode = [&](const glm::vec3& center, bool fromNet, float radius, float power) {
        auto launch = [&](const glm::vec3& pos, p* body) {
            const glm::vec3 offset = pos - center;
            const float dist = glm::length(offset);
            if (dist >= radius) return;
            // Away from the blast, biased upward so grounded targets actually lift off
            glm::vec3 dir = dist > 0.001f ? offset / dist : glm::vec3(0.0f, 1.0f, 0.0f);
            dir.y = std::max(dir.y, 0.0f) + 0.6f;
            dir = glm::normalize(dir);
            const float falloff = 1.0f - 0.7f * (dist / radius);  // 100% at centre, 30% at the edge
            body->linearvelocity += dir * power * falloff;
            body->WakeUp();

            // Another player's blast (not our own) takes away most of our air control for a while
            if (body == characterBody && fromNet) shockedTime = shockedDuration;

            if (body == characterBody && sliding) {
                // The slide overrides horizontal velocity every frame, so end it and stand back up
                sliding = false;
                glm::vec3 p0 = testcharacter->body->t.GetTranslation();
                p0.y += slideDropY + 0.02f;
                testcharacter->body->t.TranslateTo(p0);
            }
        };
        launch(testcharacter->body->t.GetTranslation(), characterBody);
        if (!fromNet) {
            launch(npc->body->t.GetTranslation(), npcBody);
            for (p* obj : physicsengine->GetObjects()) {
                if (!obj->velocity || obj == characterBody || obj == npcBody) continue;
                launch(obj->pointer->t.GetTranslation(), obj);
            }

            NetBlast nb;
            nb.center = center;
            nb.radius = radius;
            nb.power = power;
            if (lan && lan->IsActive()) lan->SendBlast(nb);

            shockActive = false;
            shockMesh->t.TranslateTo(hiddenPos);
        }
        else if (lan && lan->IsActive()) {
            // Another player's blast: launch the cubes (host: the real ones; client: its local copies)
            for (p* obj : physicsengine->GetObjects()) {
                bool isCube = false;
                for (const HostCube& hc : hostCubes) if (obj->pointer == hc.mesh) isCube = true;
                for (auto& kv : puppetCubes) if (obj->pointer == kv.second.mesh) isCube = true;
                if (isCube) launch(obj->pointer->t.GetTranslation(), obj);
            }
        }
        blastMesh->t.TranslateTo(center);
        blastMesh->t.ScaleTo(glm::vec3(0.5f));
        blastTime = blastShowTime;
        camera->AddScreenshake(0.25f);
        };

    const int   maxHealth = 100;
    const int   bodyDamage = 50;
    const int   punchDamage = 25;
    const float headFraction = 0.8f;  // top 20% of the hitbox counts as the head

    // Random respawn spot on 2fort's central courtyard floor. That is the only standable
    // ground now (the old arena floor is gone), so this keeps respawns off the void.
    std::mt19937 respawnRng(std::random_device{}());
    const float respawnHeight = 8.0f;  // respawn in the air and drop onto the courtyard
    auto RandomSpawn = [&]() -> glm::vec3 {
        // Courtyard floor spans roughly x [-40, -1], z [-81, -23] in world units.
        // Stay in the open front half, clear of the central structure and the side walls.
        std::uniform_real_distribution<float> spreadX(-38.0f, -3.5f), spreadZ(-47.0f, -26.0f);
        return { spreadX(respawnRng), respawnHeight, spreadZ(respawnRng) };
    };

    auto Shoot = [&]() {
        Ray ray;
        ray.origin = camera->t.GetTranslation();
        ray.direction = glm::normalize(camera->t.GetFrontVector());

        // Nearest shootable target along the ray: the NPC or any remote (LAN) player
        float     targetDist = 1e9f;
        glm::vec3 targetPoint(0.0f);
        t*        targetT = nullptr;
        int       targetRemoteId = -1;  // -1 = the NPC
        auto consider = [&](t& tr, int remoteId) {
            auto h = IsRayInT(ray, tr);
            if (!h.has_value()) return;
            const float d = glm::distance(ray.origin, h.value());
            if (d < targetDist) { targetDist = d; targetPoint = h.value(); targetT = &tr; targetRemoteId = remoteId; }
        };
        consider(npc->body->t, -1);
        for (auto& kv : remotes) consider(kv.second.character->body->t, kv.first);
        if (!targetT) return;

        // Walls/props in front of the target block the shot
        for (p* obj : physicsengine->GetObjects()) {
            if (obj->pointer == testcharacter->body || obj->pointer == npc->body) continue;
            if (!obj->collision) continue;
            auto blocked = IsRayInT(ray, obj->pointer->t);
            if (blocked.has_value() && glm::distance(ray.origin, blocked.value()) < targetDist)
                return;
        }

        const float heightFrac = (targetPoint.y - targetT->GetTranslation().y)
            / targetT->GetScale().y + 0.5f;
        const bool headshot = heightFrac >= headFraction;
        const int  damage = headshot ? maxHealth : bodyDamage;
        camera->AddScreenshake(0.05f);
        ShowHitMarker(headshot);

        if (targetRemoteId >= 0) {
            // Another player: their machine owns their health, so tell it
            NetHit hit;
            hit.damage = damage;
            if (lan && lan->IsActive()) lan->SendHit((uint8_t)targetRemoteId, hit);
            std::cout << (headshot ? "HEADSHOT" : "BODY SHOT") << " - player " << targetRemoteId << "\n";
            return;
        }

        npc->body->Health -= damage;
        std::cout << (headshot ? "HEADSHOT" : "BODY SHOT") << " - NPC HP " << npc->body->Health << "\n";

        if (npc->body->Health <= 0) {
            npc->body->Health = maxHealth;
            npc->body->t.TranslateTo(RandomSpawn());
            npcBody->linearvelocity = glm::vec3(0.0f);
            npcBody->WakeUp();
        }
        };

    // ========================================================================
    // Game loop
    // ========================================================================
    while (!glfwWindowShouldClose(window->handle))
    {
        frame++;
        // Real frame time (clamped so a hitch can't tunnel through colliders), scaled so the game
        // plays at the speed it had when it ran one 1/fps step per frame on a 144 Hz display.
        // refDt is the step the movement constants were tuned at.
        static double prevFrameTime = glfwGetTime();
        const double  frameNow = glfwGetTime();
        const float   deltatime = glm::clamp(float(frameNow - prevFrameTime), 0.001f, 1.0f / 60.0f) * (144.0f / float(fps));
        prevFrameTime = frameNow;
        const float   refDt = 1.0f / float(fps);

        const glm::vec3 camFront = camera->t.GetFrontVector();
        const glm::vec3 camRight = camera->t.GetRightVector();
        const float     walkScale = 0.6f;  // WASD speed relative to the original 3.9
        const float     movespeed = 3.0f * 1.3f * walkScale;

        // --------------------------------------------------------------------
        // Input � mouse
        // --------------------------------------------------------------------
        while (Mouse::Event buffer = mouse->Read()) {
            if (buffer.GetType() == Mouse::Event::Type::Move) {
                float xoffset = mouse->GetX() - mouse->GetLastX();
                float yoffset = mouse->GetLastY() - mouse->GetY();
                camera->ProcessMouseMovement(xoffset, yoffset);
            }
            else if (buffer.GetType() == Mouse::Event::Type::LPress) {
                if (shootCooldown <= 0.0f) {
                    shootCooldown = shootCooldownTime;
                    Shoot();
                }
            }
            else if (buffer.GetType() == Mouse::Event::Type::RPress) {
                camera->lockedcursor = !camera->lockedcursor;
            }
        }

        // --------------------------------------------------------------------
        // Input � held movement
        // --------------------------------------------------------------------
        glm::vec3 inputVel = glm::vec3(0.0f);
        if (keyboard->IsKeyDown('W')) inputVel += glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z));
        if (keyboard->IsKeyDown('S')) inputVel -= glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z));
        if (keyboard->IsKeyDown('A')) inputVel += glm::normalize(glm::vec3(camRight.x, 0.0f, camRight.z));
        if (keyboard->IsKeyDown('D')) inputVel -= glm::normalize(glm::vec3(camRight.x, 0.0f, camRight.z));
        if (glm::length(inputVel) > 0.0f)
            inputVel = glm::normalize(inputVel) * movespeed;

        // --------------------------------------------------------------------
        // Input � keyboard events
        // --------------------------------------------------------------------
        while (Keyboard::Event buffer = keyboard->ReadKey()) {
            if (buffer.GetCode() == 'C' && buffer.IsPress() && cubeCooldown <= 0.0f) {
                cubeCooldown = cubeCooldownTime;
                if (lan && lan->IsActive() && lan->GetMode() == Lan::Mode::Client)
                    lan->SendCubeSpawn(camera->t.GetTranslation());  // the host spawns it
                else
                    SpawnCube(camera->t.GetTranslation());
            }
            if (buffer.GetCode() == 'F' && buffer.IsPress()) {
                const glm::vec3 playerPos = testcharacter->body->t.GetTranslation();
                const glm::vec3 playerFront = glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z));

                // Restart punch on high-priority animator
                punchAnimator->m_CurrentTime = 0.0f;
                punchAnimator->PlayAnimation();

                debugcube->t.TranslateTo(playerPos + playerFront * 0.8f);
                debugcube->t.ScaleTo({ 0.8f, 0.8f, 0.8f });
                debugcube->t.RotateToQuaternion(LookAt(-playerFront));

                std::optional<TInTInfo> hit = TInT(debugcube->t, npc->body->t);
                if (hit.has_value()) {
                    std::cout << "HIT NPC\n";
                    npcBody->WakeUp();
                    npcBody->linearvelocity += playerFront * 5.0f + glm::vec3(0.0f, 3.0f, 0.0f);
                    camera->AddScreenshake(0.15f);  // landing a punch feels impactful
                    ShowHitMarker(false);

                    npc->body->Health -= punchDamage;
                    std::cout << "PUNCH - NPC HP " << npc->body->Health << "\n";
                    if (npc->body->Health <= 0) {
                        npc->body->Health = maxHealth;
                        npc->body->t.TranslateTo(RandomSpawn());
                        npcBody->linearvelocity = glm::vec3(0.0f);
                        npcBody->WakeUp();
                    }
                }
                else {
                    std::cout << "MISSED\n";
                }

                // Punch LAN players too: their machine applies the damage and knockback
                for (auto& kv : remotes) {
                    if (!TInT(debugcube->t, kv.second.character->body->t).has_value()) continue;
                    NetHit ph;
                    ph.damage = punchDamage;
                    ph.knockback = playerFront * 5.0f + glm::vec3(0.0f, 3.0f, 0.0f);
                    if (lan && lan->IsActive()) lan->SendHit((uint8_t)kv.first, ph);
                    camera->AddScreenshake(0.15f);
                    ShowHitMarker(false);
                    std::cout << "PUNCH - player " << kv.first << "\n";
                }
            }
            if (buffer.GetCode() == GLFW_KEY_LEFT_SHIFT && buffer.IsPress() && !sliding && dashCooldown <= 0.0f) {
                dashCooldown = dashCooldownTime;
                glm::vec3 dashDir = glm::length(inputVel) > 0.0f
                    ? glm::normalize(inputVel)
                    : glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z));
                characterBody->linearvelocity.x += dashDir.x * 18.0f;
                characterBody->linearvelocity.z += dashDir.z * 18.0f;
                camera->AddScreenshake(0.04f);
            }
            if (buffer.GetCode() == GLFW_KEY_LEFT_CONTROL && buffer.IsPress() && !sliding && slideCooldown <= 0.0f && IsGrounded()) {
                const glm::vec3 horizVel(characterBody->linearvelocity.x, 0.0f, characterBody->linearvelocity.z);
                if (glm::length(inputVel) > 0.0f || glm::length(horizVel) > 1.0f) {
                    slideDir = glm::length(inputVel) > 0.0f ? glm::normalize(inputVel) : glm::normalize(horizVel);
                    sliding = true;
                    slideTime = 0.0f;
                    slideCooldown = slideDuration + slideCooldownTime;

                    // Lie down feet-first: tilt the model's up axis to point back along the slide.
                    // Hitbox is the same box, rotated with it; drop it so the feet stay on the floor.
                    const glm::vec3 tiltAxis = glm::normalize(glm::cross(slideDir, glm::vec3(0.0f, 1.0f, 0.0f)));
                    testcharacter->body->t.RotateToQuaternion(
                        glm::angleAxis(glm::radians(90.0f), tiltAxis) * LookAt(-slideDir));
                    glm::vec3 pos = testcharacter->body->t.GetTranslation();
                    pos.y -= slideDropY;
                    testcharacter->body->t.TranslateTo(pos);
                    camera->AddScreenshake(0.06f);
                }
            }
            if (buffer.GetCode() == 'R' && buffer.IsPress() && !sliding && grappleCooldown <= 0.0f) {
                // Short-range grapple: aim with the camera, latch onto a surface within range
                Ray ray;
                ray.origin = camera->t.GetTranslation();
                ray.direction = glm::normalize(camFront);
                float bestDist = grappleRange;
                glm::vec3 bestPoint(0.0f);
                bool found = false;
                for (p* obj : physicsengine->GetObjects()) {
                    if (obj->pointer == testcharacter->body || obj->pointer == npc->body) continue;
                    if (!obj->collision) continue;
                    auto hit = IsRayInT(ray, obj->pointer->t);
                    if (!hit.has_value()) continue;
                    const float d = glm::distance(ray.origin, hit.value());
                    if (d < bestDist) { bestDist = d; bestPoint = hit.value(); found = true; }
                }
                if (found) {
                    characterBody->linearvelocity = glm::normalize(bestPoint - ray.origin) * grappleSpeed;
                    // Extra upward fling so the grapple gains height instead of just pulling sideways
                    characterBody->linearvelocity.y = std::max(characterBody->linearvelocity.y, 0.0f) + grappleLift;
                    characterBody->WakeUp();
                    grappleCooldown = grappleCooldownTime;
                    grappleLineTime = grappleLineShowTime;
                    grappleLineEnd = bestPoint;
                    camera->AddScreenshake(0.1f);
                }
            }
            if (buffer.GetCode() == 'E' && buffer.IsPress() && !sliding && !clawlineAttached
                && clawlineCooldown <= 0.0f && !IsGrounded()) {
                // Clawline: a straight pull along the crosshair while in the air. The ray
                // stops the pull just short of the first wall, so the zip never grinds you
                // into geometry (and the ray is what the pull's end point comes from).
                const glm::vec3 eye = camera->t.GetTranslation();
                const glm::vec3 bodyPos = testcharacter->body->t.GetTranslation();
                const glm::vec3 dir = glm::normalize(camFront);
                float pullDist = clawlineDistance;
                Ray ray;
                ray.origin = eye;
                ray.direction = dir;
                for (p* obj : physicsengine->GetObjects()) {
                    if (obj->pointer == testcharacter->body || obj->pointer == npc->body) continue;
                    if (!obj->collision) continue;
                    auto hit = IsRayInT(ray, obj->pointer->t);
                    if (hit.has_value())
                        pullDist = std::min(pullDist, glm::distance(eye, hit.value()) - 0.8f);
                }
                if (pullDist > 0.5f) {
                    // End point is measured from the body, not the eye, so the arrival check
                    // lines up with the path the body actually travels.
                    clawlineAttached = true;
                    clawlineAttachTime = 0.0f;
                    clawlineTravelled = 0.0f;
                    clawlinePullDist = pullDist;
                    clawlineDirection = dir;
                    clawlineTarget = bodyPos + dir * pullDist;
                    characterBody->linearvelocity = dir * clawlineSpeed;
                    characterBody->WakeUp();
                    clawlineCooldown = clawlineCooldownTime;
                    clawlineLineTime = clawlineLineShowTime;
                    clawlineLineEnd = clawlineTarget;
                    camera->AddScreenshake(0.08f);
                }
            }
            if (buffer.GetCode() == 'Q' && buffer.IsPress() && !shockActive && shockCooldown <= 0.0f) {
                const glm::vec3 front = glm::normalize(camFront);
                shockMesh->t.TranslateTo(camera->t.GetTranslation() + front * 0.6f);
                shockVel = front * shockSpeed;
                shockActive = true;
                shockAge = 0.0f;
                shockCooldown = shockCooldownTime;
                camera->AddScreenshake(0.05f);
            }
            if (buffer.GetCode() == 'G' && buffer.IsPress()) {
                camera->AddScreenshake(0.2f);
            }
            if (buffer.GetCode() == GLFW_KEY_SPACE && buffer.IsPress()) {
                if (!sliding && IsGrounded())
                    characterBody->linearvelocity.y = 11.0f;
            }
        }

        // --------------------------------------------------------------------
        // Input � held keys
        // --------------------------------------------------------------------
        if (keyboard->IsKeyDown(GLFW_KEY_ESCAPE))
            glfwSetWindowShouldClose(window->handle, 1);

        // Movement + damping
        grappleCooldown = std::max(grappleCooldown - deltatime, 0.0f);
        clawlineCooldown = std::max(clawlineCooldown - deltatime, 0.0f);
        slideCooldown = std::max(slideCooldown - deltatime, 0.0f);
        dashCooldown = std::max(dashCooldown - deltatime, 0.0f);
        shootCooldown = std::max(shootCooldown - deltatime, 0.0f);
        cubeCooldown = std::max(cubeCooldown - deltatime, 0.0f);
        const float accel = 40.0f;
        const float damp = 0.75f;
        const float groundTopSpeed = movespeed * accel * refDt * damp / (1.0f - damp);
        if (sliding) {
            // Direction is locked; speed eases from boost x down to 1x (quadratic: drops fastest early)
            slideTime += deltatime;
            const float u = glm::clamp(slideTime / slideFalloffTime, 0.0f, 1.0f);
            const float speed = (groundTopSpeed / walkScale) * (1.0f + (slideBoost - 1.0f) * (1.0f - u) * (1.0f - u));
            characterBody->linearvelocity.x = slideDir.x * speed;
            characterBody->linearvelocity.z = slideDir.z * speed;

            if (slideTime >= slideDuration) {
                sliding = false;
                glm::vec3 pos = testcharacter->body->t.GetTranslation();
                pos.y += slideDropY + 0.02f;  // stand back up (rotation is reset below)
                testcharacter->body->t.TranslateTo(pos);
            }
        }
        else if (IsGrounded()) {
            // Drag and acceleration rescaled so the top speed is the same at any frame rate
            const float dampStep = std::pow(damp, deltatime / refDt);
            const float accelScale = refDt * damp * (1.0f - dampStep) / (deltatime * dampStep * (1.0f - damp));
            characterBody->linearvelocity.x *= dampStep;
            characterBody->linearvelocity.z *= dampStep;
            characterBody->linearvelocity.x += inputVel.x * accel * deltatime * accelScale;
            characterBody->linearvelocity.z += inputVel.z * accel * deltatime * accelScale;
        }
        else if (glm::length(inputVel) > 0.0f) {
            // Air: no ground drag so momentum (grapple, dash) carries. Input only adds
            // speed along the wish direction up to movespeed, at airControl of ground accel.
            const glm::vec3 wishDir = glm::normalize(inputVel);
            const float current = glm::dot(glm::vec3(characterBody->linearvelocity.x, 0.0f,
                characterBody->linearvelocity.z), wishDir);
            const float add = glm::clamp(groundTopSpeed - current, 0.0f,
                accel * (shockedTime > 0.0f ? shockedAirControl : airControl) * movespeed * deltatime);
            characterBody->linearvelocity.x += wishDir.x * add;
            characterBody->linearvelocity.z += wishDir.z * add;
        }

        // --------------------------------------------------------------------
        // Scene updates
        // --------------------------------------------------------------------
        {
            const glm::vec3 playerPos = testcharacter->body->t.GetTranslation();
            const glm::vec3 playerFront = glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z));
            debugcube->t.TranslateTo(playerPos + playerFront * 0.8f);
            debugcube->t.ScaleTo({ 0.8f, 0.8f, 0.8f });
            debugcube->t.RotateToQuaternion(LookAt(-playerFront));
        }

        // Shockwave projectile: arc forward, explode on the first collider along this frame's path
        shockCooldown = std::max(shockCooldown - deltatime, 0.0f);
        shockedTime = std::max(shockedTime - deltatime, 0.0f);
        if (shockActive) {
            shockVel.y -= shockGravity * deltatime;
            shockAge += deltatime;
            const glm::vec3 from = shockMesh->t.GetTranslation();
            const glm::vec3 step = shockVel * deltatime;
            const float stepLen = glm::length(step);

            Ray ray;
            ray.origin = from;
            ray.direction = step / stepLen;
            float bestDist = stepLen;
            glm::vec3 bestPoint(0.0f);
            bool found = false;
            for (p* obj : physicsengine->GetObjects()) {
                if (obj == characterBody || !obj->collision) continue;
                auto hit = IsRayInT(ray, obj->pointer->t);
                if (!hit.has_value()) continue;
                const float d = glm::distance(from, hit.value());
                if (d <= bestDist) { bestDist = d; bestPoint = hit.value(); found = true; }
            }

            if (found) Explode(bestPoint, false, shockRadius, shockPower);
            else if (shockAge >= shockLifetime) {
                shockActive = false;
                shockMesh->t.TranslateTo(hiddenPos);
            }
            else shockMesh->t.TranslateTo(from + step);
        }
        if (blastTime > 0.0f) {
            blastTime -= deltatime;
            if (blastTime <= 0.0f) {
                blastMesh->t.TranslateTo(hiddenPos);
            }
            else {
                // Expand to the full blast radius over its lifetime
                const float u = 1.0f - blastTime / blastShowTime;
                blastMesh->t.ScaleTo(glm::vec3(0.5f + (shockRadius * 2.0f - 0.5f) * u));
            }
        }

        rain->t.TranslateTo(camera->t.GetTranslation() + glm::vec3(0.0f, 20.0f, 0.0f));

        if (!sliding) {
            testcharacter->body->t.RotateToQuaternion(
                LookAt(-glm::normalize(glm::vec3(camFront.x, 0.0f, camFront.z)))
            );
        }
        // First person: camera at the eyes (eased down while sliding)
        eyeOffset += ((sliding ? slideEyeOffset : standEyeOffset) - eyeOffset) * (1.0f - std::pow(0.7f, deltatime / refDt));
        camera->t.TranslateTo(testcharacter->body->t.GetTranslation() + glm::vec3(0.0f, eyeOffset, 0.0f));

        Sound* music = static_cast<Sound*>(mainf->GetFirstChildOfName("music"));
        music->Update3DPosition(5.0f, 4.0f, 6.5f);  // where the old floating cube was

        soundsystem->SetListenerPosition(
            camera->t.GetTranslation(),
            camera->t.GetFrontVector(),
            camera->t.GetUpVector()
        );

        // --------------------------------------------------------------------
        // FPS counter
        // --------------------------------------------------------------------
        {
            static double lastTime = glfwGetTime();
            static int    frameCount = 0;
            static int    measuredFps = 0;
            static float  accumulator = 0.0f;
            double now = glfwGetTime();
            accumulator += (float)(now - lastTime);
            lastTime = now;
            frameCount++;
            if (accumulator >= 0.25f) {
                measuredFps = (int)(frameCount / accumulator);
                frameCount = 0;
                accumulator = 0.0f;
            }
            fpsCounter->text = std::to_string(measuredFps) + " FPS";
        }
        // Console log: newest lines at the top-left
        if (g_logbuf) logText->text = g_logbuf->Tail(14);
        // Grapple rope: starts at the hand (follows the camera), ends on the latch point
        grappleLineTime = std::max(grappleLineTime - deltatime, 0.0f);
        if (grappleLineTime > 0.0f) {
            const glm::vec3 handPos = camera->t.GetTranslation() + camera->t.GetFrontVector() * 0.6f
                + camera->t.GetRightVector() * 0.3f - camera->t.GetUpVector() * 0.3f;
            const glm::vec3 delta = grappleLineEnd - handPos;
            const float len = glm::length(delta);
            if (len > 0.01f) {
                grappleLine->t.TranslateTo(handPos + delta * 0.5f);
                grappleLine->t.ScaleTo({ 0.05f, 0.05f, len });
                // Plain shortest-arc rotation of the box's long (z) axis onto the rope; LookAt breaks on straight up/down
                grappleLine->t.RotateToQuaternion(RotationBetweenVectors(glm::vec3(0.0f, 0.0f, 1.0f), delta));
            }
        }
        else grappleLine->t.TranslateTo(hiddenPos);
        // Clawline rope: same hand anchor, cyan so it reads differently from the grapple
        clawlineLineTime = std::max(clawlineLineTime - deltatime, 0.0f);
        if (clawlineLineTime > 0.0f) {
            const glm::vec3 handPos = camera->t.GetTranslation() + camera->t.GetFrontVector() * 0.6f
                + camera->t.GetRightVector() * 0.3f - camera->t.GetUpVector() * 0.3f;
            const glm::vec3 delta = clawlineLineEnd - handPos;
            const float len = glm::length(delta);
            if (len > 0.01f) {
                clawlineLine->t.TranslateTo(handPos + delta * 0.5f);
                clawlineLine->t.ScaleTo({ 0.05f, 0.05f, len });
                clawlineLine->t.RotateToQuaternion(RotationBetweenVectors(glm::vec3(0.0f, 0.0f, 1.0f), delta));
            }
        }
        else clawlineLine->t.TranslateTo(hiddenPos);
        hitMarkerTime = std::max(hitMarkerTime - deltatime, 0.0f);
        for (Box* b : hitMarker) b->Opacity = hitMarkerTime / hitMarkerDuration;
        // Damage flash: any HP loss restarts a short green fade-out
        if (testcharacter->body->Health < lastPlayerHealth)
            damageFlashTime = damageFlashDuration;
        lastPlayerHealth = testcharacter->body->Health;
        damageFlashTime = std::max(damageFlashTime - deltatime, 0.0f);
        damageFlash->Opacity = 0.45f * (damageFlashTime / damageFlashDuration);
        // Player respawn: at 0 HP or after falling off the map, teleport to a random spot
        if (testcharacter->body->Health <= 0 || testcharacter->body->t.GetTranslation().y < -20.0f) {
            testcharacter->body->Health = maxHealth;
            testcharacter->body->t.TranslateTo(RandomSpawn());
            characterBody->linearvelocity = glm::vec3(0.0f);
            characterBody->WakeUp();
            sliding = false;
            clawlineAttached = false;
        }
        hpText->text = std::to_string(std::max(testcharacter->body->Health, 0)) + "/100 HP";
        grappleText->text = grappleCooldown > 0.0f
            ? "GRAPPLE " + std::to_string(grappleCooldown).substr(0, 3) + "s"
            : "GRAPPLE READY";
        grappleText->text += clawlineCooldown > 0.0f
            ? "  |  CLAWLINE " + std::to_string(clawlineCooldown).substr(0, 3) + "s (E)"
            : "  |  CLAWLINE (E) READY";
        grappleText->text += shockCooldown > 0.0f
            ? "  |  SHOCKWAVE " + std::to_string(shockCooldown).substr(0, 3) + "s"
            : "  |  SHOCKWAVE READY (Q)";

        // Clawline (E): keep the pull running straight along the crosshair, gravity ignored,
        // until the end point is reached. Then cancel all momentum, so you stop dead instead
        // of carrying the pull's speed into whatever is next to you.
        if (clawlineAttached) {
            clawlineAttachTime += deltatime;
            clawlineTravelled += clawlineSpeed * deltatime;
            const glm::vec3 toEnd = clawlineTarget - testcharacter->body->t.GetTranslation();
            const float endDist = glm::length(toEnd);
            if (clawlineTravelled >= clawlinePullDist || endDist < clawlineStopDist
                || clawlineAttachTime > clawlineMaxPullTime) {
                clawlineAttached = false;
                characterBody->linearvelocity = glm::vec3(0.0f);  // cancel all momentum
            }
            else {
                characterBody->linearvelocity = clawlineDirection * clawlineSpeed;
                clawlineLineTime = std::max(clawlineLineTime, 0.1f);  // rope stays out while pulling
            }
            characterBody->WakeUp();
        }

        // --------------------------------------------------------------------
        // Step systems
        // --------------------------------------------------------------------
        camera->Step(deltatime);
        rain->Step(deltatime);
        testcharacter->Step(deltatime);
        npc->Step(deltatime);
        soundsystem->Update();
        // Split into equal substeps of at most 1/120 s so jump arcs, falling and collisions
        // come out the same whatever the frame rate
        {
            const int   substeps = std::max(1, (int)std::ceil(deltatime * 120.0f));
            const float subDt = deltatime / float(substeps);
            for (int i = 0; i < substeps; i++) physicsengine->Step(subDt);
        }

        // --------------------------------------------------------------------
        // LAN: send our transform, apply the other player's
        // --------------------------------------------------------------------
        // Rain cubes from the sky at random spots over the map (host / offline)
        if (!lan || !lan->IsActive() || lan->GetMode() == Lan::Mode::Host) {
            cubeRainTimer -= deltatime;
            if (cubeRainTimer <= 0.0f) {
                cubeRainTimer += cubeRainInterval;
                if (hostCubes.size() < cubeRainMax) {
                    glm::vec3 at = RandomSpawn();
                    at.y = cubeRainHeight;
                    SpawnCube(at);
                    std::uniform_real_distribution<float> spin(0.0f, 6.28318f);
                    hostCubes.back().mesh->t.RotateToEulerAngles({ spin(respawnRng), spin(respawnRng), spin(respawnRng) });
                }
            }
        }

        // Despawn cubes that have lived long enough (host / offline)
        {
            const double cubeNow = glfwGetTime();
            for (size_t i = 0; i < hostCubes.size();) {
                if (cubeNow - hostCubes[i].spawnTime > cubeLifetime) {
                    Mesh* m = hostCubes[i].mesh;
                    rendersystem->RemoveRenderable(m);
                    physicsengine->RemoveObject(m);
                    m->Delete();
                    hostCubes.erase(hostCubes.begin() + i);
                }
                else ++i;
            }
        }

        if (lan && lan->IsActive()) {
            NetState mine;
            mine.position = testcharacter->body->t.GetTranslation();
            mine.rotation = testcharacter->body->t.GetRotationQuaternion();
            mine.sliding = sliding;
            lan->Send(mine);

            const double now = glfwGetTime();
            std::vector<NetUpdate> updates;
            std::vector<NetBlast> blasts;
            std::vector<NetHit> hits;
            std::vector<NetCubeRequest> cubeRequests;
            std::vector<NetCube> cubeStates;
            lan->Receive(updates, &blasts, &hits, &cubeRequests, &cubeStates);

            // Host: spawn the cubes clients asked for (rate limited per client), then stream every cube's pose
            for (const NetCubeRequest& rq : cubeRequests) {
                double& last = lastCubeSpawn[rq.sender];
                if (now - last < cubeCooldownTime * 0.9) continue;
                last = now;
                SpawnCube(glm::clamp(rq.position, glm::vec3(-200.0f), glm::vec3(200.0f)));
            }
            if (lan->GetMode() == Lan::Mode::Host) {
                std::vector<NetCube> snapshot;
                snapshot.reserve(hostCubes.size());
                for (const HostCube& hc : hostCubes) {
                    NetCube nc;
                    nc.id = hc.id;
                    nc.position = hc.mesh->t.GetTranslation();
                    nc.rotation = hc.mesh->t.GetRotationQuaternion();
                    snapshot.push_back(nc);
                }
                lan->SendCubes(snapshot);
            }

            // Client: show the host's cubes, and drop the ones it stopped sending (despawned)
            for (const NetCube& nc : cubeStates) {
                auto it = puppetCubes.find(nc.id);
                if (it == puppetCubes.end()) {
                    Mesh* m = CreateCubeMesh();
                    m->name = "cube!";
                    m->t.ScaleTo(1.0f);
                    m->BindToRenderSystem();
                    mainf->AddChild(m);
                    // Simulated locally exactly like the host's cubes (so the local player pushes them with
                    // no lag); the host's stream below only corrects drift.
                    physicsengine->AddObject(new p(m));
                    m->t.TranslateTo(nc.position);
                    m->t.RotateToQuaternion(nc.rotation);
                    it = puppetCubes.emplace(nc.id, PuppetCube{ m, now }).first;
                }
                else {
                    t& ct = it->second.mesh->t;
                    const glm::vec3 err = nc.position - ct.GetTranslation();
                    if (glm::length(err) > 3.0f) {
                        ct.TranslateTo(nc.position);
                        ct.RotateToQuaternion(nc.rotation);
                    }
                    else {
                        ct.TranslateBy(err * 0.35f);
                        ct.RotateToQuaternion(glm::slerp(ct.GetRotationQuaternion(), nc.rotation, 0.35f));
                    }
                }
                it->second.lastSeen = now;
            }
            for (auto it = puppetCubes.begin(); it != puppetCubes.end();) {
                if (now - it->second.lastSeen > 1.5) {
                    rendersystem->RemoveRenderable(it->second.mesh);
                    physicsengine->RemoveObject(it->second.mesh);
                    it->second.mesh->Delete();
                    it = puppetCubes.erase(it);
                }
                else ++it;
            }
            for (const NetBlast& b : blasts)
                Explode(b.center, true, b.radius, b.power);
            // Damage from other players (respawn handles dying, see the player respawn check)
            for (const NetHit& h : hits) {
                testcharacter->body->Health -= h.damage;
                characterBody->linearvelocity += h.knockback;
                characterBody->WakeUp();
                camera->AddScreenshake(0.15f);
                std::cout << "HIT by player " << (int)h.sender << " for " << h.damage
                          << " - HP " << testcharacter->body->Health << "\n";
            }
            for (const NetUpdate& u : updates) {
                auto it = remotes.find(u.id);
                if (it == remotes.end()) {
                    // First packet from this player: spawn them exactly at their pose
                    RemotePlayer rp;
                    rp.character = SpawnRemote();
                    rp.character->body->t.TranslateTo(u.state.position);
                    rp.character->body->t.RotateToQuaternion(u.state.rotation);
                    rp.targetPos = u.state.position;
                    rp.targetRot = u.state.rotation;
                    if (lan->GetMode() == Lan::Mode::Host) {
                        // Solid for the host's cubes (kinematic: moved by the client's stream only)
                        rp.body = new p(rp.character->body);
                        rp.body->velocity = false;
                        physicsengine->AddObject(rp.body);
                    }
                    it = remotes.emplace(u.id, rp).first;
                }
                else {
                    // Packets arrive in bursts (several in one frame, then none), so just remember the
                    // newest pose here; the per-frame smoothing below does the actual moving.
                    it->second.targetPos = u.state.position;
                    it->second.targetRot = u.state.rotation;
                }
                it->second.lastSeen = now;
            }

            // Drop players we haven't heard from in 2 s, animate the rest
            for (auto it = remotes.begin(); it != remotes.end();) {
                if (now - it->second.lastSeen > 2.0) {
                    rendersystem->RemoveRenderable(it->second.character);
                    if (it->second.body) physicsengine->RemoveObject(it->second.character->body);
                    delete it->second.character;
                    it = remotes.erase(it);
                }
                else {
                    // Frame-rate independent ease toward the latest pose (no matter how packets were batched)
                    t& rt = it->second.character->body->t;
                    const glm::vec3 before = rt.GetTranslation();
                    if (glm::distance(before, it->second.targetPos) > 6.0f)
                        rt.TranslateTo(it->second.targetPos);  // respawn / big jump: snap
                    else {
                        const float k = 1.0f - std::exp(-deltatime * 25.0f);
                        rt.TranslateTo(glm::mix(before, it->second.targetPos, k));
                    }
                    rt.RotateToQuaternion(glm::slerp(rt.GetRotationQuaternion(), it->second.targetRot,
                        1.0f - std::exp(-deltatime * 25.0f)));
                    // Kinematic body: its velocity makes cubes get pushed like the client's own body does there
                    if (it->second.body && deltatime > 0.0f)
                        it->second.body->linearvelocity = (rt.GetTranslation() - before) / deltatime;
                    it->second.character->Step(deltatime);
                    ++it;
                }
            }

            if (lan->GetMode() == Lan::Mode::Host)
                netText->text = "LAN: hosting - " + std::to_string(lan->PeerCount() + 1) + "/"
                    + std::to_string(Lan::MaxPlayers) + " players";
            else
                netText->text = lan->IsConnected() ? "LAN: connected - " + std::to_string(remotes.size() + 1) + " players"
                                                   : "LAN: joining...";
        }

        // --------------------------------------------------------------------
        // Render
        // --------------------------------------------------------------------
        rendersystem->Render(*camera);

        glfwPollEvents();
    }
}

// ============================================================================
// GLFW callbacks
// ============================================================================
void framebuffer_size_callback(GLFWwindow* window, int w, int h)
{
    if (w <= 0 || h <= 0) return;  // minimised
    // Everything pixel-based (viewport, projection aspect, GUI layout, scissor) reads these
    ::window->width = w;
    ::window->height = h;
    // The scene is drawn into an offscreen framebuffer, so it must be re-allocated too,
    // otherwise the final blit stretches/tears after a resize.
    if (engine && engine->rendersystem) engine->rendersystem->ResizeScreen(w, h);
    glViewport(0, 0, w, h);
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // F11: toggle fullscreen on the primary monitor, restoring the old window rect on exit
    if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
        static int wx = 100, wy = 100, ww = 1920, wh = 1080;
        if (glfwGetWindowMonitor(window)) {
            glfwSetWindowMonitor(window, nullptr, wx, wy, ww, wh, 0);
        }
        else {
            glfwGetWindowPos(window, &wx, &wy);
            glfwGetWindowSize(window, &ww, &wh);
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        }
        glfwSwapInterval(1);
    }

    if (action == GLFW_PRESS)   keyboard->KeyDown(key);
    else if (action == GLFW_RELEASE) keyboard->KeyUp(key);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    mouse->OnMouseMove(static_cast<float>(xposIn), static_cast<float>(yposIn));
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    // Cursor coordinates are in window units, which can differ from framebuffer pixels (DPI scaling)
    int ww, wh;
    glfwGetWindowSize(window, &ww, &wh);
    glfwSetCursorPos(window, ww / 2.0, wh / 2.0);
    mouse->OnMouseMove(ww / 2.0f, wh / 2.0f);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) mouse->RightDown();
        else                      mouse->RightUp();
    }
    else if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) mouse->LeftDown();
        else                      mouse->LeftUp();
    }
}