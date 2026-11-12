#if defined(PH_IMPLEMENTATION) && defined(__linux__) && !defined(_DEFAULT_SOURCE)
#define _DEFAULT_SOURCE
#endif

#ifndef PH_NET_H
#define PH_NET_H

#include <SDL3/SDL.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ph_setting;

enum {
    PH_NET_OFF,
    PH_NET_HOSTING,
    PH_NET_JOINING,
    PH_NET_CONNECTED,
    PH_NET_FAILED,
};

enum {
    PH_NET_WAIT,
    PH_NET_READY,
    PH_NET_LOST,
};

enum {
    PH_NET_NAME = 32,
    PH_NET_ADDRESS = 48,
    PH_NET_MESSAGE = 192,
    PH_NET_RULES = 64,
    PH_NET_GAMES = 16,
    PH_NET_HISTORY = 512,
    PH_NET_CHECKS = 8,
    PH_NET_RTT_SAMPLES = 4,
    PH_NET_MAX_DELAY = 12,
    PH_NET_PORTS = 10,
    PH_NET_SETTINGS = 2,
    PH_NET_PACKET = 600,
    PH_NET_DELAYED = 64,
};

struct ph_net_game {
    char name[PH_NET_NAME];
    char address[PH_NET_ADDRESS];
    uint32_t ip;
    uint16_t port;
    uint32_t id;
    int other_version;
    uint64_t seen;
};

struct ph_net_check {
    int64_t frame;
    uint32_t hash;
};

struct ph_net_delayed {
    uint64_t due;
    int size;
    uint8_t data[PH_NET_PACKET];
};

struct ph_net {
    char magic[4];
    uint16_t port;
    uint32_t identity;
    float frame_rate;
    int requested_delay;
    int check_interval;
    char name[PH_NET_NAME];
    uint8_t rules[PH_NET_RULES];
    int rules_size;
    uint8_t peer_rules[PH_NET_RULES];
    int peer_rules_size;
    char peer_name[PH_NET_NAME];
    char own_address[PH_NET_ADDRESS];
    char message[PH_NET_MESSAGE];
    int status;
    int searching;
    int local_player;
    intptr_t game_socket;
    intptr_t listen_socket;
    intptr_t search_socket;
    uint16_t game_port;
    uint32_t peer_ip;
    uint16_t peer_port;
    int has_peer;
    uint32_t id;
    uint32_t session;
    uint32_t nonce;
    uint32_t peer_nonce;
    uint32_t probe_echo;
    int peer_requested_delay;
    uint32_t rtt[PH_NET_RTT_SAMPLES];
    int rtt_count;
    int delay;
    uint64_t started;
    uint64_t first_hello;
    uint64_t last_hello;
    uint64_t last_receive;
    uint64_t last_send;
    uint64_t last_look;
    struct ph_net_game games[PH_NET_GAMES];
    int game_count;
    int in_game;
    int playing;
    int peer_left;
    int64_t frame;
    int64_t local_through;
    int64_t remote_through;
    int64_t remote_ack;
    int64_t verified;
    uint32_t local_inputs[PH_NET_HISTORY];
    uint32_t remote_inputs[PH_NET_HISTORY];
    struct ph_net_check local_checks[PH_NET_CHECKS];
    struct ph_net_check remote_checks[PH_NET_CHECKS];
    uint64_t sent;
    uint64_t received;
    uint64_t resends;
    uint64_t stalls;
    uint64_t stall_us;
    uint64_t last_stall_us;
    uint64_t stall_started;
    uint64_t checks_matched;
    int64_t desync_frame;
    int loss;
    int latency;
    uint64_t dropped;
    int delayed_count;
    struct ph_net_delayed delayed[PH_NET_DELAYED];
};

void ph_net_init(struct ph_net *net, const char *magic, uint16_t port, uint32_t identity, float frame_rate);
int ph_net_host(struct ph_net *net);
int ph_net_join(struct ph_net *net, const char *address);
int ph_net_join_game(struct ph_net *net, int index);
void ph_net_search(struct ph_net *net);
void ph_net_stop_search(struct ph_net *net);
void ph_net_update(struct ph_net *net);
void ph_net_begin(struct ph_net *net);
int ph_net_frame(struct ph_net *net, uint32_t local, uint32_t inputs[2]);
int ph_net_check_due(const struct ph_net *net);
void ph_net_check(struct ph_net *net, uint32_t hash);
void ph_net_wait(struct ph_net *net, int milliseconds);
void ph_net_stop(struct ph_net *net);
int ph_net_playing(const struct ph_net *net);
int ph_net_stalled(const struct ph_net *net);
const uint8_t *ph_net_agreed_rules(const struct ph_net *net);
int ph_net_settings(struct ph_net *net, struct ph_setting *settings);
int ph_net_take(struct ph_net *net, const char *key, const char *value);
void ph_net_write(SDL_IOStream *file, struct ph_net *net);

#ifdef __cplusplus
}
#endif

#endif

#if defined(PH_IMPLEMENTATION) && !defined(PH_NET_IMPLEMENTED)
#define PH_NET_IMPLEMENTED

#if defined(_WIN32)
#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#if defined(_MSC_VER)
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#endif
typedef SOCKET ph_socket;
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#if !defined(__ANDROID__) || __ANDROID_API__ >= 24
#include <ifaddrs.h>
#define PH_NET_IFADDRS 1
#endif
typedef int ph_socket;
#endif

#include "ph_settings.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum {
    PH_NET_NO_SOCKET = -1,
    PH_NET_PROTOCOL = 1,
    PH_NET_HEADER = 10,
    PH_NET_REASON = 96,
    PH_NET_INPUTS_PER_PACKET = 64,
    PH_NET_PACKET_CHECKS = 2,
    PH_NET_INTERFACES = 16,
    PH_NET_LOOK_MS = 1000,
    PH_NET_FORGET_MS = 4000,
    PH_NET_HELLO_MS = 100,
    PH_NET_PROBE_MS = 1500,
    PH_NET_JOIN_MS = 10000,
    PH_NET_SILENT_MS = 8000,
    PH_NET_RESEND_MS = 10,
    PH_NET_MIN_DELAY = 2,
    PH_NET_STALL_NOTICE_MS = 250,
    PH_NET_BYE_COPIES = 4,
    PH_NET_DRAIN = 256,
};

enum {
    PH_PACKET_LOOK = 1,
    PH_PACKET_HERE,
    PH_PACKET_HELLO,
    PH_PACKET_PROBE,
    PH_PACKET_WELCOME,
    PH_PACKET_REFUSE,
    PH_PACKET_INPUT,
    PH_PACKET_BYE,
};

struct ph_packet {
    uint8_t data[PH_NET_PACKET];
    int size;
    int at;
    int bad;
};

static const uint8_t ph_net_group[4] = { 239, 255, 80, 72 };

static uint64_t ph_net_now(void)
{
    return SDL_GetTicks();
}

static uint64_t ph_net_now_us(void)
{
    return SDL_GetTicksNS() / 1000;
}

static int ph_net_start(void)
{
#if defined(_WIN32)
    static int started;
    WSADATA data;
    if (!started) {
        started = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }
    return started;
#else
    return 1;
#endif
}

static void ph_net_close(intptr_t *handle)
{
    if (*handle == PH_NET_NO_SOCKET) {
        return;
    }
#if defined(_WIN32)
    closesocket((ph_socket)*handle);
#else
    close((ph_socket)*handle);
#endif
    *handle = PH_NET_NO_SOCKET;
}

static struct sockaddr_in ph_net_address(uint32_t ip, uint16_t port)
{
    struct sockaddr_in address;
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(ip);
    return address;
}

static intptr_t ph_net_open(uint16_t port, int shared)
{
    ph_socket handle = socket(AF_INET, SOCK_DGRAM, 0);
    int yes = 1;
#if defined(_WIN32)
    if (handle == INVALID_SOCKET) {
        return PH_NET_NO_SOCKET;
    }
    u_long nonblocking = 1;
    ioctlsocket(handle, FIONBIO, &nonblocking);
#else
    if (handle < 0) {
        return PH_NET_NO_SOCKET;
    }
    fcntl(handle, F_SETFL, fcntl(handle, F_GETFL, 0) | O_NONBLOCK);
#endif
    setsockopt(handle, SOL_SOCKET, SO_BROADCAST, (const char *)&yes, sizeof yes);
    if (shared) {
        setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof yes);
#if defined(SO_REUSEPORT) && !defined(_WIN32)
        setsockopt(handle, SOL_SOCKET, SO_REUSEPORT, (const char *)&yes, sizeof yes);
#endif
    }
    struct sockaddr_in address = ph_net_address(INADDR_ANY, port);
    intptr_t result = (intptr_t)handle;
    if (bind(handle, (struct sockaddr *)&address, sizeof address) != 0) {
        ph_net_close(&result);
    }
    return result;
}

static uint16_t ph_net_bound_port(intptr_t handle)
{
    struct sockaddr_in address;
    socklen_t size = sizeof address;
    if (getsockname((ph_socket)handle, (struct sockaddr *)&address, &size) != 0) {
        return 0;
    }
    return ntohs(address.sin_port);
}

static void ph_net_send_bytes(struct ph_net *net, intptr_t handle, const uint8_t *data, int size, uint32_t ip, uint16_t port)
{
    if (handle == PH_NET_NO_SOCKET) {
        return;
    }
    struct sockaddr_in address = ph_net_address(ip, port);
    if (sendto((ph_socket)handle, (const char *)data, size, 0, (const struct sockaddr *)&address, sizeof address) == size) {
        net->sent++;
    }
}

static void ph_net_send_to(struct ph_net *net, intptr_t handle, const struct ph_packet *packet, uint32_t ip, uint16_t port)
{
    ph_net_send_bytes(net, handle, packet->data, packet->size, ip, port);
}

static void ph_net_send_late(struct ph_net *net, const struct ph_packet *packet)
{
    if (net->loss && (int)(SDL_rand_bits() % 100) < net->loss) {
        net->dropped++;
        return;
    }
    if (net->latency <= 0 || net->delayed_count >= PH_NET_DELAYED) {
        ph_net_send_to(net, net->game_socket, packet, net->peer_ip, net->peer_port);
        return;
    }
    struct ph_net_delayed *delayed = &net->delayed[net->delayed_count++];
    delayed->due = ph_net_now() + (uint64_t)(net->latency / 2) + SDL_rand_bits() % (uint32_t)(net->latency / 2 + 1);
    delayed->size = packet->size;
    SDL_memcpy(delayed->data, packet->data, (size_t)packet->size);
}

static void ph_net_release(struct ph_net *net)
{
    int kept = 0;
    uint64_t now = ph_net_now();
    for (int index = 0; index < net->delayed_count; index++) {
        struct ph_net_delayed *delayed = &net->delayed[index];
        if (delayed->due <= now) {
            ph_net_send_bytes(net, net->game_socket, delayed->data, delayed->size, net->peer_ip, net->peer_port);
        } else {
            net->delayed[kept++] = *delayed;
        }
    }
    net->delayed_count = kept;
}

static int ph_net_receive(intptr_t handle, struct ph_packet *packet, uint32_t *ip, uint16_t *port)
{
    struct sockaddr_in address;
    socklen_t size = sizeof address;
    int received = (int)recvfrom((ph_socket)handle, (char *)packet->data, PH_NET_PACKET, 0, (struct sockaddr *)&address, &size);
    if (received <= 0) {
        return 0;
    }
    packet->size = received;
    packet->at = 0;
    packet->bad = 0;
    *ip = ntohl(address.sin_addr.s_addr);
    *port = ntohs(address.sin_port);
    return 1;
}

static void ph_net_format(uint32_t ip, char *text, int size)
{
    SDL_snprintf(text, (size_t)size, "%u.%u.%u.%u", ip >> 24, ip >> 16 & 0xFF, ip >> 8 & 0xFF, ip & 0xFF);
}

static int ph_net_interfaces(uint32_t *addresses, uint32_t *broadcasts, int limit)
{
    int count = 0;
#if defined(_WIN32)
    ULONG size = 32768;
    IP_ADAPTER_ADDRESSES *adapters = (IP_ADAPTER_ADDRESSES *)SDL_malloc(size);
    if (!adapters || GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER, NULL, adapters, &size) != NO_ERROR) {
        SDL_free(adapters);
        return 0;
    }
    for (IP_ADAPTER_ADDRESSES *adapter = adapters; adapter && count < limit; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }
        for (IP_ADAPTER_UNICAST_ADDRESS *unicast = adapter->FirstUnicastAddress; unicast && count < limit; unicast = unicast->Next) {
            if (unicast->Address.lpSockaddr->sa_family != AF_INET) {
                continue;
            }
            uint32_t ip = ntohl(((struct sockaddr_in *)unicast->Address.lpSockaddr)->sin_addr.s_addr);
            int prefix = unicast->OnLinkPrefixLength;
            uint32_t mask = prefix <= 0 ? 0 : prefix >= 32 ? 0xFFFFFFFFu : 0xFFFFFFFFu << (32 - prefix);
            addresses[count] = ip;
            broadcasts[count++] = ip | ~mask;
        }
    }
    SDL_free(adapters);
#elif defined(PH_NET_IFADDRS)
    struct ifaddrs *found = NULL;
    if (getifaddrs(&found) != 0) {
        return 0;
    }
    for (struct ifaddrs *entry = found; entry && count < limit; entry = entry->ifa_next) {
        if (!entry->ifa_addr || entry->ifa_addr->sa_family != AF_INET || !(entry->ifa_flags & IFF_UP) || (entry->ifa_flags & IFF_LOOPBACK)) {
            continue;
        }
        addresses[count] = ntohl(((struct sockaddr_in *)entry->ifa_addr)->sin_addr.s_addr);
        int broadcasts_here = (entry->ifa_flags & IFF_BROADCAST) && entry->ifa_broadaddr;
        broadcasts[count++] = broadcasts_here ? ntohl(((struct sockaddr_in *)entry->ifa_broadaddr)->sin_addr.s_addr) : 0;
    }
    freeifaddrs(found);
#else
    (void)addresses;
    (void)broadcasts;
    (void)limit;
#endif
    return count;
}

static uint32_t ph_net_route_address(void)
{
    intptr_t handle = ph_net_open(0, 0);
    uint32_t ip = 0;
    if (handle == PH_NET_NO_SOCKET) {
        return 0;
    }
    struct sockaddr_in target = ph_net_address(0x08080808u, 53);
    struct sockaddr_in local;
    socklen_t size = sizeof local;
    if (connect((ph_socket)handle, (struct sockaddr *)&target, sizeof target) == 0 && getsockname((ph_socket)handle, (struct sockaddr *)&local, &size) == 0) {
        ip = ntohl(local.sin_addr.s_addr);
    }
    ph_net_close(&handle);
    return ip;
}

static void ph_net_find_address(struct ph_net *net)
{
    uint32_t addresses[PH_NET_INTERFACES];
    uint32_t broadcasts[PH_NET_INTERFACES];
    uint32_t ip = ph_net_interfaces(addresses, broadcasts, PH_NET_INTERFACES) > 0 ? addresses[0] : ph_net_route_address();
    if (!ip) {
        SDL_strlcpy(net->own_address, "No network", sizeof net->own_address);
        return;
    }
    ph_net_format(ip, net->own_address, sizeof net->own_address);
    if (net->game_port && net->game_port != net->port + 1) {
        char port[8];
        SDL_snprintf(port, sizeof port, ":%u", net->game_port);
        SDL_strlcat(net->own_address, port, sizeof net->own_address);
    }
}

static void ph_put8(struct ph_packet *packet, uint32_t value)
{
    if (packet->size < PH_NET_PACKET) {
        packet->data[packet->size++] = (uint8_t)value;
    }
}

static void ph_put16(struct ph_packet *packet, uint32_t value)
{
    ph_put8(packet, value >> 8);
    ph_put8(packet, value);
}

static void ph_put32(struct ph_packet *packet, uint32_t value)
{
    ph_put16(packet, value >> 16);
    ph_put16(packet, value);
}

static void ph_put_text(struct ph_packet *packet, const char *text, int size)
{
    size_t length = SDL_strlen(text);
    for (int index = 0; index < size; index++) {
        ph_put8(packet, (size_t)index < length && index < size - 1 ? (uint8_t)text[index] : 0);
    }
}

static uint32_t ph_get8(struct ph_packet *packet)
{
    if (packet->at >= packet->size) {
        packet->bad = 1;
        return 0;
    }
    return packet->data[packet->at++];
}

static uint32_t ph_get16(struct ph_packet *packet)
{
    uint32_t high = ph_get8(packet);
    return high << 8 | ph_get8(packet);
}

static uint32_t ph_get32(struct ph_packet *packet)
{
    uint32_t high = ph_get16(packet);
    return high << 16 | ph_get16(packet);
}

static void ph_get_text(struct ph_packet *packet, char *text, int size)
{
    for (int index = 0; index < size; index++) {
        uint32_t letter = ph_get8(packet);
        text[index] = letter >= 32 && letter < 127 ? (char)letter : 0;
    }
    text[size - 1] = 0;
}

static struct ph_packet ph_net_start_packet(const struct ph_net *net, int type, uint32_t session)
{
    struct ph_packet packet;
    packet.size = 0;
    packet.at = 0;
    packet.bad = 0;
    for (int index = 0; index < 4; index++) {
        ph_put8(&packet, (uint8_t)net->magic[index]);
    }
    ph_put8(&packet, PH_NET_PROTOCOL);
    ph_put8(&packet, (uint32_t)type);
    ph_put32(&packet, session);
    return packet;
}

static void ph_net_say(struct ph_net *net, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    SDL_vsnprintf(net->message, sizeof net->message, format, arguments);
    va_end(arguments);
}

static void ph_net_send_inputs(struct ph_net *net, int bye)
{
    struct ph_packet packet = ph_net_start_packet(net, bye ? PH_PACKET_BYE : PH_PACKET_INPUT, net->session);
    int64_t first = net->remote_ack + 1;
    int64_t count = SDL_clamp(net->local_through - first + 1, 0, PH_NET_INPUTS_PER_PACKET);
    ph_put32(&packet, (uint32_t)first);
    ph_put8(&packet, (uint32_t)count);
    ph_put32(&packet, (uint32_t)(int32_t)net->remote_through);
    int interval = net->check_interval;
    int64_t latest = net->frame / interval * interval;
    for (int index = 0; index < PH_NET_PACKET_CHECKS; index++) {
        int64_t frame = latest - index * interval;
        const struct ph_net_check *check = frame >= 0 ? &net->local_checks[frame / interval % PH_NET_CHECKS] : NULL;
        int known = check && check->frame == frame;
        ph_put32(&packet, known ? (uint32_t)frame : 0xFFFFFFFFu);
        ph_put32(&packet, known ? check->hash : 0);
    }
    for (int64_t frame = first; frame < first + count; frame++) {
        ph_put32(&packet, net->local_inputs[frame % PH_NET_HISTORY]);
    }
    if (bye) {
        ph_net_send_to(net, net->game_socket, &packet, net->peer_ip, net->peer_port);
    } else {
        ph_net_send_late(net, &packet);
    }
    net->last_send = ph_net_now();
}

static void ph_net_send_bye(struct ph_net *net)
{
    if (net->status != PH_NET_CONNECTED || net->game_socket == PH_NET_NO_SOCKET) {
        return;
    }
    for (int copy = 0; copy < PH_NET_BYE_COPIES; copy++) {
        ph_net_send_inputs(net, 1);
    }
}

static void ph_net_close_session(struct ph_net *net)
{
    ph_net_close(&net->game_socket);
    ph_net_close(&net->listen_socket);
    net->in_game = 0;
    net->delayed_count = 0;
}

static void ph_net_fail(struct ph_net *net, const char *format, ...)
{
    ph_net_send_bye(net);
    va_list arguments;
    va_start(arguments, format);
    SDL_vsnprintf(net->message, sizeof net->message, format, arguments);
    va_end(arguments);
    ph_net_close_session(net);
    net->status = PH_NET_FAILED;
}

static const char *ph_net_peer(const struct ph_net *net)
{
    return net->peer_name[0] ? net->peer_name : "The other player";
}

static void ph_net_compare(struct ph_net *net)
{
    for (int index = 0; index < PH_NET_CHECKS && net->status == PH_NET_CONNECTED; index++) {
        struct ph_net_check local = net->local_checks[index];
        struct ph_net_check remote = net->remote_checks[index];
        if (local.frame < 0 || local.frame != remote.frame || local.frame <= net->verified) {
            continue;
        }
        if (local.hash != remote.hash) {
            net->desync_frame = local.frame;
            ph_net_fail(net, "The two games went out of sync at frame %lld.", (long long)local.frame);
            return;
        }
        net->verified = local.frame;
        net->checks_matched++;
    }
}

static void ph_net_read_inputs(struct ph_net *net, struct ph_packet *packet, int bye)
{
    int64_t first = ph_get32(packet);
    int count = (int)ph_get8(packet);
    int64_t ack = (int32_t)ph_get32(packet);
    struct ph_net_check checks[PH_NET_PACKET_CHECKS];
    for (int index = 0; index < PH_NET_PACKET_CHECKS; index++) {
        uint32_t frame = ph_get32(packet);
        checks[index].frame = frame == 0xFFFFFFFFu ? -1 : (int64_t)frame;
        checks[index].hash = ph_get32(packet);
    }
    if (packet->bad || packet->size - packet->at < count * 4) {
        return;
    }
    net->received++;
    net->last_receive = ph_net_now();
    net->remote_ack = SDL_max(net->remote_ack, SDL_min(ack, net->local_through));
    for (int index = 0; index < count; index++) {
        uint32_t input = ph_get32(packet);
        int64_t frame = first + index;
        if (frame == net->remote_through + 1 && frame < net->frame + PH_NET_HISTORY) {
            net->remote_inputs[frame % PH_NET_HISTORY] = input;
            net->remote_through = frame;
        }
    }
    if (bye) {
        net->peer_left = 1;
    }
    for (int index = 0; index < PH_NET_PACKET_CHECKS; index++) {
        if (checks[index].frame >= 0) {
            net->remote_checks[checks[index].frame / net->check_interval % PH_NET_CHECKS] = checks[index];
        }
    }
    ph_net_compare(net);
}

static void ph_net_send_here(struct ph_net *net, uint32_t ip, uint16_t port)
{
    struct ph_packet packet = ph_net_start_packet(net, PH_PACKET_HERE, 0);
    ph_put16(&packet, net->game_port);
    ph_put32(&packet, net->id);
    ph_put32(&packet, net->identity);
    ph_put_text(&packet, net->name, PH_NET_NAME);
    ph_net_send_to(net, net->listen_socket, &packet, ip, port);
}

static void ph_net_send_refuse(struct ph_net *net, uint32_t ip, uint16_t port, const char *reason)
{
    struct ph_packet packet = ph_net_start_packet(net, PH_PACKET_REFUSE, 0);
    ph_put_text(&packet, reason, PH_NET_REASON);
    ph_net_send_to(net, net->game_socket, &packet, ip, port);
}

static void ph_net_send_hello(struct ph_net *net)
{
    struct ph_packet packet = ph_net_start_packet(net, PH_PACKET_HELLO, 0);
    ph_put32(&packet, net->identity);
    ph_put32(&packet, net->nonce);
    ph_put32(&packet, net->probe_echo);
    ph_put8(&packet, (uint32_t)net->requested_delay);
    ph_put_text(&packet, net->name, PH_NET_NAME);
    ph_net_send_to(net, net->game_socket, &packet, net->peer_ip, net->peer_port);
    net->probe_echo = 0;
    net->last_hello = ph_net_now();
}

static void ph_net_send_welcome(struct ph_net *net)
{
    struct ph_packet packet = ph_net_start_packet(net, PH_PACKET_WELCOME, net->session);
    ph_put32(&packet, net->identity);
    ph_put32(&packet, net->peer_nonce);
    ph_put8(&packet, (uint32_t)net->delay);
    ph_put_text(&packet, net->name, PH_NET_NAME);
    ph_put8(&packet, (uint32_t)net->rules_size);
    for (int index = 0; index < net->rules_size; index++) {
        ph_put8(&packet, net->rules[index]);
    }
    ph_net_send_to(net, net->game_socket, &packet, net->peer_ip, net->peer_port);
}

static int ph_net_chosen_delay(const struct ph_net *net)
{
    int requested = SDL_max(net->requested_delay, net->peer_requested_delay);
    if (requested > 0) {
        return SDL_clamp(requested, 1, PH_NET_MAX_DELAY);
    }
    uint32_t sorted[PH_NET_RTT_SAMPLES] = { 0 };
    for (int index = 0; index < net->rtt_count; index++) {
        int place = index;
        for (; place > 0 && sorted[place - 1] > net->rtt[index]; place--) {
            sorted[place] = sorted[place - 1];
        }
        sorted[place] = net->rtt[index];
    }
    float frame_ms = 1000.0f / (net->frame_rate > 0 ? net->frame_rate : 60);
    float median_ms = (float)sorted[net->rtt_count / 2];
    int delay = (int)SDL_ceilf(median_ms / 2.0f / frame_ms) + 2;
    return SDL_clamp(delay, PH_NET_MIN_DELAY, PH_NET_MAX_DELAY);
}

static void ph_net_prepare(struct ph_net *net)
{
    net->in_game = 0;
    net->peer_left = 0;
    net->frame = 0;
    net->local_through = net->delay - 1;
    net->remote_through = net->delay - 1;
    net->remote_ack = net->delay - 1;
    net->verified = -1;
    SDL_zeroa(net->local_inputs);
    SDL_zeroa(net->remote_inputs);
    for (int index = 0; index < PH_NET_CHECKS; index++) {
        net->local_checks[index] = (struct ph_net_check){ -1, 0 };
        net->remote_checks[index] = (struct ph_net_check){ -1, 0 };
    }
    net->last_receive = ph_net_now();
    net->stall_started = 0;
    net->stalls = 0;
    net->stall_us = 0;
    net->last_stall_us = 0;
    net->resends = 0;
    net->checks_matched = 0;
    net->dropped = 0;
}

static void ph_net_host_hello(struct ph_net *net, struct ph_packet *packet, uint32_t ip, uint16_t port)
{
    uint32_t identity = ph_get32(packet);
    uint32_t nonce = ph_get32(packet);
    uint32_t echo = ph_get32(packet);
    int requested = (int)ph_get8(packet);
    char name[PH_NET_NAME];
    ph_get_text(packet, name, sizeof name);
    if (packet->bad) {
        return;
    }
    int same = net->has_peer && ip == net->peer_ip && port == net->peer_port && nonce == net->peer_nonce;
    if (net->status == PH_NET_CONNECTED) {
        if (same) {
            ph_net_send_welcome(net);
        } else {
            ph_net_send_refuse(net, ip, port, "That game already has two players.");
        }
        return;
    }
    if (identity != net->identity) {
        ph_net_send_refuse(net, ip, port, "The two games are different versions or use different ROMs.");
        ph_net_say(net, "%s tried to join with a different version or ROM.", name[0] ? name : "A player");
        return;
    }
    if (!same) {
        net->peer_ip = ip;
        net->peer_port = port;
        net->peer_nonce = nonce;
        net->has_peer = 1;
        net->rtt_count = 0;
        net->first_hello = ph_net_now();
    }
    SDL_strlcpy(net->peer_name, name[0] ? name : "Player 2", sizeof net->peer_name);
    net->peer_requested_delay = requested;
    if (echo && net->rtt_count < PH_NET_RTT_SAMPLES) {
        net->rtt[net->rtt_count++] = (uint32_t)ph_net_now() + 1 - echo;
    }
    if (net->rtt_count < PH_NET_RTT_SAMPLES && ph_net_now() - net->first_hello < PH_NET_PROBE_MS) {
        struct ph_packet probe = ph_net_start_packet(net, PH_PACKET_PROBE, 0);
        ph_put32(&probe, nonce);
        ph_put32(&probe, (uint32_t)ph_net_now() + 1);
        ph_net_send_to(net, net->game_socket, &probe, ip, port);
        return;
    }
    net->delay = ph_net_chosen_delay(net);
    net->session = SDL_rand_bits() | 1;
    net->local_player = 0;
    ph_net_prepare(net);
    net->status = PH_NET_CONNECTED;
    ph_net_close(&net->listen_socket);
    ph_net_send_welcome(net);
    ph_net_say(net, "%s joined. Input delay %d frames.", net->peer_name, net->delay);
}

static void ph_net_join_welcome(struct ph_net *net, struct ph_packet *packet, uint32_t session)
{
    uint32_t identity = ph_get32(packet);
    uint32_t nonce = ph_get32(packet);
    int delay = (int)ph_get8(packet);
    char name[PH_NET_NAME];
    ph_get_text(packet, name, sizeof name);
    int sent_rules = (int)ph_get8(packet);
    int rules_size = SDL_min(sent_rules, PH_NET_RULES);
    uint8_t rules[PH_NET_RULES];
    for (int index = 0; index < rules_size; index++) {
        rules[index] = (uint8_t)ph_get8(packet);
    }
    if (packet->bad || nonce != net->nonce || net->status != PH_NET_JOINING) {
        return;
    }
    if (identity != net->identity) {
        ph_net_fail(net, "Cannot join: the two games are different versions or use different ROMs.");
        return;
    }
    SDL_strlcpy(net->peer_name, name[0] ? name : "Player 1", sizeof net->peer_name);
    SDL_memcpy(net->peer_rules, rules, (size_t)rules_size);
    net->peer_rules_size = rules_size;
    net->session = session;
    net->delay = SDL_clamp(delay, 1, PH_NET_MAX_DELAY);
    net->local_player = 1;
    ph_net_prepare(net);
    net->status = PH_NET_CONNECTED;
    ph_net_say(net, "Joined %s. Input delay %d frames.", net->peer_name, net->delay);
}

static void ph_net_game_packet(struct ph_net *net, struct ph_packet *packet, uint32_t ip, uint16_t port)
{
    if (packet->size < PH_NET_HEADER || SDL_memcmp(packet->data, net->magic, 4)) {
        return;
    }
    packet->at = 4;
    uint32_t version = ph_get8(packet);
    int type = (int)ph_get8(packet);
    uint32_t session = ph_get32(packet);
    int hosting = net->status == PH_NET_HOSTING || (net->status == PH_NET_CONNECTED && net->local_player == 0);
    int from_peer = net->has_peer && ip == net->peer_ip && port == net->peer_port;
    if (version != PH_NET_PROTOCOL) {
        if (hosting && type == PH_PACKET_HELLO) {
            ph_net_send_refuse(net, ip, port, "The two games use different network versions.");
        } else if (net->status == PH_NET_JOINING && from_peer) {
            ph_net_fail(net, "Cannot join: the two games use different network versions.");
        }
        return;
    }
    if (hosting && type == PH_PACKET_HELLO) {
        ph_net_host_hello(net, packet, ip, port);
    } else if (net->status == PH_NET_JOINING && from_peer && type == PH_PACKET_PROBE) {
        if (ph_get32(packet) == net->nonce) {
            net->probe_echo = ph_get32(packet);
            ph_net_send_hello(net);
        }
    } else if (net->status == PH_NET_JOINING && from_peer && type == PH_PACKET_WELCOME) {
        ph_net_join_welcome(net, packet, session);
    } else if (net->status == PH_NET_JOINING && from_peer && type == PH_PACKET_REFUSE) {
        char reason[PH_NET_REASON];
        ph_get_text(packet, reason, sizeof reason);
        ph_net_fail(net, "Cannot join: %s", reason);
    } else if (net->status == PH_NET_CONNECTED && from_peer && session == net->session && (type == PH_PACKET_INPUT || type == PH_PACKET_BYE)) {
        ph_net_read_inputs(net, packet, type == PH_PACKET_BYE);
    }
}

static void ph_net_listen_packet(struct ph_net *net, struct ph_packet *packet, uint32_t ip, uint16_t port)
{
    if (packet->size >= PH_NET_HEADER && !SDL_memcmp(packet->data, net->magic, 4) && packet->data[4] == PH_NET_PROTOCOL &&
        packet->data[5] == PH_PACKET_LOOK && net->status == PH_NET_HOSTING) {
        ph_net_send_here(net, ip, port);
    }
}

static void ph_net_search_packet(struct ph_net *net, struct ph_packet *packet, uint32_t ip)
{
    if (packet->size < PH_NET_HEADER || SDL_memcmp(packet->data, net->magic, 4) || packet->data[4] != PH_NET_PROTOCOL || packet->data[5] != PH_PACKET_HERE) {
        return;
    }
    packet->at = PH_NET_HEADER;
    uint16_t port = (uint16_t)ph_get16(packet);
    uint32_t id = ph_get32(packet);
    uint32_t identity = ph_get32(packet);
    char name[PH_NET_NAME];
    ph_get_text(packet, name, sizeof name);
    if (packet->bad || (id == net->id && net->status == PH_NET_HOSTING)) {
        return;
    }
    int index = 0;
    while (index < net->game_count && net->games[index].id != id) {
        index++;
    }
    if (index == PH_NET_GAMES) {
        return;
    }
    struct ph_net_game *game = &net->games[index];
    int loopback = ip >> 24 == 127;
    if (index == net->game_count) {
        net->game_count++;
        SDL_zerop(game);
    }
    if (!game->ip || !loopback || game->ip >> 24 == 127) {
        game->ip = ip;
        ph_net_format(ip, game->address, sizeof game->address);
    }
    game->id = id;
    game->port = port;
    game->other_version = identity != net->identity;
    game->seen = ph_net_now();
    SDL_strlcpy(game->name, name[0] ? name : game->address, sizeof game->name);
}

static void ph_net_drain(struct ph_net *net, intptr_t *handle, int kind)
{
    struct ph_packet packet;
    uint32_t ip = 0;
    uint16_t port = 0;
    for (int count = 0; count < PH_NET_DRAIN && *handle != PH_NET_NO_SOCKET && ph_net_receive(*handle, &packet, &ip, &port); count++) {
        if (kind == 0) {
            ph_net_game_packet(net, &packet, ip, port);
        } else if (kind == 1) {
            ph_net_listen_packet(net, &packet, ip, port);
        } else {
            ph_net_search_packet(net, &packet, ip);
        }
    }
}

static void ph_net_send_looks(struct ph_net *net)
{
    struct ph_packet packet = ph_net_start_packet(net, PH_PACKET_LOOK, 0);
    uint32_t addresses[PH_NET_INTERFACES];
    uint32_t broadcasts[PH_NET_INTERFACES];
    uint32_t group = (uint32_t)ph_net_group[0] << 24 | (uint32_t)ph_net_group[1] << 16 | (uint32_t)ph_net_group[2] << 8 | ph_net_group[3];
    ph_net_send_to(net, net->search_socket, &packet, 0xFFFFFFFFu, net->port);
    ph_net_send_to(net, net->search_socket, &packet, 0x7F000001u, net->port);
    ph_net_send_to(net, net->search_socket, &packet, group, net->port);
    int count = ph_net_interfaces(addresses, broadcasts, PH_NET_INTERFACES);
    for (int index = 0; index < count; index++) {
        if (broadcasts[index] && broadcasts[index] != 0xFFFFFFFFu) {
            ph_net_send_to(net, net->search_socket, &packet, broadcasts[index], net->port);
        }
    }
    net->last_look = ph_net_now();
}

static void ph_net_forget(struct ph_net *net)
{
    int kept = 0;
    for (int index = 0; index < net->game_count; index++) {
        if (ph_net_now() - net->games[index].seen < PH_NET_FORGET_MS) {
            net->games[kept++] = net->games[index];
        }
    }
    net->game_count = kept;
}

static void ph_net_default_name(struct ph_net *net)
{
    char host[128] = "";
    if (net->name[0]) {
        return;
    }
    if (gethostname(host, sizeof host - 1) != 0 || !host[0]) {
        SDL_strlcpy(host, "Player", sizeof host);
    }
    char *dot = SDL_strchr(host, '.');
    if (dot && dot != host) {
        *dot = 0;
    }
    SDL_strlcpy(net->name, host, sizeof net->name);
}

void ph_net_init(struct ph_net *net, const char *magic, uint16_t port, uint32_t identity, float frame_rate)
{
    SDL_zerop(net);
    const char *source = magic && magic[0] ? magic : "PHNT";
    size_t length = SDL_strlen(source);
    for (int index = 0; index < 4; index++) {
        net->magic[index] = (size_t)index < length ? source[index] : ' ';
    }
    net->port = port;
    net->identity = identity;
    net->frame_rate = frame_rate;
    net->check_interval = 60;
    net->desync_frame = -1;
    net->game_socket = PH_NET_NO_SOCKET;
    net->listen_socket = PH_NET_NO_SOCKET;
    net->search_socket = PH_NET_NO_SOCKET;
}

static void ph_net_reset(struct ph_net *net)
{
    ph_net_close_session(net);
    net->status = PH_NET_OFF;
    net->has_peer = 0;
    net->peer_name[0] = 0;
    net->peer_rules_size = 0;
    net->peer_requested_delay = 0;
    net->probe_echo = 0;
    net->rtt_count = 0;
    net->session = 0;
    net->game_port = 0;
    net->message[0] = 0;
    net->sent = 0;
    net->received = 0;
    net->desync_frame = -1;
}

int ph_net_host(struct ph_net *net)
{
    ph_net_stop(net);
    ph_net_reset(net);
    if (!ph_net_start()) {
        ph_net_fail(net, "Could not start networking.");
        return 0;
    }
    ph_net_default_name(net);
    for (int offset = 1; offset <= PH_NET_PORTS && net->game_socket == PH_NET_NO_SOCKET; offset++) {
        net->game_socket = ph_net_open((uint16_t)(net->port + offset), 0);
        net->game_port = (uint16_t)(net->port + offset);
    }
    if (net->game_socket == PH_NET_NO_SOCKET) {
        ph_net_fail(net, "Could not open network port %u for hosting.", net->port + 1);
        return 0;
    }
    net->listen_socket = ph_net_open(net->port, 1);
    if (net->listen_socket != PH_NET_NO_SOCKET) {
        struct ip_mreq membership;
        memset(&membership, 0, sizeof membership);
        memcpy(&membership.imr_multiaddr.s_addr, ph_net_group, 4);
        membership.imr_interface.s_addr = htonl(INADDR_ANY);
        setsockopt((ph_socket)net->listen_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&membership, sizeof membership);
    }
    net->id = SDL_rand_bits() | 1;
    net->started = ph_net_now();
    net->status = PH_NET_HOSTING;
    ph_net_find_address(net);
    ph_net_say(net, "Waiting for another player to join.");
    return 1;
}

static int ph_net_resolve(const char *text, uint16_t fallback_port, uint32_t *ip, uint16_t *port)
{
    char host[256];
    SDL_strlcpy(host, text ? text : "", sizeof host);
    *port = fallback_port;
    char *colon = SDL_strchr(host, ':');
    if (colon) {
        *colon = 0;
        int number = SDL_atoi(colon + 1);
        *port = number > 0 && number < 65536 ? (uint16_t)number : fallback_port;
    }
    struct in_addr numeric;
    if (inet_pton(AF_INET, host[0] ? host : "127.0.0.1", &numeric) == 1) {
        *ip = ntohl(numeric.s_addr);
        return 1;
    }
    struct addrinfo hints;
    struct addrinfo *found = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(host, NULL, &hints, &found) != 0 || !found) {
        return 0;
    }
    *ip = ntohl(((struct sockaddr_in *)found->ai_addr)->sin_addr.s_addr);
    freeaddrinfo(found);
    return 1;
}

static int ph_net_join_at(struct ph_net *net, uint32_t ip, uint16_t port)
{
    char address[PH_NET_ADDRESS];
    ph_net_close_session(net);
    ph_net_reset(net);
    ph_net_default_name(net);
    net->game_socket = ph_net_open(0, 0);
    if (net->game_socket == PH_NET_NO_SOCKET) {
        ph_net_fail(net, "Could not open a network port.");
        return 0;
    }
    net->game_port = ph_net_bound_port(net->game_socket);
    net->peer_ip = ip;
    net->peer_port = port;
    net->has_peer = 1;
    net->nonce = SDL_rand_bits() | 1;
    net->started = ph_net_now();
    net->status = PH_NET_JOINING;
    ph_net_format(ip, address, sizeof address);
    ph_net_say(net, "Connecting to %s:%u...", address, port);
    ph_net_send_hello(net);
    return 1;
}

int ph_net_join(struct ph_net *net, const char *address)
{
    uint32_t ip = 0;
    uint16_t port = 0;
    if (!ph_net_start()) {
        ph_net_fail(net, "Could not start networking.");
        return 0;
    }
    if (!ph_net_resolve(address, (uint16_t)(net->port + 1), &ip, &port)) {
        ph_net_reset(net);
        ph_net_fail(net, "Could not find the address %s.", address ? address : "");
        return 0;
    }
    return ph_net_join_at(net, ip, port);
}

int ph_net_join_game(struct ph_net *net, int index)
{
    if (index < 0 || index >= net->game_count) {
        return 0;
    }
    if (net->games[index].other_version) {
        ph_net_say(net, "%s is running a different version or ROM.", net->games[index].name);
        return 0;
    }
    return ph_net_join_at(net, net->games[index].ip, net->games[index].port);
}

void ph_net_search(struct ph_net *net)
{
    if (!ph_net_start()) {
        return;
    }
    if (net->search_socket == PH_NET_NO_SOCKET) {
        net->search_socket = ph_net_open(0, 0);
        net->game_count = 0;
        net->last_look = 0;
    }
    net->searching = net->search_socket != PH_NET_NO_SOCKET;
    ph_net_default_name(net);
    if (!net->own_address[0] || net->status == PH_NET_OFF) {
        ph_net_find_address(net);
    }
}

void ph_net_stop_search(struct ph_net *net)
{
    ph_net_close(&net->search_socket);
    net->searching = 0;
    net->game_count = 0;
}

void ph_net_update(struct ph_net *net)
{
    ph_net_release(net);
    if (net->searching && net->search_socket != PH_NET_NO_SOCKET) {
        if (!net->last_look || ph_net_now() - net->last_look >= PH_NET_LOOK_MS) {
            ph_net_send_looks(net);
        }
        ph_net_drain(net, &net->search_socket, 2);
        ph_net_forget(net);
    }
    ph_net_drain(net, &net->listen_socket, 1);
    ph_net_drain(net, &net->game_socket, 0);
    if (net->status == PH_NET_JOINING) {
        if (ph_net_now() - net->started > PH_NET_JOIN_MS) {
            ph_net_fail(net, "No answer from the host. Check the address and that both devices are on the same network.");
        } else if (ph_net_now() - net->last_hello >= PH_NET_HELLO_MS) {
            ph_net_send_hello(net);
        }
    }
    if (net->status == PH_NET_CONNECTED && !net->in_game && net->peer_left) {
        ph_net_fail(net, "%s left.", ph_net_peer(net));
    }
    if (net->status == PH_NET_CONNECTED && net->in_game && net->remote_ack < net->local_through && ph_net_now() - net->last_send >= PH_NET_RESEND_MS) {
        net->resends++;
        ph_net_send_inputs(net, 0);
    }
}

void ph_net_begin(struct ph_net *net)
{
    ph_net_stop_search(net);
    net->playing = 1;
}

int ph_net_playing(const struct ph_net *net)
{
    return net->playing;
}

int ph_net_stalled(const struct ph_net *net)
{
    return net->playing && net->stall_started && ph_net_now_us() - net->stall_started >= PH_NET_STALL_NOTICE_MS * 1000ull;
}

const uint8_t *ph_net_agreed_rules(const struct ph_net *net)
{
    return net->local_player != 0 && net->peer_rules_size >= net->rules_size ? net->peer_rules : net->rules;
}

int ph_net_frame(struct ph_net *net, uint32_t local, uint32_t inputs[2])
{
    if (net->status != PH_NET_CONNECTED) {
        return PH_NET_LOST;
    }
    net->in_game = 1;
    ph_net_release(net);
    int64_t target = net->frame + net->delay;
    if (net->local_through < target) {
        net->local_inputs[target % PH_NET_HISTORY] = local;
        net->local_through = target;
        ph_net_send_inputs(net, 0);
    }
    ph_net_drain(net, &net->game_socket, 0);
    if (net->status != PH_NET_CONNECTED) {
        return PH_NET_LOST;
    }
    if (net->remote_through >= net->frame) {
        if (net->stall_started) {
            net->last_stall_us = ph_net_now_us() - net->stall_started;
            net->stall_us += net->last_stall_us;
            net->stall_started = 0;
        }
        int slot = (int)(net->frame % PH_NET_HISTORY);
        inputs[net->local_player] = net->local_inputs[slot];
        inputs[1 - net->local_player] = net->remote_inputs[slot];
        net->frame++;
        return PH_NET_READY;
    }
    if (net->peer_left) {
        ph_net_fail(net, "%s left the game.", ph_net_peer(net));
        return PH_NET_LOST;
    }
    if (ph_net_now() - net->last_receive > PH_NET_SILENT_MS) {
        ph_net_fail(net, "Lost contact with %s.", ph_net_peer(net));
        return PH_NET_LOST;
    }
    if (!net->stall_started) {
        net->stall_started = ph_net_now_us();
        net->stalls++;
    }
    if (ph_net_now() - net->last_send >= PH_NET_RESEND_MS) {
        net->resends++;
        ph_net_send_inputs(net, 0);
    }
    return PH_NET_WAIT;
}

int ph_net_check_due(const struct ph_net *net)
{
    if (net->status != PH_NET_CONNECTED || net->frame % net->check_interval) {
        return 0;
    }
    return net->local_checks[net->frame / net->check_interval % PH_NET_CHECKS].frame != net->frame;
}

void ph_net_check(struct ph_net *net, uint32_t hash)
{
    if (net->status != PH_NET_CONNECTED || net->frame % net->check_interval) {
        return;
    }
    net->local_checks[net->frame / net->check_interval % PH_NET_CHECKS] = (struct ph_net_check){ net->frame, hash };
    ph_net_compare(net);
}

void ph_net_wait(struct ph_net *net, int milliseconds)
{
    if (net->game_socket == PH_NET_NO_SOCKET) {
        SDL_Delay((Uint32)milliseconds);
        return;
    }
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET((ph_socket)net->game_socket, &readable);
    struct timeval timeout = { milliseconds / 1000, milliseconds % 1000 * 1000 };
    select((int)net->game_socket + 1, &readable, NULL, NULL, &timeout);
}

void ph_net_stop(struct ph_net *net)
{
    ph_net_send_bye(net);
    ph_net_close_session(net);
    ph_net_stop_search(net);
    if (net->status != PH_NET_FAILED) {
        net->message[0] = 0;
    }
    net->status = PH_NET_OFF;
    net->playing = 0;
}

int ph_net_settings(struct ph_net *net, struct ph_setting *settings)
{
    settings[0] = (struct ph_setting){ .key = "lan_name", .label = "Your name", .text = net->name, .text_size = PH_NET_NAME };
    settings[1] = (struct ph_setting){ .key = "lan_delay", .label = "Input delay", .value = &net->requested_delay, .maximum = PH_NET_MAX_DELAY, .step = 1, .unit = " frames", .zero = "Auto" };
    return PH_NET_SETTINGS;
}

int ph_net_take(struct ph_net *net, const char *key, const char *value)
{
    struct ph_setting settings[PH_NET_SETTINGS];
    return ph_settings_take(settings, ph_net_settings(net, settings), key, value);
}

void ph_net_write(SDL_IOStream *file, struct ph_net *net)
{
    struct ph_setting settings[PH_NET_SETTINGS];
    ph_settings_write(file, settings, ph_net_settings(net, settings));
}

#endif
