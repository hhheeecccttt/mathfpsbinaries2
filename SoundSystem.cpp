#include "SoundSystem.h"
#include <iostream>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void SoundSystem::CheckError(FMOD_RESULT result, const char* context)
{
    if (result != FMOD_OK) {
        std::cout << "FMOD error [" << context << "]: "
            << FMOD_ErrorString(result) << "\n";
    }
}

FMOD_VECTOR SoundSystem::ToFMOD(const glm::vec3& v)
{
    return { v.x, v.y, v.z };
}

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

SoundSystem::SoundSystem()
{
    FMOD_RESULT result;

    result = FMOD::System_Create(&system);
    CheckError(result, "System_Create");

    // 512 max channels – raise if you need more simultaneous voices.
    result = system->init(512, FMOD_INIT_NORMAL | FMOD_INIT_3D_RIGHTHANDED, nullptr);
    CheckError(result, "system->init");

    // Default 3D settings: scale 1 unit = 1 metre, doppler on, rolloff linear.
    system->set3DSettings(1.0f,   // doppler scale
        1.0f,   // distance factor (metres per unit)
        1.0f);  // rolloff scale

    // Default listener orientation (matches XAudio2 original).
    SetListenerPosition({ 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f });
}

SoundSystem::~SoundSystem()
{
    for (auto& [key, sd] : cache) {
        if (sd && sd->sound)
            sd->sound->release();
        delete sd;
    }
    cache.clear();

    if (system) {
        system->close();
        system->release();
    }
}

// ---------------------------------------------------------------------------
// Asset loading
// ---------------------------------------------------------------------------

SoundData* SoundSystem::LoadSound(const std::string& filename, bool is3D)
{
    // Return cached version if already loaded.
    auto it = cache.find(filename);
    if (it != cache.end()) return it->second;

    FMOD_MODE mode = FMOD_DEFAULT | FMOD_CREATECOMPRESSEDSAMPLE;
    if (is3D)  mode |= FMOD_3D | FMOD_3D_LINEARROLLOFF;
    else       mode |= FMOD_2D;

    SoundData* sd = new SoundData();
    sd->is3D = is3D;

    FMOD_RESULT result = system->createSound(filename.c_str(), mode, nullptr, &sd->sound);
    CheckError(result, "createSound");

    if (result != FMOD_OK) {
        delete sd;
        return nullptr;
    }

    // Default min/max distances for 3D sounds (tune per-asset if needed).
    if (is3D)
        sd->sound->set3DMinMaxDistance(1.0f, 100.0f);

    cache[filename] = sd;
    return sd;
}

// ---------------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------------

FMOD::Channel* SoundSystem::PlaySound(SoundData* data, bool loop)
{
    if (!data || !data->sound) return nullptr;

    data->sound->setMode(loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);

    FMOD::Channel* channel = nullptr;
    FMOD_RESULT result = system->playSound(data->sound, nullptr, false, &channel);
    CheckError(result, "playSound");

    return (result == FMOD_OK) ? channel : nullptr;
}

FMOD::Channel* SoundSystem::PlaySound3D(SoundData* data,
    const glm::vec3& position,
    bool loop)
{
    if (!data || !data->sound) return nullptr;

    // Start paused so we can set position before the first audio callback.
    data->sound->setMode(loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);

    FMOD::Channel* channel = nullptr;
    FMOD_RESULT result = system->playSound(data->sound, nullptr, true, &channel);
    CheckError(result, "playSound3D");

    if (result != FMOD_OK || !channel) return nullptr;

    FMOD_VECTOR pos = ToFMOD(position);
    FMOD_VECTOR vel = { 0.0f, 0.0f, 0.0f };
    channel->set3DAttributes(&pos, &vel);
    channel->setPaused(false);

    return channel;
}

// ---------------------------------------------------------------------------
// Listener
// ---------------------------------------------------------------------------

void SoundSystem::SetListenerPosition(const glm::vec3& position,
    const glm::vec3& forward,
    const glm::vec3& up,
    const glm::vec3& velocity)
{
    FMOD_VECTOR pos = ToFMOD(position);
    FMOD_VECTOR vel = ToFMOD(velocity);
    FMOD_VECTOR fwd = ToFMOD(forward);
    FMOD_VECTOR up_ = ToFMOD(up);
    system->set3DListenerAttributes(0, &pos, &vel, &fwd, &up_);
}

// ---------------------------------------------------------------------------
// Per-channel 3D update
// ---------------------------------------------------------------------------

void SoundSystem::SetChannelPosition(FMOD::Channel* channel,
    const glm::vec3& position,
    const glm::vec3& velocity)
{
    if (!channel) return;
    FMOD_VECTOR pos = ToFMOD(position);
    FMOD_VECTOR vel = ToFMOD(velocity);
    channel->set3DAttributes(&pos, &vel);
}

// ---------------------------------------------------------------------------
// Frame update  –  must be called once per game loop tick
// ---------------------------------------------------------------------------

void SoundSystem::Update()
{
    system->update();
}