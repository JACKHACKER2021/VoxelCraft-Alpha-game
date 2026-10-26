#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace vc::net {

// LAN games announce themselves on this UDP port and listen for players on the TCP one.
constexpr int kDiscoveryPort = 19132;
constexpr int kGamePort = 19133;

// Little-endian message building and reading.
class Writer {
public:
    Writer& u8(uint8_t v) { b_.push_back(v); return *this; }
    Writer& u16(uint16_t v) { return u8(v & 0xff).u8(v >> 8); }
    Writer& u32(uint32_t v) { return u16(v & 0xffff).u16(v >> 16); }
    Writer& i32(int32_t v) { return u32((uint32_t)v); }
    Writer& f32(float v);
    Writer& str(const std::string& s);                  // u16 length + bytes
    Writer& bytes(const std::vector<uint8_t>& data);    // u32 length + bytes
    const std::vector<uint8_t>& data() const { return b_; }

private:
    std::vector<uint8_t> b_;
};

class Reader {
public:
    explicit Reader(const std::vector<uint8_t>& data) : d_(data) {}
    uint8_t u8();
    uint16_t u16();
    uint32_t u32();
    int32_t i32() { return (int32_t)u32(); }
    float f32();
    std::string str();
    std::vector<uint8_t> bytes();
    // False once a read ran past the end (all later reads return zeros).
    bool ok() const { return ok_; }

private:
    bool need(size_t n);
    const std::vector<uint8_t>& d_;
    size_t pos_ = 0;
    bool ok_ = true;
};

struct Packet {
    uint8_t type = 0;
    std::vector<uint8_t> data;
};

// A TCP stream of packets (u32 length, u8 type, payload). Non-blocking: poll() every frame.
class Connection {
public:
    Connection() = default;
    explicit Connection(int fd) : fd_(fd) {}
    ~Connection();
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    // Starts connecting to host:port (an IPv4 address or a name). False if it failed right away.
    bool connect(const std::string& host, int port, std::string& error);
    void send(uint8_t type, const std::vector<uint8_t>& payload);
    void send(uint8_t type, const Writer& w) { send(type, w.data()); }
    // Sends what is queued and reads what arrived. Returns false once the connection is closed.
    bool poll(std::vector<Packet>& received);
    void close();
    bool open() const { return fd_ >= 0; }
    bool connected() const { return fd_ >= 0 && !connecting_; }
    size_t queued() const { return out_.size() - outPos_; }
    const std::string& error() const { return error_; }

private:
    int fd_ = -1;
    bool connecting_ = false;
    std::vector<uint8_t> in_, out_;
    size_t outPos_ = 0;
    std::string error_;
};

// Accepts players on a TCP port.
class Listener {
public:
    ~Listener();
    // Tries `port` and the next few ports. Returns the port in use, or 0.
    int open(int port);
    void close();
    int accept();  // a new socket, or -1
    int port() const { return port_; }

private:
    int fd_ = -1, port_ = 0;
};

// A game someone opened to the LAN.
struct LanGame {
    std::string address;  // IPv4 of the host
    int port = kGamePort;
    std::string host, world;
    int mode = 0;
    int players = 1;
    double lastSeen = 0;
};

// Sends "here is a game" datagrams to every local broadcast address (and to this machine).
class Announcer {
public:
    ~Announcer();
    void announce(const LanGame& game);

private:
    int fd_ = -1;
};

// Listens for announcements; games disappear a few seconds after their last one.
class Discovery {
public:
    ~Discovery();
    bool start();
    void stop();
    bool running() const { return fd_ >= 0; }
    void poll(double now);
    const std::vector<LanGame>& games() const { return games_; }

private:
    int fd_ = -1;
    std::vector<LanGame> games_;
};

// IPv4 addresses of this device (not loopback), to show where others can connect.
std::vector<std::string> localAddresses();

// 1.4.0: how a device can be reached. A phone's own hotspot works with no Wi-Fi router
// and no mobile data at all: the phones talk to each other directly.
enum class AddressKind { Hotspot, Wifi, Internet };
struct LocalAddress {
    std::string ip, iface;
    AddressKind kind;
};
std::vector<LocalAddress> localInterfaces();
AddressKind classifyAddress(const std::string& ip, const std::string& iface);

// Room codes: an IPv4 address and a port packed into ten letters and digits, like
// "VC-7K3PA-QX2MD", easy to read out or type. No server is involved: the code is the address.
std::string roomCode(const std::string& ip, int port);
// Accepts a room code (with or without "VC-" and dashes, any case) or "ip", "ip:port".
bool parseJoinAddress(const std::string& text, std::string& ip, int& port);

}  // namespace vc::net
