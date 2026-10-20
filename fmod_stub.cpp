// Silent FMOD stub for the Linux build: implements just the C++ API methods the
// game uses (see SoundSystem.cpp / Sound.cpp) as no-ops returning FMOD_OK, against
// the real vendored FMOD headers. The game runs exactly the same, just without audio.
#ifndef _WIN32
#include <fmod.hpp>
#include <cstdlib>

// Dummy handles so callers' null checks behave like a working audio system.
static FMOD::System*  g_stubSystem  = (FMOD::System*)std::malloc(1);
static FMOD::Sound*   g_stubSound   = (FMOD::Sound*)std::malloc(1);
static FMOD::Channel* g_stubChannel = (FMOD::Channel*)std::malloc(1);

extern "C" {

// The header's inline FMOD::System_Create() calls this C entry point.
FMOD_RESULT F_API FMOD_System_Create(FMOD_SYSTEM** system, unsigned int) {
    *system = (FMOD_SYSTEM*)g_stubSystem;
    return FMOD_OK;
}

}  // extern "C"

// ---- System ----------------------------------------------------------------
FMOD_RESULT FMOD::System::release() { return FMOD_OK; }
FMOD_RESULT FMOD::System::init(int, FMOD_INITFLAGS, void*) { return FMOD_OK; }
FMOD_RESULT FMOD::System::close() { return FMOD_OK; }
FMOD_RESULT FMOD::System::update() { return FMOD_OK; }
FMOD_RESULT FMOD::System::set3DSettings(float, float, float) { return FMOD_OK; }
FMOD_RESULT FMOD::System::set3DListenerAttributes(int, const FMOD_VECTOR*, const FMOD_VECTOR*,
                                                  const FMOD_VECTOR*, const FMOD_VECTOR*) {
    return FMOD_OK;
}
FMOD_RESULT FMOD::System::createSound(const char*, FMOD_MODE, FMOD_CREATESOUNDEXINFO*, Sound** sound) {
    *sound = g_stubSound;
    return FMOD_OK;
}
FMOD_RESULT FMOD::System::playSound(Sound*, ChannelGroup*, bool, Channel** channel) {
    *channel = g_stubChannel;
    return FMOD_OK;
}

// ---- Sound ----------------------------------------------------------------
FMOD_RESULT FMOD::Sound::release() { return FMOD_OK; }
FMOD_RESULT FMOD::Sound::set3DMinMaxDistance(float, float) { return FMOD_OK; }
FMOD_RESULT FMOD::Sound::setMode(FMOD_MODE) { return FMOD_OK; }

// ---- ChannelControl (Channel derives from it) ------------------------------
FMOD_RESULT FMOD::ChannelControl::set3DAttributes(const FMOD_VECTOR*, const FMOD_VECTOR*) { return FMOD_OK; }
FMOD_RESULT FMOD::ChannelControl::setPaused(bool) { return FMOD_OK; }
FMOD_RESULT FMOD::ChannelControl::isPlaying(bool* isplaying) {
    if (isplaying) *isplaying = true;
    return FMOD_OK;
}
FMOD_RESULT FMOD::ChannelControl::stop() { return FMOD_OK; }

#endif
