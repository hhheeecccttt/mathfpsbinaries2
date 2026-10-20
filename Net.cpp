#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int NetSockLen;
#else
// POSIX sockets: the Winsock API used here maps almost 1:1, so the few
// differences are shimmed below instead of forking the whole file.
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR   (-1)
#define WSAEWOULDBLOCK EWOULDBLOCK
typedef socklen_t NetSockLen;
static inline int closesocket(int s) { return ::close(s); }
#endif

#include "Net.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>

// Winsock has WSAGetLastError(), POSIX just uses errno.
static inline int NetLastError() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

namespace {
    constexpr uint32_t       NET_MAGIC = 0x45494946;  // "EIIF"
    constexpr uint16_t       NET_VERSION = 3;
    constexpr unsigned short NET_PORT = 27015;
    constexpr double         NET_TIMEOUT = 2.0;       // seconds of silence before a player is dropped

    // One datagram = Header followed by 'count' Entry records.
    // Client -> host: count 1 (the client's own state, id ignored).
    // Host -> client: one entry per other player, host included (id 0).
#pragma pack(push, 1)
    struct Header {
        uint32_t magic;
        uint16_t version;
        uint32_t seq;
        uint8_t  count;
    };
    struct Entry {
        uint8_t id;
        float   pos[3];
        float   rot[4];  // x, y, z, w
        uint8_t sliding;
    };
#pragma pack(pop)

    constexpr uint32_t NET_MAGIC_BLAST = 0x45494942;  // "EIIB"
    constexpr int      BLAST_COPIES = 4;              // each blast is sent this many times

#pragma pack(push, 1)
    struct BlastPacket {
        uint32_t magic;
        uint16_t version;
        uint8_t  sender;
        uint32_t id;
        float    center[3];
        float    radius;
        float    power;
    };
#pragma pack(pop)

    constexpr uint32_t NET_MAGIC_HIT = 0x45494948;  // "EIIH"

#pragma pack(push, 1)
    struct HitPacket {
        uint32_t magic;
        uint16_t version;
        uint8_t  sender;
        uint8_t  target;
        uint32_t id;
        int16_t  damage;
        float    knock[3];
    };
#pragma pack(pop)

    constexpr uint32_t NET_MAGIC_SPAWN = 0x45494953;  // "EIIS"
    constexpr uint32_t NET_MAGIC_CUBES = 0x45494943;  // "EIIC"
    constexpr int      CUBES_PER_PACKET = 36;

#pragma pack(push, 1)
    struct SpawnPacket {
        uint32_t magic;
        uint16_t version;
        uint8_t  sender;
        uint32_t id;
        float    pos[3];
    };
    struct CubesHeader {
        uint32_t magic;
        uint16_t version;
        uint8_t  count;
    };
    struct CubeEntry {
        uint32_t id;
        float    pos[3];
        float    rot[4];  // x, y, z, w
    };
#pragma pack(pop)

    constexpr int MAX_ENTRIES = Lan::MaxPlayers;
    constexpr int MAX_DATAGRAM = (int)sizeof(Header) + MAX_ENTRIES * (int)sizeof(Entry);

    double Now() {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }

    Entry MakeEntry(uint8_t id, const NetState& s) {
        Entry e;
        e.id = id;
        e.pos[0] = s.position.x; e.pos[1] = s.position.y; e.pos[2] = s.position.z;
        e.rot[0] = s.rotation.x; e.rot[1] = s.rotation.y; e.rot[2] = s.rotation.z; e.rot[3] = s.rotation.w;
        e.sliding = s.sliding ? 1 : 0;
        return e;
    }

    NetState ReadEntry(const Entry& e) {
        NetState s;
        s.position = glm::vec3(e.pos[0], e.pos[1], e.pos[2]);
        s.rotation = glm::quat(e.rot[3], e.rot[0], e.rot[1], e.rot[2]);  // glm ctor order is w, x, y, z
        s.sliding = e.sliding != 0;
        return s;
    }

    bool SameAddress(const sockaddr_in& a, const sockaddr_in& b) {
        return a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
    }
}

struct Lan::Impl {
    struct Peer {
        sockaddr_in addr{};
        uint8_t     id = 0;
        double      lastRecv = 0.0;
        uint32_t    lastSeq = 0;
        bool        started = false;  // has sent at least one datagram (so lastSeq is valid)
        NetState    state;
        bool        hasState = false;
    };

    SOCKET      sock = INVALID_SOCKET;
    bool        wsaStarted = false;
    uint32_t    sendSeq = 0;

    // Host
    std::vector<Peer> peers;

    // Client
    sockaddr_in hostAddr{};
    bool        anyRecv = false;
    uint32_t    lastSeq = 0;
    double      lastRecvTime = 0.0;

    // Blasts waiting for their remaining copies to go out
    struct PendingBlast { BlastPacket pkt; int copiesLeft; };
    std::vector<PendingBlast> pendingBlasts;
    uint32_t blastSeq = 0;
    std::vector<uint64_t> seenBlasts;  // (type, sender, id) of recent blasts/hits, for de-duplication

    struct PendingSpawn { SpawnPacket pkt; int copiesLeft; };
    std::vector<PendingSpawn> pendingSpawns;
    uint32_t spawnSeq = 0;
    size_t   cubeCursor = 0;  // host: where the next slice of cubes starts

    struct PendingHit { HitPacket pkt; int copiesLeft; };
    std::vector<PendingHit> pendingHits;
    uint32_t hitSeq = 0;

    void SendHitPacket(const sockaddr_in& to, const HitPacket& pkt) {
        sendto(sock, (const char*)&pkt, (int)sizeof(pkt), 0, (const sockaddr*)&to, (int)sizeof(to));
    }

    // True the first time a (type, sender, id) triple is seen. type: 0 = blast, 1 = hit.
    bool FirstSighting(uint8_t type, uint8_t sender, uint32_t id) {
        const uint64_t key = ((uint64_t)type << 40) | ((uint64_t)sender << 32) | id;
        if (std::find(seenBlasts.begin(), seenBlasts.end(), key) != seenBlasts.end()) return false;
        seenBlasts.push_back(key);
        if (seenBlasts.size() > 64) seenBlasts.erase(seenBlasts.begin());
        return true;
    }

    void SendBlastPacket(const sockaddr_in& to, const BlastPacket& pkt) {
        sendto(sock, (const char*)&pkt, (int)sizeof(pkt), 0, (const sockaddr*)&to, (int)sizeof(to));
    }

    void SendDatagram(const sockaddr_in& to, const std::vector<Entry>& entries) {
        char buf[MAX_DATAGRAM];
        Header h;
        h.magic = NET_MAGIC;
        h.version = NET_VERSION;
        h.seq = ++sendSeq;
        h.count = (uint8_t)entries.size();
        std::memcpy(buf, &h, sizeof(h));
        std::memcpy(buf + sizeof(h), entries.data(), entries.size() * sizeof(Entry));
        sendto(sock, buf, (int)(sizeof(h) + entries.size() * sizeof(Entry)), 0,
               (const sockaddr*)&to, (int)sizeof(to));
    }
};

Lan::Lan() : impl(new Impl()) {}

Lan::~Lan() {
    Stop();
    delete impl;
}

bool Lan::Start(Mode m, const std::string& hostIp) {
    if (m == Mode::Off) return false;

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cout << "[net] WSAStartup failed\n";
        return false;
    }
    impl->wsaStarted = true;
#endif

    impl->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (impl->sock == INVALID_SOCKET) {
        std::cout << "[net] socket() failed: " << NetLastError() << "\n";
        Stop();
        return false;
    }

#ifdef _WIN32
    u_long nonBlocking = 1;
    ioctlsocket(impl->sock, FIONBIO, &nonBlocking);

    // Don't let an ICMP "port unreachable" (other side not up yet) kill recvfrom with WSAECONNRESET
    BOOL newBehavior = FALSE;
    DWORD bytes = 0;
    WSAIoctl(impl->sock, SIO_UDP_CONNRESET, &newBehavior, sizeof(newBehavior), NULL, 0, &bytes, NULL, NULL);
#else
    fcntl(impl->sock, F_SETFL, fcntl(impl->sock, F_GETFL, 0) | O_NONBLOCK);
#endif

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_ANY);
    local.sin_port = htons(m == Mode::Host ? NET_PORT : 0);  // clients use any free port
    if (bind(impl->sock, (sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
        std::cout << "[net] bind() failed: " << NetLastError()
                  << " (is another copy already hosting on this PC?)\n";
        Stop();
        return false;
    }

    if (m == Mode::Client) {
        impl->hostAddr.sin_family = AF_INET;
        impl->hostAddr.sin_port = htons(NET_PORT);
        if (inet_pton(AF_INET, hostIp.c_str(), &impl->hostAddr.sin_addr) != 1) {
            std::cout << "[net] '" << hostIp << "' is not a valid IPv4 address\n";
            Stop();
            return false;
        }
    }

    mode = m;
    active = true;
    return true;
}

void Lan::Stop() {
    if (impl->sock != INVALID_SOCKET) {
        closesocket(impl->sock);
        impl->sock = INVALID_SOCKET;
    }
#ifdef _WIN32
    if (impl->wsaStarted) {
        WSACleanup();
        impl->wsaStarted = false;
    }
#endif
    impl->peers.clear();
    impl->pendingBlasts.clear();
    impl->pendingHits.clear();
    impl->pendingSpawns.clear();
    impl->seenBlasts.clear();
    impl->anyRecv = false;
    active = false;
    mode = Mode::Off;
}

void Lan::SendBlast(const NetBlast& b) {
    if (!active) return;
    Impl::PendingBlast pb;
    pb.pkt.magic = NET_MAGIC_BLAST;
    pb.pkt.version = NET_VERSION;
    pb.pkt.sender = 0;  // the host stamps the real id when relaying a client's blast
    pb.pkt.id = ++impl->blastSeq;
    pb.pkt.center[0] = b.center.x; pb.pkt.center[1] = b.center.y; pb.pkt.center[2] = b.center.z;
    pb.pkt.radius = b.radius;
    pb.pkt.power = b.power;
    pb.copiesLeft = BLAST_COPIES;
    impl->pendingBlasts.push_back(pb);
}

void Lan::SendHit(uint8_t target, const NetHit& h) {
    if (!active) return;
    Impl::PendingHit ph;
    ph.pkt.magic = NET_MAGIC_HIT;
    ph.pkt.version = NET_VERSION;
    ph.pkt.sender = 0;  // the host stamps the real id when relaying a client's hit
    ph.pkt.target = target;
    ph.pkt.id = ++impl->hitSeq;
    ph.pkt.damage = (int16_t)std::clamp(h.damage, 0, 1000);
    ph.pkt.knock[0] = h.knockback.x; ph.pkt.knock[1] = h.knockback.y; ph.pkt.knock[2] = h.knockback.z;
    ph.copiesLeft = BLAST_COPIES;
    impl->pendingHits.push_back(ph);
}

void Lan::SendCubeSpawn(const glm::vec3& position) {
    if (!active || mode != Mode::Client) return;
    Impl::PendingSpawn ps;
    ps.pkt.magic = NET_MAGIC_SPAWN;
    ps.pkt.version = NET_VERSION;
    ps.pkt.sender = 0;  // the host stamps the real id
    ps.pkt.id = ++impl->spawnSeq;
    ps.pkt.pos[0] = position.x; ps.pkt.pos[1] = position.y; ps.pkt.pos[2] = position.z;
    ps.copiesLeft = BLAST_COPIES;
    impl->pendingSpawns.push_back(ps);
}

void Lan::SendCubes(const std::vector<NetCube>& cubes) {
    if (!active || mode != Mode::Host || cubes.empty() || impl->peers.empty()) return;

    const size_t n = cubes.size();
    const size_t count = std::min(n, (size_t)CUBES_PER_PACKET);
    if (impl->cubeCursor >= n) impl->cubeCursor = 0;

    char buf[sizeof(CubesHeader) + CUBES_PER_PACKET * sizeof(CubeEntry)];
    CubesHeader h;
    h.magic = NET_MAGIC_CUBES;
    h.version = NET_VERSION;
    h.count = (uint8_t)count;
    std::memcpy(buf, &h, sizeof(h));
    for (size_t i = 0; i < count; i++) {
        const NetCube& c = cubes[(impl->cubeCursor + i) % n];
        CubeEntry e;
        e.id = c.id;
        e.pos[0] = c.position.x; e.pos[1] = c.position.y; e.pos[2] = c.position.z;
        e.rot[0] = c.rotation.x; e.rot[1] = c.rotation.y; e.rot[2] = c.rotation.z; e.rot[3] = c.rotation.w;
        std::memcpy(buf + sizeof(h) + i * sizeof(e), &e, sizeof(e));
    }
    impl->cubeCursor = (impl->cubeCursor + count) % n;

    const int len = (int)(sizeof(h) + count * sizeof(CubeEntry));
    for (const Impl::Peer& to : impl->peers)
        sendto(impl->sock, buf, len, 0, (const sockaddr*)&to.addr, (int)sizeof(to.addr));
}

void Lan::Send(const NetState& s) {
    if (!active) return;

    // One more copy of every pending cube spawn request (client -> host)
    for (Impl::PendingSpawn& ps : impl->pendingSpawns) {
        sendto(impl->sock, (const char*)&ps.pkt, (int)sizeof(ps.pkt), 0,
               (const sockaddr*)&impl->hostAddr, (int)sizeof(impl->hostAddr));
        ps.copiesLeft--;
    }
    impl->pendingSpawns.erase(
        std::remove_if(impl->pendingSpawns.begin(), impl->pendingSpawns.end(),
                       [](const Impl::PendingSpawn& ps) { return ps.copiesLeft <= 0; }),
        impl->pendingSpawns.end());

    // One more copy of every pending hit: clients send to the host, the host to the target only
    for (Impl::PendingHit& ph : impl->pendingHits) {
        if (mode == Mode::Client) impl->SendHitPacket(impl->hostAddr, ph.pkt);
        else for (const Impl::Peer& to : impl->peers)
            if (to.id == ph.pkt.target) impl->SendHitPacket(to.addr, ph.pkt);
        ph.copiesLeft--;
    }
    impl->pendingHits.erase(
        std::remove_if(impl->pendingHits.begin(), impl->pendingHits.end(),
                       [](const Impl::PendingHit& ph) { return ph.copiesLeft <= 0; }),
        impl->pendingHits.end());

    // One more copy of every pending blast (to the host, or from the host to everyone)
    for (Impl::PendingBlast& pb : impl->pendingBlasts) {
        if (mode == Mode::Client) impl->SendBlastPacket(impl->hostAddr, pb.pkt);
        else for (const Impl::Peer& to : impl->peers) impl->SendBlastPacket(to.addr, pb.pkt);
        pb.copiesLeft--;
    }
    impl->pendingBlasts.erase(
        std::remove_if(impl->pendingBlasts.begin(), impl->pendingBlasts.end(),
                       [](const Impl::PendingBlast& pb) { return pb.copiesLeft <= 0; }),
        impl->pendingBlasts.end());

    if (mode == Mode::Client) {
        impl->SendDatagram(impl->hostAddr, { MakeEntry(0, s) });
        return;
    }

    // Host: to each player, send the host's state plus every other player's latest state
    for (const Impl::Peer& to : impl->peers) {
        std::vector<Entry> entries;
        entries.push_back(MakeEntry(0, s));
        for (const Impl::Peer& other : impl->peers) {
            if (other.id != to.id && other.hasState && (int)entries.size() < MAX_ENTRIES)
                entries.push_back(MakeEntry(other.id, other.state));
        }
        impl->SendDatagram(to.addr, entries);
    }
}

void Lan::Receive(std::vector<NetUpdate>& out, std::vector<NetBlast>* blasts, std::vector<NetHit>* hits,
                  std::vector<NetCubeRequest>* spawns, std::vector<NetCube>* cubes) {
    if (!active) return;

    char buf[1500];
    for (int i = 0; i < 512; i++) {  // bounded so a flood can't stall the frame
        sockaddr_in from{};
        NetSockLen fromLen = (NetSockLen)sizeof(from);
        int r = recvfrom(impl->sock, buf, (int)sizeof(buf), 0, (sockaddr*)&from, &fromLen);
        if (r == SOCKET_ERROR) {
            if (NetLastError() == WSAEWOULDBLOCK) break;
            continue;
        }
        if (r < (int)sizeof(Header)) continue;

        uint32_t magic;
        std::memcpy(&magic, buf, sizeof(magic));
        if (magic == NET_MAGIC_BLAST) {
            if (r != (int)sizeof(BlastPacket)) continue;
            BlastPacket bp;
            std::memcpy(&bp, buf, sizeof(bp));
            if (bp.version != NET_VERSION) continue;

            if (mode == Mode::Host) {
                // Only known players may blast. Stamp their id and relay to everyone else.
                const Impl::Peer* sender = nullptr;
                for (const Impl::Peer& pe : impl->peers)
                    if (SameAddress(pe.addr, from)) { sender = &pe; break; }
                if (!sender) continue;
                bp.sender = sender->id;
                for (const Impl::Peer& to : impl->peers)
                    if (to.id != sender->id) impl->SendBlastPacket(to.addr, bp);
            }
            else if (!SameAddress(from, impl->hostAddr)) continue;  // clients only listen to the host

            if (blasts && impl->FirstSighting(0, bp.sender, bp.id)) {
                NetBlast nb;
                nb.sender = bp.sender;
                nb.id = bp.id;
                nb.center = glm::vec3(bp.center[0], bp.center[1], bp.center[2]);
                nb.radius = bp.radius;
                nb.power = bp.power;
                blasts->push_back(nb);
            }
            continue;
        }

        if (magic == NET_MAGIC_SPAWN) {
            if (mode != Mode::Host || r != (int)sizeof(SpawnPacket)) continue;
            SpawnPacket sp;
            std::memcpy(&sp, buf, sizeof(sp));
            if (sp.version != NET_VERSION) continue;

            // Only known players may spawn cubes
            const Impl::Peer* sender = nullptr;
            for (const Impl::Peer& pe : impl->peers)
                if (SameAddress(pe.addr, from)) { sender = &pe; break; }
            if (!sender) continue;

            if (spawns && impl->FirstSighting(2, sender->id, sp.id)) {
                NetCubeRequest req;
                req.sender = sender->id;
                req.id = sp.id;
                req.position = glm::vec3(sp.pos[0], sp.pos[1], sp.pos[2]);
                spawns->push_back(req);
            }
            continue;
        }

        if (magic == NET_MAGIC_CUBES) {
            if (mode != Mode::Client || !SameAddress(from, impl->hostAddr)) continue;
            CubesHeader ch;
            if (r < (int)sizeof(ch)) continue;
            std::memcpy(&ch, buf, sizeof(ch));
            if (ch.version != NET_VERSION || ch.count > CUBES_PER_PACKET) continue;
            if (r != (int)(sizeof(ch) + ch.count * sizeof(CubeEntry))) continue;

            if (cubes) {
                for (int k = 0; k < ch.count; k++) {
                    CubeEntry e;
                    std::memcpy(&e, buf + sizeof(ch) + k * sizeof(e), sizeof(e));
                    NetCube c;
                    c.id = e.id;
                    c.position = glm::vec3(e.pos[0], e.pos[1], e.pos[2]);
                    c.rotation = glm::quat(e.rot[3], e.rot[0], e.rot[1], e.rot[2]);
                    cubes->push_back(c);
                }
            }
            continue;
        }

        if (magic == NET_MAGIC_HIT) {
            if (r != (int)sizeof(HitPacket)) continue;
            HitPacket hp;
            std::memcpy(&hp, buf, sizeof(hp));
            if (hp.version != NET_VERSION) continue;

            bool forUs = true;
            if (mode == Mode::Host) {
                // Only known players may shoot. Stamp their id; deliver to us or forward to the target.
                const Impl::Peer* sender = nullptr;
                for (const Impl::Peer& pe : impl->peers)
                    if (SameAddress(pe.addr, from)) { sender = &pe; break; }
                if (!sender) continue;
                hp.sender = sender->id;
                if (hp.target != 0) {
                    forUs = false;
                    for (const Impl::Peer& to : impl->peers)
                        if (to.id == hp.target) impl->SendHitPacket(to.addr, hp);
                }
            }
            else if (!SameAddress(from, impl->hostAddr)) continue;  // clients only listen to the host

            if (forUs && hits && impl->FirstSighting(1, hp.sender, hp.id)) {
                NetHit nh;
                nh.sender = hp.sender;
                nh.id = hp.id;
                nh.damage = hp.damage;
                nh.knockback = glm::vec3(hp.knock[0], hp.knock[1], hp.knock[2]);
                hits->push_back(nh);
            }
            continue;
        }

        Header h;
        std::memcpy(&h, buf, sizeof(h));
        if (h.magic != NET_MAGIC || h.version != NET_VERSION) continue;
        if (h.count == 0 || h.count > MAX_ENTRIES) continue;
        if (r != (int)(sizeof(Header) + h.count * sizeof(Entry))) continue;

        const double now = Now();

        if (mode == Mode::Host) {
            // Find this sender, or admit them as a new player
            Impl::Peer* peer = nullptr;
            for (Impl::Peer& p : impl->peers)
                if (SameAddress(p.addr, from)) { peer = &p; break; }

            if (!peer) {
                if ((int)impl->peers.size() >= MaxPlayers - 1) continue;  // pit is full
                uint8_t id = 1;
                while (std::any_of(impl->peers.begin(), impl->peers.end(),
                                   [&](const Impl::Peer& p) { return p.id == id; }))
                    id++;
                Impl::Peer np;
                np.addr = from;
                np.id = id;
                impl->peers.push_back(np);
                peer = &impl->peers.back();
                std::cout << "[net] player " << (int)id << " joined\n";
            }

            if (peer->started && (int32_t)(h.seq - peer->lastSeq) <= 0) continue;  // stale / duplicate
            peer->started = true;
            peer->lastSeq = h.seq;
            peer->lastRecv = now;

            Entry e;
            std::memcpy(&e, buf + sizeof(Header), sizeof(Entry));
            peer->state = ReadEntry(e);
            peer->hasState = true;

            NetUpdate u;
            u.id = peer->id;
            u.state = peer->state;
            out.push_back(u);
        }
        else {
            if (!SameAddress(from, impl->hostAddr)) continue;  // only listen to the host

            // After a long silence the host may have restarted (sequence back to 1): accept anything
            if (impl->anyRecv && now - impl->lastRecvTime >= NET_TIMEOUT) impl->anyRecv = false;
            if (impl->anyRecv && (int32_t)(h.seq - impl->lastSeq) <= 0) continue;
            impl->anyRecv = true;
            impl->lastSeq = h.seq;
            impl->lastRecvTime = now;

            for (int k = 0; k < h.count; k++) {
                Entry e;
                std::memcpy(&e, buf + sizeof(Header) + k * sizeof(Entry), sizeof(Entry));
                NetUpdate u;
                u.id = e.id;
                u.state = ReadEntry(e);
                out.push_back(u);
            }
        }
    }

    // Host: forget players who stopped sending
    if (mode == Mode::Host) {
        const double now = Now();
        for (size_t i = 0; i < impl->peers.size();) {
            if (now - impl->peers[i].lastRecv > NET_TIMEOUT) {
                std::cout << "[net] player " << (int)impl->peers[i].id << " left\n";
                impl->peers.erase(impl->peers.begin() + i);
            }
            else i++;
        }
    }
}

bool Lan::IsConnected() const {
    if (!active) return false;
    if (mode == Mode::Host) return !impl->peers.empty();
    return impl->anyRecv && (Now() - impl->lastRecvTime) < NET_TIMEOUT;
}

int Lan::PeerCount() const {
    return (int)impl->peers.size();
}

std::string Lan::LocalAddresses() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return "";
#endif

    std::string result;
    char name[256];
    if (gethostname(name, sizeof(name)) == 0) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        addrinfo* list = nullptr;
        if (getaddrinfo(name, nullptr, &hints, &list) == 0) {
            for (addrinfo* a = list; a; a = a->ai_next) {
                char ip[INET_ADDRSTRLEN];
                const sockaddr_in* sa = (const sockaddr_in*)a->ai_addr;
                if (inet_ntop(AF_INET, &sa->sin_addr, ip, sizeof(ip)))
                    result += std::string(result.empty() ? "" : ", ") + ip;
            }
            freeaddrinfo(list);
        }
    }
#ifdef _WIN32
    WSACleanup();
#endif
    return result;
}
