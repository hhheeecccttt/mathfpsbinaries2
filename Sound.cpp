#include "Sound.h"
#include "common.h"

Sound::Sound()
{
    pos = { 0.0f, 0.0f, 0.0f };
    m_soundData = nullptr;
    m_channel = nullptr;
}

Sound::~Sound()
{
    StopTrack();
    // SoundSystem owns the SoundData lifetime — don't delete m_soundData here.
}

// ---------------------------------------------------------------------------
// LoadSound
// Delegates to SoundSystem's cache so the same file is never loaded twice.
// ---------------------------------------------------------------------------
void Sound::LoadSound(const std::string& filename, bool is3D)
{
    StopTrack();
    m_soundData = soundsystem->LoadSound(filename, is3D);
    if (!m_soundData)
        std::cout << "Sound::LoadSound — failed to load: " << filename << "\n";
    else if (is3D)
        m_soundData->sound->set3DMinMaxDistance(minDistance, maxDistance);
}

// ---------------------------------------------------------------------------
// PlayTrack
// ---------------------------------------------------------------------------
bool Sound::PlayTrack(bool loop)
{
    if (!m_soundData || !m_soundData->sound) {
        std::cout << "Sound::PlayTrack — no sound loaded\n";
        return false;
    }

    if (m_soundData->is3D) {
        m_channel = soundsystem->PlaySound3D(m_soundData, pos, loop);
    }
    else {
        m_channel = soundsystem->PlaySound(m_soundData, loop);
    }

    return m_channel != nullptr;
}

// ---------------------------------------------------------------------------
// StopTrack
// ---------------------------------------------------------------------------
bool Sound::StopTrack()
{
    if (!m_channel) return true;

    bool isPlaying = false;
    m_channel->isPlaying(&isPlaying);
    if (isPlaying)
        m_channel->stop();

    m_channel = nullptr;
    return true;
}

// ---------------------------------------------------------------------------
// Update3DPosition
// Call every frame for moving emitters.
// ---------------------------------------------------------------------------
void Sound::Update3DPosition(float x, float y, float z)
{
    pos = { x, y, z };

    // If channel is still active, push the new position immediately.
    if (m_channel) {
        bool isPlaying = false;
        m_channel->isPlaying(&isPlaying);
        if (isPlaying) {
            // Velocity estimated as zero — pass your actual velocity if you have it.
            soundsystem->SetChannelPosition(m_channel, pos);
        }
        else {
            m_channel = nullptr; // channel finished, clean up handle
        }
    }
}