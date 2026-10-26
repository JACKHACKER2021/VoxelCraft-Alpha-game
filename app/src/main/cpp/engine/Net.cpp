#include "Net.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace vc::net {

namespace {

constexpr uint32_t kMaxPacket = 32u << 20;  // a whole world of chunks never comes in one packet
const char kAnnounceMagic[] = "VXCLAN1";

void setNonBlocking(int fd) { fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK); }

#ifdef MSG_NOSIGNAL
constexpr int kSendFlags = MSG_NOSIGNAL;
#else
constexpr int kSendFlags = 0;
#endif

}  // namespace

// ---------------------------------------------------------------- Writer / Reader

Writer& Writer::f32(float v) {
    uint32_t u;
    std::memcpy(&u, &v, 4);
    return u32(u);
}

Writer& Writer::str(const std::string& s) {
    size_t n = std::min<size_t>(s.size(), 0xffff);
    u16((uint16_t)n);
    b_.insert(b_.end(), s.begin(), s.begin() + n);
    return *this;
}

Writer& Writer::bytes(const std::vector<uint8_t>& data) {
    u32((uint32_t)data.size());
    b_.insert(b_.end(), data.begin(), data.end());
    return *this;
}

bool Reader::need(size_t n) {
    if (ok_ && pos_ + n <= d_.size()) return true;
    ok_ = false;
    return false;
}

uint8_t Reader::u8() { return need(1) ? d_[pos_++] : 0; }
uint16_t Reader::u16() {
    uint16_t lo = u8();
    return (uint16_t)(lo | (u8() << 8));
}
uint32_t Reader::u32() {
    uint32_t lo = u16();
    return lo | ((uint32_t)u16() << 16);
}
float Reader::f32() {
    uint32_t u = u32();
    float v;
    std::memcpy(&v, &u, 4);
    return std::isfinite(v) ? v : 0.0f;
}
std::string Reader::str() {
    size_t n = u16();
    if (!need(n)) return "";
    std::string s(d_.begin() + pos_, d_.begin() + pos_ + n);
    pos_ += n;
    return s;
}
std::vector<uint8_t> Reader::bytes() {
    size_t n = u32();
    if (!need(n)) return {};
    std::vector<uint8_t> v(d_.begin() + pos_, d_.begin() + pos_ + n);
    pos_ += n;
    return v;
}

// ---------------------------------------------------------------- Connection

Connection::~Connection() { close(); }

void Connection::close() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    connecting_ = false;
}

bool Connection::connect(const std::string& host, int port, std::string& error) {
    close();
    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res) {
        error = "Neznámá adresa";
        return false;
    }
    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        freeaddrinfo(res);
        error = "Síť není dostupná";
        return false;
    }
    setNonBlocking(fd_);
    int one = 1;
    setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    int r = ::connect(fd_, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (r < 0 && errno != EINPROGRESS) {
        close();
        error = "Nelze se připojit";
        return false;
    }
    connecting_ = r < 0;
    return true;
}

void Connection::send(uint8_t type, const std::vector<uint8_t>& payload) {
    if (fd_ < 0) return;
    uint32_t n = (uint32_t)payload.size() + 1;
    for (int i = 0; i < 4; ++i) out_.push_back((uint8_t)(n >> (8 * i)));
    out_.push_back(type);
    out_.insert(out_.end(), payload.begin(), payload.end());
}

bool Connection::poll(std::vector<Packet>& received) {
    if (fd_ < 0) return false;
    if (connecting_) {
        // The non-blocking connect finishes when the socket becomes writable.
        sockaddr_in peer{};
        socklen_t len = sizeof peer;
        if (getpeername(fd_, (sockaddr*)&peer, &len) == 0) {
            connecting_ = false;
        } else {
            int err = 0;
            socklen_t el = sizeof err;
            getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &el);
            if (err != 0 && err != EINPROGRESS && err != EALREADY) {
                error_ = "Nelze se připojit";
                close();
                return false;
            }
            return true;
        }
        int one = 1;
        setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    }
    // Write.
    while (outPos_ < out_.size()) {
        ssize_t n = ::send(fd_, out_.data() + outPos_, out_.size() - outPos_, kSendFlags);
        if (n > 0) {
            outPos_ += (size_t)n;
        } else {
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) break;
            error_ = "Spojení přerušeno";
            close();
            return false;
        }
    }
    if (outPos_ == out_.size()) {
        out_.clear();
        outPos_ = 0;
    } else if (outPos_ > (1u << 20)) {
        out_.erase(out_.begin(), out_.begin() + outPos_);
        outPos_ = 0;
    }
    // Read.
    uint8_t buf[65536];
    for (;;) {
        ssize_t n = ::recv(fd_, buf, sizeof buf, 0);
        if (n > 0) {
            in_.insert(in_.end(), buf, buf + n);
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) break;
        error_ = "Spojení ukončeno";
        close();
        break;
    }
    // Split packets.
    size_t pos = 0;
    while (in_.size() - pos >= 5) {
        uint32_t len = in_[pos] | (in_[pos + 1] << 8) | (in_[pos + 2] << 16) | ((uint32_t)in_[pos + 3] << 24);
        if (len == 0 || len > kMaxPacket) {
            error_ = "Neplatná data ze sítě";
            close();
            return false;
        }
        if (in_.size() - pos - 4 < len) break;
        Packet p;
        p.type = in_[pos + 4];
        p.data.assign(in_.begin() + pos + 5, in_.begin() + pos + 4 + len);
        received.push_back(std::move(p));
        pos += 4 + len;
    }
    in_.erase(in_.begin(), in_.begin() + pos);
    return fd_ >= 0;
}

// ---------------------------------------------------------------- Listener

Listener::~Listener() { close(); }

void Listener::close() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    port_ = 0;
}

int Listener::open(int port) {
    close();
    for (int p = port; p < port + 10; ++p) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) return 0;
        int one = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_ANY);
        a.sin_port = htons((uint16_t)p);
        if (bind(fd, (sockaddr*)&a, sizeof a) == 0 && listen(fd, 8) == 0) {
            setNonBlocking(fd);
            fd_ = fd;
            port_ = p;
            return p;
        }
        ::close(fd);
    }
    return 0;
}

int Listener::accept() {
    if (fd_ < 0) return -1;
    int fd = ::accept(fd_, nullptr, nullptr);
    if (fd < 0) return -1;
    setNonBlocking(fd);
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    return fd;
}

// ---------------------------------------------------------------- discovery

std::vector<std::string> localAddresses() {
    std::vector<std::string> out;
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0) return out;
    for (ifaddrs* i = list; i; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || (i->ifa_flags & IFF_LOOPBACK) || !(i->ifa_flags & IFF_UP))
            continue;
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &((sockaddr_in*)i->ifa_addr)->sin_addr, buf, sizeof buf);
        if (std::find(out.begin(), out.end(), buf) == out.end()) out.push_back(buf);
    }
    freeifaddrs(list);
    return out;
}

std::vector<LocalAddress> localInterfaces() {
    std::vector<LocalAddress> out;
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0) return out;
    for (ifaddrs* i = list; i; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || (i->ifa_flags & IFF_LOOPBACK) || !(i->ifa_flags & IFF_UP))
            continue;
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &((sockaddr_in*)i->ifa_addr)->sin_addr, buf, sizeof buf);
        const std::string name = i->ifa_name ? i->ifa_name : "";
        bool seen = false;
        for (const LocalAddress& a : out) seen |= a.ip == buf;
        if (!seen) out.push_back({buf, name, classifyAddress(buf, name)});
    }
    freeifaddrs(list);
    // The hotspot first: it is the one to share when there is no Wi-Fi.
    std::stable_sort(out.begin(), out.end(), [](const LocalAddress& a, const LocalAddress& b) { return a.kind < b.kind; });
    return out;
}

AddressKind classifyAddress(const std::string& ip, const std::string& iface) {
    // Android names its hotspot interface ap0, swlan0, softap0 or wlan1; older phones use 192.168.43.x.
    const bool apName = iface.rfind("ap", 0) == 0 || iface.rfind("swlan", 0) == 0 || iface.rfind("softap", 0) == 0 ||
                        iface == "wlan1";
    if (apName || ip.rfind("192.168.43.", 0) == 0 || ip.rfind("172.20.10.", 0) == 0) return AddressKind::Hotspot;
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (std::sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return AddressKind::Internet;
    const bool priv = a == 10 || (a == 172 && b >= 16 && b <= 31) || (a == 192 && b == 168) || (a == 100 && b >= 64 && b <= 127);
    return priv ? AddressKind::Wifi : AddressKind::Internet;
}

namespace {
const char kCodeDigits[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";  // no 0/O, 1/I: easy to read out
}

std::string roomCode(const std::string& ip, int port) {
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (std::sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4 || a > 255 || b > 255 || c > 255 || d > 255) return "";
    uint64_t v = ((uint64_t)a << 40) | ((uint64_t)b << 32) | ((uint64_t)c << 24) | ((uint64_t)d << 16) | (uint64_t)(port & 0xFFFF);
    // A light scramble so neighbouring addresses do not look alike.
    v ^= 0x5A3C96E1B7D2ull & 0xFFFFFFFFFFFFull;
    std::string digits(10, '2');
    for (int i = 9; i >= 0; --i) {
        digits[(size_t)i] = kCodeDigits[v & 31];
        v >>= 5;
    }
    return "VC-" + digits.substr(0, 5) + "-" + digits.substr(5);
}

bool parseJoinAddress(const std::string& text, std::string& ip, int& port) {
    std::string t;
    for (char ch : text)
        if (ch != ' ' && ch != '\t' && ch != '\n') t += ch;
    // A room code: ten code letters after removing "VC" and dashes.
    std::string code;
    {
        std::string u;
        for (char ch : t) u += (char)std::toupper((unsigned char)ch);
        if (u.rfind("VC-", 0) == 0) u = u.substr(3);
        for (char ch : u)
            if (ch != '-') code += ch;
    }
    if (code.size() == 10 && code.find('.') == std::string::npos && code.find(':') == std::string::npos) {
        uint64_t v = 0;
        for (char ch : code) {
            const char* p = std::strchr(kCodeDigits, ch);
            if (!p || !*p) return false;
            v = (v << 5) | (uint64_t)(p - kCodeDigits);
        }
        v ^= 0x5A3C96E1B7D2ull & 0xFFFFFFFFFFFFull;
        char buf[32];
        std::snprintf(buf, sizeof buf, "%u.%u.%u.%u", (unsigned)(v >> 40 & 255), (unsigned)(v >> 32 & 255), (unsigned)(v >> 24 & 255),
                      (unsigned)(v >> 16 & 255));
        ip = buf;
        port = (int)(v & 0xFFFF);
        return port > 0;
    }
    // "ip" or "ip:port"
    ip = t;
    port = kGamePort;
    if (size_t colon = t.rfind(':'); colon != std::string::npos) {
        ip = t.substr(0, colon);
        port = std::atoi(t.c_str() + colon + 1);
    }
    return !ip.empty() && port > 0 && port < 65536;
}

Announcer::~Announcer() {
    if (fd_ >= 0) ::close(fd_);
}

void Announcer::announce(const LanGame& game) {
    if (fd_ < 0) {
        fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd_ < 0) return;
        int one = 1;
        setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, &one, sizeof one);
        setNonBlocking(fd_);
    }
    std::string msg = std::string(kAnnounceMagic) + "\n" + std::to_string(game.port) + "\n" + game.host + "\n" +
                      game.world + "\n" + std::to_string(game.mode) + "\n" + std::to_string(game.players);
    std::vector<in_addr_t> targets = {htonl(INADDR_BROADCAST), htonl(INADDR_LOOPBACK)};
    ifaddrs* list = nullptr;
    if (getifaddrs(&list) == 0) {
        for (ifaddrs* i = list; i; i = i->ifa_next)
            if (i->ifa_addr && i->ifa_addr->sa_family == AF_INET && (i->ifa_flags & IFF_BROADCAST) && i->ifa_broadaddr)
                targets.push_back(((sockaddr_in*)i->ifa_broadaddr)->sin_addr.s_addr);
        freeifaddrs(list);
    }
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    for (in_addr_t t : targets) {
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = t;
        a.sin_port = htons(kDiscoveryPort);
        sendto(fd_, msg.data(), msg.size(), kSendFlags, (sockaddr*)&a, sizeof a);
    }
}

Discovery::~Discovery() { stop(); }

bool Discovery::start() {
    if (fd_ >= 0) return true;
    fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd_ < 0) return false;
    int one = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
#ifdef SO_REUSEPORT
    setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &one, sizeof one);
#endif
    setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, &one, sizeof one);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    a.sin_port = htons(kDiscoveryPort);
    if (bind(fd_, (sockaddr*)&a, sizeof a) != 0) {
        stop();
        return false;
    }
    setNonBlocking(fd_);
    return true;
}

void Discovery::stop() {
    if (fd_ >= 0) ::close(fd_);
    fd_ = -1;
    games_.clear();
}

void Discovery::poll(double now) {
    if (fd_ < 0) return;
    char buf[1024];
    for (;;) {
        sockaddr_in from{};
        socklen_t len = sizeof from;
        ssize_t n = recvfrom(fd_, buf, sizeof buf - 1, 0, (sockaddr*)&from, &len);
        if (n <= 0) break;
        buf[n] = 0;
        std::vector<std::string> f;
        std::string cur;
        for (ssize_t i = 0; i < n; ++i) {
            if (buf[i] == '\n') {
                f.push_back(cur);
                cur.clear();
            } else {
                cur += buf[i];
            }
        }
        f.push_back(cur);
        if (f.size() < 6 || f[0] != kAnnounceMagic) continue;
        LanGame g;
        char addr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from.sin_addr, addr, sizeof addr);
        g.address = addr;
        g.port = std::atoi(f[1].c_str());
        g.host = f[2].substr(0, 40);
        g.world = f[3].substr(0, 40);
        g.mode = std::atoi(f[4].c_str());
        g.players = std::atoi(f[5].c_str());
        g.lastSeen = now;
        if (g.port <= 0 || g.port > 65535) continue;
        // The same game may arrive over loopback and the LAN; keep one entry per host and port.
        auto same = [&](const LanGame& o) { return o.port == g.port && o.host == g.host && o.world == g.world; };
        auto it = std::find_if(games_.begin(), games_.end(), same);
        if (it == games_.end()) {
            games_.push_back(g);
        } else {
            bool keepAddress = it->address != "127.0.0.1" && g.address == "127.0.0.1";
            std::string a = it->address;
            *it = g;
            if (keepAddress) it->address = a;
        }
    }
    games_.erase(std::remove_if(games_.begin(), games_.end(), [&](const LanGame& g) { return now - g.lastSeen > 6; }),
                 games_.end());
}

}  // namespace vc::net
