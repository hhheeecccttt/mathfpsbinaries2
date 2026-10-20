#ifndef NET_CLASS
#define NET_CLASS

#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Multiplayer UDP link for a LAN pit. One player hosts (UDP port 27015), everyone else joins by IP.
// Clients send their own transform to the host every frame; the host relays everyone's
// transform to everyone else. The host is player id 0, joiners get ids 1, 2, 3, ...
// All Winsock code lives in Net.cpp so windows.h never leaks into the rest of the engine.

struct NetState {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    bool      sliding = false;
};

// A state received for some other player.
struct NetUpdate {
    uint8_t  id = 0;
    NetState state;
};

// A shockwave explosion. Every machine applies the launch to its own player, since remote
// players are only puppets here. Blasts are re-sent a few times (UDP) and de-duplicated on receive.
struct NetBlast {
    uint8_t   sender = 0;  // filled in by the host; ignored on send
    uint32_t  id = 0;      // filled in by SendBlast
    glm::vec3 center = glm::vec3(0.0f);
    float     radius = 0.0f;
    float     power = 0.0f;
};

// Damage dealt to another player. Each machine owns its own player's health, so the shooter
// aims locally and sends this to the victim, who applies it. Re-sent a few times and de-duplicated.
struct NetHit {
    uint8_t   sender = 0;  // filled in by the host
    uint32_t  id = 0;      // filled in by SendHit
    int       damage = 0;
    glm::vec3 knockback = glm::vec3(0.0f);  // added to the victim's velocity
};

// A physics cube owned by the host. The host simulates every cube and streams their poses to the
// clients, which only draw them. The host also decides when a cube despawns.
struct NetCube {
    uint32_t  id = 0;
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
};

// A client asking the host to spawn a cube. Re-sent a few times (UDP) and de-duplicated by the host.
struct NetCubeRequest {
    uint8_t   sender = 0;  // filled in by the host
    uint32_t  id = 0;
    glm::vec3 position = glm::vec3(0.0f);
};

class Lan {
public:
    enum class Mode { Off, Host, Client };

    // Total players in the pit, host included.
    static constexpr int MaxPlayers = 8;

    Lan();
    ~Lan();

    // Opens the socket. For Client, hostIp is the host's IPv4 address. Returns false on failure.
    bool Start(Mode mode, const std::string& hostIp);
    void Stop();

    Mode GetMode() const { return mode; }
    bool IsActive() const { return active; }

    // Call once per frame with our own state.
    void Send(const NetState& state);
    // Drains all waiting packets and appends the other players' newest states to 'out'.
    // If 'blasts' is given, new (de-duplicated) shockwave blasts from other players are appended to it.
    // 'hits' gets damage aimed at us (de-duplicated).
    // 'spawns' (host only) gets cube spawn requests from clients; 'cubes' (client only) gets the
    // host's cube poses.
    void Receive(std::vector<NetUpdate>& out, std::vector<NetBlast>* blasts = nullptr,
                 std::vector<NetHit>* hits = nullptr, std::vector<NetCubeRequest>* spawns = nullptr,
                 std::vector<NetCube>* cubes = nullptr);
    // Client: ask the host to spawn a cube at 'position'. Sent over the next few Send() calls.
    void SendCubeSpawn(const glm::vec3& position);
    // Host: stream cube poses to every client. Call once per frame; large sets go out in rotating
    // slices, so a cube is refreshed every few frames.
    void SendCubes(const std::vector<NetCube>& cubes);
    // Damage another player. 'target' is the id from their NetUpdate (the host is 0).
    void SendHit(uint8_t target, const NetHit& hit);
    // Tell the other players about a blast we caused. Sent over the next few Send() calls.
    void SendBlast(const NetBlast& blast);

    // Host: at least one player has joined. Client: the host has been heard from in the last 2 s.
    bool IsConnected() const;
    // Host only: number of joined players, not counting the host.
    int PeerCount() const;

    // This machine's IPv4 addresses (for the host to read out to the joiners).
    static std::string LocalAddresses();

private:
    struct Impl;
    Impl* impl;
    Mode mode = Mode::Off;
    bool active = false;
};

inline Lan* lan = nullptr;

#endif
