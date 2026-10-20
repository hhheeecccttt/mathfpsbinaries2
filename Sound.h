#ifndef SOUND_CLASS
#define SOUND_CLASS

#include <fmod.hpp>
#include <glm/glm.hpp>
#include "Object.h"
#include "SoundSystem.h"

class Sound : public Object {
public:
    Sound();
    ~Sound();

    // Load a sound file through the system cache.
    // is3D=false for music/UI that doesn't need spatialization.
    void LoadSound(const std::string& filename, bool is3D = true);

    bool PlayTrack(bool loop = false);
    bool StopTrack();

    // Call every frame if the sound is moving.
    void Update3DPosition(float x, float y, float z);
    void Update3DPosition(const glm::vec3& p) { Update3DPosition(p.x, p.y, p.z); }

    FMOD::Channel* GetChannel() const { return m_channel; }

public:
    glm::vec3 pos = { 0.0f, 0.0f, 0.0f };

    // Attenuation distances — set before PlayTrack if you want non-defaults.
    float minDistance = 1.0f;
    float maxDistance = 100.0f;

    // Doppler scale for this emitter (1.0 = normal).
    float dopplerScale = 1.0f;

private:
    SoundData* m_soundData = nullptr;
    FMOD::Channel* m_channel = nullptr;
};

#endif