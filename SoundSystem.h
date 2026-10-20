#ifndef SOUND_SYSTEM_CLASS
#define SOUND_SYSTEM_CLASS
#define NOMINMAX
#include <fmod.hpp>
#include <fmod_errors.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

// Loaded sound asset – owns the FMOD::Sound* handle.
// Call SoundSystem::LoadSound() to get one; the system owns the lifetime.
struct SoundData {
    FMOD::Sound* sound = nullptr;
    bool is3D = true;
};

class SoundSystem {
public:
    SoundSystem();
    ~SoundSystem();

    // ---- asset loading ----
    // Returns a non-owning pointer into the internal cache.
    // Pass is3D=false for music/UI sounds that don't need spatialization.
    SoundData* LoadSound(const std::string& filename, bool is3D = true);

    // ---- playback ----
    // Plays a sound and returns the channel it's playing on (nullptr on failure).
    FMOD::Channel* PlaySound(SoundData* data, bool loop = false);

    // Plays a sound at a world position with full 3D attenuation.
    FMOD::Channel* PlaySound3D(SoundData* data, const glm::vec3& position, bool loop = false);

    // ---- 3D listener ----
    void SetListenerPosition(const glm::vec3& position,
        const glm::vec3& forward,
        const glm::vec3& up,
        const glm::vec3& velocity = glm::vec3(0.0f));

    // ---- per-channel 3D updates ----
    // Call every frame for moving emitters.
    void SetChannelPosition(FMOD::Channel* channel,
        const glm::vec3& position,
        const glm::vec3& velocity = glm::vec3(0.0f));

    // ---- must be called once per frame ----
    void Update();

    // ---- helpers ----
    FMOD::System* GetSystem() const { return system; }

private:
    FMOD::System* system = nullptr;

    // Cache: filename ? SoundData. System owns all SoundData objects.
    std::unordered_map<std::string, SoundData*> cache;

    static void CheckError(FMOD_RESULT result, const char* context);
    static FMOD_VECTOR ToFMOD(const glm::vec3& v);
};

#endif