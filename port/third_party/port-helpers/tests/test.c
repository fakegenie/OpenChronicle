#define PH_IMPLEMENTATION
#include "../ph.h"
#include "fixture.h"

#include <stdio.h>
#include <string.h>

enum { TOUCH_PAD, TOUCH_A, TOUCH_B, TOUCH_START, TOUCH_MENU, TOUCH_COUNT };
enum { ACTION_UP, ACTION_DOWN, ACTION_LEFT, ACTION_RIGHT, ACTION_SHOOT, ACTION_JUMP, ACTION_START, ACTION_COUNT };

static int failures;
static int checks;

static int scale_mode;
static int fullscreen;
static int volume;
static int region;
static char nickname[32];

static const char *const scale_names[] = { "Integer", "Fit", "Stretch" };
static const char *const region_names[] = { "Japan", "USA", "Europe" };
static const char *const section_names[] = { "Video", "Audio", "Game" };

static const struct ph_setting settings[] = {
    { .key = "scale", .label = "Scaling", .section = 0, .value = &scale_mode, .fallback = 1, .choices = scale_names, .choice_count = 3 },
    { .key = "fullscreen", .label = "Fullscreen", .section = 0, .show = PH_SHOW_DESKTOP, .value = &fullscreen, .choices = ph_off_on, .choice_count = 2 },
    { .key = "volume", .label = "Volume", .section = 1, .value = &volume, .fallback = 80, .maximum = 100, .step = 10, .unit = "%", .zero = "Muted" },
    { .key = "region", .label = "Region", .section = 2, .value = &region, .choices = region_names, .choice_count = 3 },
    { .key = "nickname", .label = "Nickname", .section = 2, .text = nickname, .text_size = sizeof nickname, .text_fallback = "Player" },
};

static const char *const action_keys[] = { "up", "down", "left", "right", "shoot", "jump", "start" };
static const char *const action_labels[] = { "Up", "Down", "Left", "Right", "Shoot", "Jump", "Start" };
static const int default_keys[] = { SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_RETURN };
static const int default_buttons[] = { SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_START };

static const struct ph_touch_control landscape[] = {
    { "pad", "", PH_TOUCH_PAD, 0.13f, 0.70f, 150, 0, 0, 0 },
    { "a", "A", PH_TOUCH_BUTTON, 0.80f, 0.76f, 66, 0, 0, 1u << ACTION_SHOOT },
    { "b", "B", PH_TOUCH_BUTTON, 0.92f, 0.58f, 66, 0, 0, 1u << ACTION_JUMP },
    { "start", "Start", PH_TOUCH_PILL, 0.56f, 0.95f, 76, 30, 0, 1u << ACTION_START },
    { "menu", "", PH_TOUCH_PAUSE, 0.44f, 0.95f, 44, 44, 0, 0 },
};

static const struct ph_touch_control portrait[] = {
    { "pad", "", PH_TOUCH_PAD, 0.25f, 0.80f, 150, 0, 0, 0 },
    { "a", "A", PH_TOUCH_BUTTON, 0.70f, 0.84f, 66, 0, 0, 1u << ACTION_SHOOT },
    { "b", "B", PH_TOUCH_BUTTON, 0.86f, 0.76f, 66, 0, 0, 1u << ACTION_JUMP },
    { "start", "Start", PH_TOUCH_PILL, 0.62f, 0.96f, 76, 30, 0, 1u << ACTION_START },
    { "menu", "", PH_TOUCH_PAUSE, 0.38f, 0.96f, 44, 44, 0, 0 },
};

static const struct ph_touch_control stick_layout[] = {
    { "stick", "", PH_TOUCH_STICK, 0.14f, 0.55f, 120, 0, 0, 0 },
    { "a", "A", PH_TOUCH_BUTTON, 0.86f, 0.62f, 68, 0, 0, 0 },
    { "b", "B", PH_TOUCH_BUTTON, 0.938f, 0.47f, 68, 0, 0, 0 },
    { "c", "C", PH_TOUCH_BUTTON, 0.782f, 0.47f, 68, 0, 0, 0 },
    { "d", "D", PH_TOUCH_BUTTON, 0.86f, 0.32f, 68, 0, 0, 0 },
    { "start", "Start", PH_TOUCH_PILL, 0.91f, 0.91f, 100, 34, 0, 0 },
    { "menu", "", PH_TOUCH_PAUSE, 0.05f, 0.09f, 44, 44, 0, 0 },
};

static void check(int ok, const char *what)
{
    checks++;
    if (!ok) {
        failures++;
        printf("FAIL %s\n", what);
    }
}

static void test_hash(void)
{
    char text[PH_SHA1_TEXT_SIZE];
    check(ph_crc32(0, "123456789", 9) == 0xCBF43926u, "crc32 check value");
    check(ph_crc32(ph_crc32(0, "1234", 4), "56789", 5) == 0xCBF43926u, "crc32 chained");
    check(ph_fnv32(PH_FNV32_START, "foobar", 6) == 0xBF9CF968u, "fnv32 foobar");
    check(ph_fnv64(PH_FNV64_START, "foobar", 6) == 0x85944171F73967E8ull, "fnv64 foobar");
    ph_sha1_text("abc", 3, text);
    check(!strcmp(text, "a9993e364706816aba3e25717850c26c9cd0d89d"), "sha1 abc");
    ph_sha1_text("", 0, text);
    check(!strcmp(text, "da39a3ee5e6b4b0d3255bfef95601890afd80709"), "sha1 empty");
    const char *long_text = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    ph_sha1_text(long_text, strlen(long_text), text);
    check(!strcmp(text, "84983e441c3bd26ebaae4aa1f95129e5e54670f1"), "sha1 two blocks");
}

static void test_zip(void)
{
    struct ph_zip_entry entries[8];
    check(ph_zip_is_zip(fixture_zip, sizeof fixture_zip), "zip detected");
    check(!ph_zip_is_zip(fixture_dynamic, sizeof fixture_dynamic), "deflate stream is not a zip");
    int count = ph_zip_entries(fixture_zip, sizeof fixture_zip, entries, 8);
    check(count == 3, "zip entry count");
    int hello = -1;
    for (int index = 0; index < count; index++) {
        if (ph_zip_named(&entries[index], "hello.txt")) {
            hello = index;
        }
    }
    check(hello >= 0, "zip name match ignores folder and case");
    if (hello >= 0) {
        uint8_t out[1024];
        const char *line = "The quick brown fox jumps over the lazy dog. ";
        int extracted = entries[hello].size <= sizeof out && ph_zip_extract(&entries[hello], out);
        check(extracted && entries[hello].method == PH_ZIP_DEFLATED, "zip deflated entry extracts");
        check(entries[hello].crc == fixture_hello_crc, "zip entry crc read");
        check(extracted && !memcmp(out + 45 * 19, line, 45), "zip deflated contents");
    }
    uint8_t interleaved[64] = { 0 };
    struct ph_zip_part even = { "even.bin", 0, 32, 0, 2 };
    struct ph_zip_part odd = { NULL, fixture_odd_crc, 32, 1, 2 };
    check(ph_zip_place(entries, count, &even, interleaved, sizeof interleaved), "zip place by name");
    check(ph_zip_place(entries, count, &odd, interleaved, sizeof interleaved), "zip place by crc");
    int ordered = 1;
    for (int index = 0; index < 32; index++) {
        ordered &= interleaved[index * 2] == index && interleaved[index * 2 + 1] == 0x80 + index;
    }
    check(ordered, "zip interleaved placement");
    struct ph_zip_part missing = { "nothing.bin", 0, 32, 0, 1 };
    check(!ph_zip_place(entries, count, &missing, interleaved, sizeof interleaved), "zip missing part");
    static uint8_t expected[4000];
    static uint8_t inflated[4000];
    for (int index = 0; index < 4000; index++) {
        expected[index] = (uint8_t)((index * index + index / 7) % 251);
    }
    check(ph_inflate(fixture_dynamic, sizeof fixture_dynamic, inflated, sizeof inflated) && !memcmp(inflated, expected, sizeof expected), "inflate dynamic");
    check(ph_inflate(fixture_stored, sizeof fixture_stored, inflated, 300) && !memcmp(inflated, expected, 300), "inflate stored");
    check(!ph_inflate(fixture_dynamic, sizeof fixture_dynamic / 2, inflated, sizeof inflated), "inflate truncated fails");
}

static void setup_input(struct ph_input *input)
{
    ph_input_init(input, 2, ACTION_COUNT, action_keys, action_labels);
    ph_input_defaults(input, 0, default_keys, default_buttons);
    ph_input_defaults(input, 1, NULL, default_buttons);
}

static char *read_all(SDL_IOStream *file)
{
    Sint64 size = SDL_GetIOSize(file);
    char *text = (char *)SDL_calloc(1, (size_t)size + 1);
    SDL_SeekIO(file, 0, SDL_IO_SEEK_SET);
    SDL_ReadIO(file, text, (size_t)size);
    return text;
}

static void test_settings(void)
{
    int count = SDL_arraysize(settings);
    struct ph_input input;
    struct ph_touch touch;
    struct ph_net net;
    char text[64];
    ph_settings_reset(settings, count);
    check(scale_mode == 1 && volume == 80 && !strcmp(nickname, "Player"), "settings defaults");
    ph_setting_step(&settings[0], 1, 1);
    ph_setting_step(&settings[0], 1, 1);
    check(scale_mode == 0, "choice wraps");
    ph_setting_step(&settings[2], 5, 0);
    check(volume == 100, "number clamps");
    volume = 0;
    ph_setting_text(&settings[2], text, sizeof text);
    check(!strcmp(text, "Muted"), "zero text");
    volume = 40;
    ph_setting_text(&settings[2], text, sizeof text);
    check(!strcmp(text, "40%"), "unit text");
    region = 2;
    SDL_strlcpy(nickname, "Zed", sizeof nickname);
    setup_input(&input);
    ph_input_bind(&input, 0, ACTION_SHOOT, PH_SLOT_PAD, (struct ph_binding){ PH_BIND_AXIS, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 1 });
    ph_input_bind(&input, 1, ACTION_JUMP, PH_SLOT_KEY, (struct ph_binding){ PH_BIND_KEY, SDL_SCANCODE_X, 0 });
    check(input.bindings[0][ACTION_JUMP][PH_SLOT_KEY].kind == PH_BIND_NONE, "binding a key removes it from the other player");
    ph_touch_init(&touch, landscape, portrait, TOUCH_COUNT);
    touch.size = 120;
    touch.mode = PH_TOUCH_ALWAYS;
    touch.places[PH_TOUCH_LANDSCAPE][TOUCH_A] = (SDL_FPoint){ 0.5f, 0.25f };
    touch.placed[PH_TOUCH_LANDSCAPE] |= 1u << TOUCH_A;
    ph_net_init(&net, "TEST", 47000, 1, 60);
    SDL_strlcpy(net.name, "Lounge", sizeof net.name);
    net.requested_delay = 4;

    SDL_IOStream *file = SDL_IOFromDynamicMem();
    SDL_IOprintf(file, "# saved by the test\n");
    ph_settings_write(file, settings, count);
    ph_input_write(file, &input);
    ph_touch_write(file, &touch);
    ph_net_write(file, &net);
    char *saved = read_all(file);
    SDL_CloseIO(file);

    struct ph_input loaded_input;
    struct ph_touch loaded_touch;
    struct ph_net loaded_net;
    ph_settings_reset(settings, count);
    setup_input(&loaded_input);
    ph_touch_init(&loaded_touch, landscape, portrait, TOUCH_COUNT);
    ph_net_init(&loaded_net, "TEST", 47000, 1, 60);
    char *cursor = saved;
    char *key = NULL;
    char *value = NULL;
    int unknown = 0;
    while (ph_next_pair(&cursor, &key, &value)) {
        int taken = ph_settings_take(settings, count, key, value) || ph_input_take(&loaded_input, key, value) || ph_touch_take(&loaded_touch, key, value) ||
            ph_net_take(&loaded_net, key, value);
        unknown += !taken;
    }
    SDL_free(saved);
    check(!unknown, "every saved key is taken back");
    check(scale_mode == 0 && volume == 40 && region == 2 && !strcmp(nickname, "Zed"), "settings round trip");
    check(!memcmp(loaded_input.bindings, input.bindings, sizeof input.bindings), "input bindings round trip");
    check(loaded_touch.size == 120 && loaded_touch.mode == PH_TOUCH_ALWAYS, "touch options round trip");
    check(loaded_touch.placed[PH_TOUCH_LANDSCAPE] == 1u << TOUCH_A && SDL_fabsf(loaded_touch.places[PH_TOUCH_LANDSCAPE][TOUCH_A].y - 0.25f) < 0.001f, "touch layout round trip");
    check(!strcmp(loaded_net.name, "Lounge") && loaded_net.requested_delay == 4, "net options round trip");
    ph_input_binding_text(&input, 0, ACTION_UP, PH_SLOT_KEY, text, sizeof text);
    check(!strcmp(text, "Up"), "key binding text");
    ph_input_binding_text(&input, 0, ACTION_SHOOT, PH_SLOT_PAD, text, sizeof text);
    check(text[0] != 0, "axis binding text");
}

static void test_config(void)
{
    struct ph_input input;
    struct ph_net net;
    setup_input(&input);
    ph_net_init(&net, "TEST", 47000, 1, 60);
    struct ph_config config = { settings, SDL_arraysize(settings), &input, NULL, &net };
    ph_files_init("port-helpers", "test");
    ph_settings_reset(settings, SDL_arraysize(settings));
    volume = 30;
    ph_menu_flavor = PH_FLAVOR_MINT;
    SDL_strlcpy(net.name, "Den", sizeof net.name);
    ph_input_bind(&input, 0, ACTION_JUMP, PH_SLOT_KEY, (struct ph_binding){ PH_BIND_KEY, SDL_SCANCODE_C, 0 });
    check(ph_config_save(&config, "test.cfg"), "config saves");
    volume = 0;
    ph_menu_flavor = PH_FLAVOR_PLAIN;
    setup_input(&input);
    ph_net_init(&net, "TEST", 47000, 1, 60);
    int loaded = ph_config_load(&config, "test.cfg");
    check(loaded && volume == 30 && ph_menu_flavor == PH_FLAVOR_MINT && !SDL_strcmp(net.name, "Den") && input.bindings[0][ACTION_JUMP][PH_SLOT_KEY].code == SDL_SCANCODE_C,
          "config loads what it saved including window flavor");
    char path[PH_PATH_SIZE];
    ph_user_path("test.cfg", path, sizeof path);
    SDL_RemovePath(path);
    check(!ph_config_load(&config, "test.cfg") && volume == 80 && ph_menu_flavor == PH_FLAVOR_PLAIN, "a missing config loads the defaults");
}

static void test_file_name(void)
{
    check(!SDL_strcmp(ph_file_name("roms/snes/tbg.sfc"), "tbg.sfc") && !SDL_strcmp(ph_file_name("C:\\roms\\game.zip"), "game.zip") && !SDL_strcmp(ph_file_name("game.zip"), "game.zip"),
          "file names drop the folder");
    check(!SDL_strcmp(ph_file_name("content://com.android.externalstorage.documents/document/primary%3AROMs%2Fmiexchng.zip"), "miexchng.zip") &&
              !SDL_strcmp(ph_file_name("content://com.android.externalstorage.documents/document/primary%3Atbg.sfc"), "tbg.sfc"),
          "Android content URIs decode their leaf file name");
    char src[PH_PATH_SIZE], dst[PH_PATH_SIZE];
    if (ph_user_path("copy_src.bin", src, sizeof src) && ph_user_path("copy_dst.bin", dst, sizeof dst)) {
        const char payload[] = "port-helpers streaming copy check";
        check(SDL_SaveFile(src, payload, sizeof payload) && ph_copy_file(src, dst), "ph_copy_file copies files");
        size_t got_size = 0;
        char *got = SDL_LoadFile(dst, &got_size);
        check(got && got_size == sizeof payload && !SDL_memcmp(got, payload, sizeof payload), "ph_copy_file preserves exact bytes");
        SDL_free(got);
        SDL_RemovePath(src);
        SDL_RemovePath(dst);
    }
}

static uint32_t host_input(int64_t frame)
{
    return (uint32_t)(frame * 3 + 1);
}

static uint32_t guest_input(int64_t frame)
{
    return (uint32_t)(frame * 7 + 2) | 0x80000000u;
}

static void pump(struct ph_net *host, struct ph_net *guest)
{
    ph_net_update(host);
    ph_net_update(guest);
    SDL_Delay(2);
}

static int play_frame(struct ph_net *host, struct ph_net *guest, uint32_t host_hash, uint32_t guest_hash, uint32_t host_inputs[2], uint32_t guest_inputs[2])
{
    int host_done = 0;
    int guest_done = 0;
    uint64_t until = SDL_GetTicks() + 3000;
    while ((!host_done || !guest_done) && SDL_GetTicks() < until) {
        int64_t frame = host->frame;
        if (!host_done) {
            int result = ph_net_frame(host, host_input(frame), host_inputs);
            host_done = result == PH_NET_READY;
            if (result == PH_NET_LOST) {
                return 0;
            }
        }
        if (!guest_done) {
            int result = ph_net_frame(guest, guest_input(guest->frame), guest_inputs);
            guest_done = result == PH_NET_READY;
            if (result == PH_NET_LOST) {
                return 0;
            }
        }
        if (!host_done || !guest_done) {
            ph_net_update(host);
            ph_net_update(guest);
            ph_net_wait(host, 1);
        }
    }
    if (ph_net_check_due(host)) {
        ph_net_check(host, host_hash);
    }
    if (ph_net_check_due(guest)) {
        ph_net_check(guest, guest_hash);
    }
    return host_done && guest_done;
}

static int connect_pair(struct ph_net *host, struct ph_net *guest, uint16_t port)
{
    ph_net_init(host, "TEST", port, 0xC0FFEE, 60);
    ph_net_init(guest, "TEST", port, 0xC0FFEE, 60);
    char address[32];
    ph_net_host(host);
    SDL_snprintf(address, sizeof address, "127.0.0.1:%u", host->game_port);
    ph_net_join(guest, address);
    uint64_t until = SDL_GetTicks() + 5000;
    while ((host->status != PH_NET_CONNECTED || guest->status != PH_NET_CONNECTED) && SDL_GetTicks() < until) {
        pump(host, guest);
    }
    return host->status == PH_NET_CONNECTED && guest->status == PH_NET_CONNECTED;
}

static void test_net_lossy(void)
{
    struct ph_net host;
    struct ph_net guest;
    uint16_t port = (uint16_t)(46000 + SDL_rand(2000) * 8);
    check(connect_pair(&host, &guest, port), "lossy net connects");
    host.loss = guest.loss = 25;
    host.latency = guest.latency = 30;
    int agreed = 1;
    int played = 0;
    for (int frame = 0; frame < 200 && host.status == PH_NET_CONNECTED; frame++) {
        uint32_t host_inputs[2] = { 0 };
        uint32_t guest_inputs[2] = { 0 };
        if (!play_frame(&host, &guest, (uint32_t)frame, (uint32_t)frame, host_inputs, guest_inputs)) {
            break;
        }
        played++;
        agreed &= host_inputs[0] == guest_inputs[0] && host_inputs[1] == guest_inputs[1];
    }
    check(played == 200 && agreed, "lossy net plays 200 matching frames");
    check(host.dropped > 0 && guest.dropped > 0, "lossy net drops packets");
    check(host.checks_matched > 0 && guest.checks_matched > 0, "lossy net state checks match");
    ph_net_stop(&host);
    ph_net_stop(&guest);
}

static void test_net(void)
{
    struct ph_net host;
    struct ph_net guest;
    uint16_t port = (uint16_t)(42000 + SDL_rand(2000) * 8);
    ph_net_init(&host, "TEST", port, 0xC0FFEE, 60);
    ph_net_init(&guest, "TEST", port, 0xC0FFEE, 60);
    SDL_strlcpy(host.name, "Host", sizeof host.name);
    SDL_strlcpy(guest.name, "Guest", sizeof guest.name);
    host.rules[0] = 9;
    host.rules_size = 1;
    guest.rules[0] = 4;
    guest.rules_size = 1;
    check(ph_net_host(&host) && host.status == PH_NET_HOSTING, "net host starts");
    ph_net_search(&guest);
    uint64_t until = SDL_GetTicks() + 4000;
    while (!guest.game_count && SDL_GetTicks() < until) {
        pump(&host, &guest);
    }
    check(guest.game_count == 1 && !strcmp(guest.games[0].name, "Host") && guest.games[0].port == host.game_port, "net discovery finds the host");
    if (guest.game_count) {
        check(ph_net_join_game(&guest, 0), "net join starts");
    } else {
        char address[32];
        SDL_snprintf(address, sizeof address, "127.0.0.1:%u", host.game_port);
        check(ph_net_join(&guest, address), "net join by address starts");
    }
    until = SDL_GetTicks() + 5000;
    while ((host.status != PH_NET_CONNECTED || guest.status != PH_NET_CONNECTED) && SDL_GetTicks() < until) {
        pump(&host, &guest);
    }
    check(host.status == PH_NET_CONNECTED && guest.status == PH_NET_CONNECTED, "net connects");
    check(host.local_player == 0 && guest.local_player == 1, "net players assigned");
    check(host.delay == guest.delay && host.delay >= 1, "net delay agreed");
    check(!strcmp(host.peer_name, "Guest") && !strcmp(guest.peer_name, "Host"), "net names exchanged");
    check(guest.peer_rules_size == 1 && guest.peer_rules[0] == 9, "net rules sent");
    check(ph_net_agreed_rules(&host)[0] == 9 && ph_net_agreed_rules(&guest)[0] == 9, "net both sides play the host's rules");
    check(!ph_net_playing(&host), "net is not playing before the game begins");
    ph_net_begin(&host);
    ph_net_begin(&guest);
    check(ph_net_playing(&host) && ph_net_playing(&guest) && !guest.searching, "net begin marks the game playing and stops searching");
    int agreed = 1;
    int expected = 1;
    int played = 0;
    for (int64_t frame = 0; frame < 300 && host.status == PH_NET_CONNECTED; frame++) {
        uint32_t host_inputs[2] = { 0 };
        uint32_t guest_inputs[2] = { 0 };
        if (!play_frame(&host, &guest, (uint32_t)frame, (uint32_t)frame, host_inputs, guest_inputs)) {
            break;
        }
        played++;
        agreed &= host_inputs[0] == guest_inputs[0] && host_inputs[1] == guest_inputs[1];
        int64_t source = frame - host.delay;
        uint32_t want_host = source >= 0 ? host_input(source) : 0;
        uint32_t want_guest = source >= 0 ? guest_input(source) : 0;
        expected &= host_inputs[0] == want_host && host_inputs[1] == want_guest;
    }
    check(played == 300, "net plays 300 lockstep frames");
    check(agreed, "net both sides see the same inputs");
    check(expected, "net inputs arrive after the delay");
    check(host.checks_matched > 0 && guest.checks_matched > 0, "net state checks match");
    uint32_t alone[2];
    for (int tries = 0; tries < 20 && ph_net_frame(&host, 0, alone) == PH_NET_READY; tries++) {
    }
    check(!ph_net_stalled(&host), "net is not stalled the moment it waits");
    SDL_Delay(300);
    ph_net_frame(&host, 0, alone);
    check(ph_net_stalled(&host) && !ph_net_stalled(&guest), "net reports a stall when the other side stops sending");
    for (int frame = 0; frame < 180 && host.status == PH_NET_CONNECTED && guest.status == PH_NET_CONNECTED; frame++) {
        uint32_t inputs[2][2];
        play_frame(&host, &guest, 1, 2, inputs[0], inputs[1]);
    }
    for (int tries = 0; tries < 200 && (host.status == PH_NET_CONNECTED || guest.status == PH_NET_CONNECTED); tries++) {
        uint32_t inputs[2];
        if (host.status == PH_NET_CONNECTED) {
            ph_net_frame(&host, 0, inputs);
        }
        if (guest.status == PH_NET_CONNECTED) {
            ph_net_frame(&guest, 0, inputs);
        }
        SDL_Delay(2);
    }
    check(host.status == PH_NET_FAILED && guest.status == PH_NET_FAILED, "net desync ends both sessions");
    check(SDL_strstr(host.message, "out of sync") || SDL_strstr(guest.message, "out of sync"), "net desync message");
    check(SDL_max(host.desync_frame, guest.desync_frame) >= 300, "net desync frame recorded");
    ph_net_stop(&host);
    ph_net_stop(&guest);

    struct ph_net other;
    ph_net_init(&host, "TEST", port, 0xC0FFEE, 60);
    ph_net_init(&other, "TEST", port, 0xBADBAD, 60);
    ph_net_host(&host);
    char address[32];
    SDL_snprintf(address, sizeof address, "127.0.0.1:%u", host.game_port);
    ph_net_join(&other, address);
    until = SDL_GetTicks() + 3000;
    while (other.status == PH_NET_JOINING && SDL_GetTicks() < until) {
        pump(&host, &other);
    }
    check(other.status == PH_NET_FAILED && SDL_strstr(other.message, "different"), "net refuses a different game");
    check(host.status == PH_NET_HOSTING, "net host keeps waiting after a refusal");
    ph_net_stop(&host);
    ph_net_stop(&other);
}

static SDL_Event finger_event(SDL_EventType type, SDL_Renderer *renderer, SDL_FingerID id, SDL_FPoint point)
{
    SDL_FRect output = ph_view_output(renderer);
    SDL_Event event;
    SDL_zero(event);
    event.type = type;
    event.tfinger.touchID = 7;
    event.tfinger.fingerID = id;
    event.tfinger.x = point.x / output.w;
    event.tfinger.y = point.y / output.h;
    return event;
}

static SDL_Event key_event(SDL_Scancode scancode)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = scancode;
    event.key.key = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);
    event.key.down = true;
    return event;
}

static SDL_FPoint center_of(SDL_FRect rect)
{
    return (SDL_FPoint){ rect.x + rect.w / 2, rect.y + rect.h / 2 };
}

static int row_named(const struct ph_menu *menu, const char *label)
{
    for (int index = 0; index < menu->count; index++) {
        if (!SDL_strcmp(menu->rows[index].label, label)) {
            return index;
        }
    }
    return -1;
}

static void test_touch(SDL_Renderer *renderer)
{
    struct ph_touch touch;
    struct ph_input input;
    setup_input(&input);
    ph_touch_init(&touch, landscape, portrait, TOUCH_COUNT);
    ph_touch_layout(&touch, renderer);
    check(touch.orientation == PH_TOUCH_LANDSCAPE, "touch landscape layout");
    SDL_FPoint a = center_of(touch.rects[TOUCH_A]);
    SDL_Event event = finger_event(SDL_EVENT_FINGER_DOWN, renderer, 1, a);
    ph_touch_event(&touch, renderer, &event);
    check(touch.seen && (touch.held & 1u << TOUCH_A), "touch button held");
    SDL_FPoint pad = center_of(touch.rects[TOUCH_PAD]);
    SDL_FPoint right = { pad.x + touch.rects[TOUCH_PAD].w * 0.35f, pad.y };
    event = finger_event(SDL_EVENT_FINGER_DOWN, renderer, 2, right);
    ph_touch_event(&touch, renderer, &event);
    check(touch.directions == PH_TOUCH_RIGHT, "touch pad right");
    check(ph_touch_mask(&touch, &input) == (1u << ACTION_SHOOT | 1u << ACTION_RIGHT), "touch mask maps the pad and buttons to actions");
    SDL_FPoint up_left = { pad.x - touch.rects[TOUCH_PAD].w * 0.3f, pad.y - touch.rects[TOUCH_PAD].h * 0.3f };
    event = finger_event(SDL_EVENT_FINGER_MOTION, renderer, 2, up_left);
    ph_touch_event(&touch, renderer, &event);
    check(touch.directions == (PH_TOUCH_UP | PH_TOUCH_LEFT) || touch.directions == PH_TOUCH_UP || touch.directions == PH_TOUCH_LEFT, "touch pad follows the finger");
    touch.eight_way = 1;
    event = finger_event(SDL_EVENT_FINGER_MOTION, renderer, 2, (SDL_FPoint){ up_left.x - 1, up_left.y - 1 });
    ph_touch_event(&touch, renderer, &event);
    check(touch.directions == (PH_TOUCH_UP | PH_TOUCH_LEFT), "touch pad diagonal in eight way mode");
    event = finger_event(SDL_EVENT_FINGER_UP, renderer, 2, up_left);
    ph_touch_event(&touch, renderer, &event);
    check(touch.directions == 0, "touch pad released");
    event = finger_event(SDL_EVENT_FINGER_UP, renderer, 1, a);
    ph_touch_event(&touch, renderer, &event);
    check(touch.held == 0, "touch button released");
    SDL_FPoint menu = center_of(touch.rects[TOUCH_MENU]);
    event = finger_event(SDL_EVENT_FINGER_DOWN, renderer, 3, menu);
    ph_touch_event(&touch, renderer, &event);
    event = finger_event(SDL_EVENT_FINGER_UP, renderer, 3, menu);
    ph_touch_event(&touch, renderer, &event);
    check(ph_touch_tapped(&touch, TOUCH_MENU) && !ph_touch_tapped(&touch, TOUCH_MENU), "touch pause tap reported once");
    event = finger_event(SDL_EVENT_FINGER_DOWN, renderer, 4, menu);
    ph_touch_event(&touch, renderer, &event);
    event = finger_event(SDL_EVENT_FINGER_UP, renderer, 4, menu);
    ph_touch_event(&touch, renderer, &event);
    check(ph_touch_paused(&touch) && !ph_touch_paused(&touch), "touch pause control pauses once");
    event = key_event(SDL_SCANCODE_SPACE);
    ph_touch_event(&touch, renderer, &event);
    check(!ph_touch_visible(&touch), "touch hides after a key press");
    ph_touch_close(&touch);
}

static int diagonals_shown(struct ph_touch *touch)
{
    struct ph_setting options[PH_TOUCH_SETTINGS];
    int count = ph_touch_settings(touch, options);
    for (int index = 0; index < count; index++) {
        if (!SDL_strcmp(options[index].label, "Diagonals")) {
            return ph_setting_shown(&options[index]);
        }
    }
    return 0;
}

static void test_fullscreen_key(void)
{
    SDL_Event event = key_event(SDL_SCANCODE_F11);
    int f11 = ph_view_fullscreen_key(&event);
    event = key_event(SDL_SCANCODE_RETURN);
    int plain = ph_view_fullscreen_key(&event);
    event.key.mod = SDL_KMOD_LALT;
    int alt = ph_view_fullscreen_key(&event);
    event.key.repeat = true;
    int repeat = ph_view_fullscreen_key(&event);
    check(f11 && !plain && alt && !repeat, "F11 and Alt+Enter toggle fullscreen");
}

static void test_script(SDL_Renderer *renderer)
{
    const char *path = "tests/out/script.txt";
    const char *lines = "2 key Return\n3 bogus thing\n5 click 10 20\n6 tap 30\n8 shot tests/out/x.bmp\n9 down Left Shift\n12 up Left Shift\n";
    SDL_SaveFile(path, lines, SDL_strlen(lines));
    static struct ph_script script;
    check(ph_script_load(&script, path) && script.count == 5, "a script keeps the lines it understands");
    check(script.lines[1].command == PH_SCRIPT_CLICK && script.lines[1].x == 10 && script.lines[1].y == 20, "a script click has a position");
    check(script.lines[2].command == PH_SCRIPT_SHOT && !SDL_strcmp(script.lines[2].text, "tests/out/x.bmp"), "a script shot has a path");
    check(script.lines[3].command == PH_SCRIPT_DOWN && script.lines[4].command == PH_SCRIPT_UP && !SDL_strcmp(script.lines[4].text, "Left Shift"), "a script holds a key between down and up");
    const char *shot = "1 shot tests/out/script_shot.png\n";
    SDL_SaveFile(path, shot, SDL_strlen(shot));
    ph_script_load(&script, path);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    ph_script_step(&script, NULL, renderer);
    size_t size = 0;
    uint8_t *saved = (uint8_t *)SDL_LoadFile("tests/out/script_shot.png", &size);
    check(saved && size > 8 && !SDL_memcmp(saved, "\x89PNG", 4), "a script shot ending in .png saves a PNG");
    SDL_free(saved);
    SDL_RemovePath("tests/out/script_shot.png");
    SDL_RemovePath(path);
}

static void setup_launcher(struct ph_launcher *launcher, SDL_Window *window, struct ph_input *input, struct ph_touch *touch, struct ph_net *net)
{
    ph_launcher_init(launcher, window);
    launcher->settings = settings;
    launcher->setting_count = SDL_arraysize(settings);
    launcher->sections = section_names;
    launcher->section_count = SDL_arraysize(section_names);
    launcher->input = input;
    launcher->touch = touch;
    launcher->net = net;
    launcher->slots = 4;
    launcher->rom_ready = 1;
    launcher->rom_hint = "Choose game.zip from your ROM folder.";
    SDL_strlcpy(launcher->rom_status, "Found game.zip", sizeof launcher->rom_status);
}

static void test_block_held(void)
{
    struct ph_input input;
    setup_input(&input);
    input.blocked[0] = 1u << ACTION_SHOOT;
    check(!ph_input_held(&input, 0, ACTION_SHOOT) && input.blocked[0] == 0, "a blocked action unblocks once released");
    ph_input_block_held(&input);
    check(input.blocked[0] == 0 && ph_input_mask(&input, 0) == 0, "blocking with nothing held blocks nothing");
}

static void test_text(void)
{
    char fitted[64];
    float wide = ph_text_width("Settings", 24, PH_TEXT_BOLD);
    check(wide > ph_text_width("Settings", 24, PH_TEXT_MEDIUM) && wide > 80 && wide < 110, "bold text is wider than medium");
    check(ph_text_width("AV", 40, PH_TEXT_MEDIUM) < ph_text_width("A", 40, PH_TEXT_MEDIUM) + ph_text_width("V", 40, PH_TEXT_MEDIUM), "text is kerned");
    ph_text_fit("Arrange touch controls", 20, PH_TEXT_MEDIUM, 120, fitted, sizeof fitted);
    check(ph_text_width(fitted, 20, PH_TEXT_MEDIUM) <= 120 && SDL_strstr(fitted, "\xE2\x80\xA6"), "text fit adds an ellipsis");
    check(ph_text_width("caf\xC3\xA9", 20, PH_TEXT_MEDIUM) > ph_text_width("caf", 20, PH_TEXT_MEDIUM), "unknown characters still take space");
}

static void test_launcher(SDL_Window *window, SDL_Renderer *renderer)
{
    struct ph_launcher launcher;
    struct ph_input input;
    struct ph_touch touch;
    struct ph_net net;
    setup_input(&input);
    ph_touch_init(&touch, landscape, portrait, TOUCH_COUNT);
    ph_net_init(&net, "TEST", 47100, 1, 60);
    setup_launcher(&launcher, window, &input, &touch, &net);
    ph_settings_reset(settings, SDL_arraysize(settings));
    SDL_Event event = key_event(SDL_SCANCODE_RETURN);
    struct ph_launch launch = ph_launcher_event(&launcher, renderer, &event);
    check(launch.kind == PH_LAUNCH_PLAY, "launcher play");
    event = key_event(SDL_SCANCODE_DOWN);
    ph_launcher_event(&launcher, renderer, &event);
    ph_launcher_event(&launcher, renderer, &event);
    event = key_event(SDL_SCANCODE_RETURN);
    ph_launcher_event(&launcher, renderer, &event);
    check(launcher.menu.page == PH_PAGE_SETTINGS, "launcher opens settings category menu");
    ph_launcher_draw(&launcher, renderer);
    check(row_named(&launcher.menu, "Video") >= 0 && row_named(&launcher.menu, "Audio") >= 0 &&
          row_named(&launcher.menu, "Game") >= 0 && row_named(&launcher.menu, "Controls") >= 0 &&
          row_named(&launcher.menu, "System") >= 0,
          "settings category menu lists Video, Audio, Game, Controls and System");
    check(launcher.menu.layers[0].valid && launcher.menu.layers[1].valid &&
          launcher.menu.layers[0].panel.x < launcher.menu.width * 0.12f &&
          launcher.menu.layers[0].panel.y < launcher.menu.height * 0.12f &&
          launcher.menu.layers[1].panel.x > launcher.menu.layers[0].panel.x &&
          launcher.menu.layers[1].panel.y > launcher.menu.layers[0].panel.y,
          "root box anchors top-left and stacked submenu shifts down-right on top");
    event = key_event(SDL_SCANCODE_RETURN);
    ph_launcher_event(&launcher, renderer, &event);
    check(launcher.menu.page == PH_PAGE_SECTION_BASE + 0 && ph_launcher_is_settings_page(launcher.menu.page),
          "selecting Video opens nested Video settings page");
    event = key_event(SDL_SCANCODE_RIGHT);
    launch = ph_launcher_event(&launcher, renderer, &event);
    check(launch.kind == PH_LAUNCH_SETTING && launch.value == 0 && scale_mode == 2, "launcher adjusts a setting");
    event = key_event(SDL_SCANCODE_ESCAPE);
    ph_launcher_event(&launcher, renderer, &event);
    check(launcher.menu.page == PH_PAGE_SETTINGS, "escape from nested settings category returns to Settings");
    ph_launcher_event(&launcher, renderer, &event);
    check(launcher.menu.page == PH_PAGE_MAIN, "launcher goes back to main");
    ph_launcher_draw(&launcher, renderer);
    check(!launcher.menu.title[0] && launcher.menu.back.w == 0, "the main page has no title or back button");
    ph_menu_push(&launcher.menu, PH_PAGE_SETTINGS);
    ph_launcher_draw(&launcher, renderer);
    int back_rows = 0;
    for (int index = 0; index < launcher.menu.count; index++) {
        back_rows += !SDL_strcmp(launcher.menu.rows[index].label, "Back");
    }
    check(launcher.menu.back.w > 0 && !back_rows, "sub-pages have a back button instead of a Back row");
    SDL_FPoint back = center_of(ph_menu_pixels(&launcher.menu, launcher.menu.back));
    event = finger_event(SDL_EVENT_FINGER_DOWN, renderer, 9, back);
    ph_launcher_event(&launcher, renderer, &event);
    check(launcher.menu.press_back && !ph_menu_lit(&launcher.menu, launcher.menu.selected[PH_PAGE_SETTINGS]), "touch shows no selection while the back button is held");
    event = finger_event(SDL_EVENT_FINGER_UP, renderer, 9, back);
    ph_launcher_event(&launcher, renderer, &event);
    check(launcher.menu.page == PH_PAGE_MAIN && launcher.menu.depth == 1, "the back button returns to the previous page");
    launcher.menu.touch = 0;
    ph_menu_push(&launcher.menu, PH_PAGE_SETTINGS);
    ph_launcher_draw(&launcher, renderer);
    /* Tapping an exposed corner of the parent Main card pops back */
    SDL_FRect parent_px = ph_menu_pixels(&launcher.menu, launcher.menu.layers[0].panel);
    SDL_FPoint exposed_parent = { parent_px.x + 8.0f, parent_px.y + 8.0f };
    event = finger_event(SDL_EVENT_FINGER_DOWN, renderer, 10, exposed_parent);
    ph_launcher_event(&launcher, renderer, &event);
    event = finger_event(SDL_EVENT_FINGER_UP, renderer, 10, exposed_parent);
    ph_launcher_event(&launcher, renderer, &event);
    check(launcher.menu.page == PH_PAGE_MAIN && launcher.menu.depth == 1, "tapping an exposed parent card pops back");
    launcher.menu.touch = 0;
    ph_menu_push(&launcher.menu, PH_PAGE_SETTINGS);
    ph_launcher_draw(&launcher, renderer);
    launcher.menu.selected[PH_PAGE_SETTINGS] = row_named(&launcher.menu, "System");
    event = key_event(SDL_SCANCODE_RETURN);
    ph_launcher_event(&launcher, renderer, &event);
    ph_launcher_draw(&launcher, renderer);
    touch.size = 140;
    launcher.menu.selected[PH_PAGE_SYSTEM_SEC] = row_named(&launcher.menu, "Restore defaults");
    ph_launcher_event(&launcher, renderer, &event);
    check(touch.size == 100, "restore defaults in System resets the touch options");
    event = key_event(SDL_SCANCODE_ESCAPE);
    ph_launcher_event(&launcher, renderer, &event);
    ph_launcher_draw(&launcher, renderer);
    launcher.menu.selected[PH_PAGE_SETTINGS] = row_named(&launcher.menu, "Controls");
    event = key_event(SDL_SCANCODE_RETURN);
    ph_launcher_event(&launcher, renderer, &event);
    ph_launcher_draw(&launcher, renderer);
    int mappings = row_named(&launcher.menu, "Mappings");
    int touch_mode = row_named(&launcher.menu, "Touch controls");
    int arrange = row_named(&launcher.menu, "Arrange touch controls");
    check(launcher.menu.page == PH_PAGE_CONTROLS_SEC && mappings >= 0 && mappings < touch_mode && touch_mode < arrange,
          "Controls category page has Mappings, the touch options and Arrange touch controls");
    launcher.menu.selected[PH_PAGE_CONTROLS_SEC] = mappings;
    ph_launcher_event(&launcher, renderer, &event);
    ph_launcher_draw(&launcher, renderer);
    check(launcher.menu.page == PH_PAGE_MAPPINGS && launcher.menu.depth == 4 &&
          launcher.menu.layers[0].panel.x < launcher.menu.layers[1].panel.x &&
          launcher.menu.layers[1].panel.x < launcher.menu.layers[2].panel.x &&
          launcher.menu.layers[2].panel.x < launcher.menu.layers[3].panel.x &&
          launcher.menu.panel.x + launcher.menu.panel.w <= launcher.menu.width &&
          !SDL_strcmp(launcher.menu.title, "Mappings") && row_named(&launcher.menu, "Touch controls") < 0,
          "Mappings stacks cleanly as a 4th cascading card without the touch options");
    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    ph_launcher_draw(&launcher, renderer);
    launcher.menu.selected[PH_PAGE_MAIN] = row_named(&launcher.menu, "LAN play");
    ph_launcher_event(&launcher, renderer, &event);
    SDL_strlcpy(net.games[0].name, "Living room", sizeof net.games[0].name);
    SDL_strlcpy(net.games[0].address, "192.168.1.20", sizeof net.games[0].address);
    net.game_count = 1;
    ph_launcher_draw(&launcher, renderer);
    int host_row = row_named(&launcher.menu, "Host");
    check(launcher.menu.page == PH_PAGE_LAN && host_row >= 0 && row_named(&launcher.menu, "Living room") > host_row && row_named(&launcher.menu, "Your name") < 0 && row_named(&launcher.menu, "Join address") < 0,
          "LAN play is Host with the games found below it");
    net.game_count = 0;
    launcher.menu.selected[PH_PAGE_LAN] = host_row;
    ph_launcher_event(&launcher, renderer, &event);
    ph_launcher_draw(&launcher, renderer);
    check(launcher.menu.page == PH_PAGE_WAITING && net.status == PH_NET_HOSTING && row_named(&launcher.menu, "Your name") >= 0, "Host starts hosting and offers your name");
    ph_net_stop(&net);
    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    ph_launcher_close(&launcher);
    event = key_event(SDL_SCANCODE_ESCAPE);
    launch = ph_launcher_event(&launcher, renderer, &event);
    check(launch.kind == PH_LAUNCH_PAUSE && launcher.open && launcher.menu.page == PH_PAGE_PAUSE, "escape in game opens the pause menu");
    ph_launcher_draw(&launcher, renderer);
    check(launcher.menu.style == PH_STYLE_BIG && !launcher.menu.title[0] && launcher.menu.over_game, "pause menu is the main page card over the game");
    event = key_event(SDL_SCANCODE_ESCAPE);
    launch = ph_launcher_event(&launcher, renderer, &event);
    check(launch.kind == PH_LAUNCH_RESUME, "launcher back resumes from pause");
    ph_launcher_close(&launcher);
    SDL_zero(event);
    event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    launch = ph_launcher_event(&launcher, renderer, &event);
    check(launch.kind == PH_LAUNCH_PAUSE, "losing focus pauses");
    ph_launcher_close(&launcher);
    net.status = PH_NET_CONNECTED;
    launch = ph_launcher_event(&launcher, renderer, &event);
    check(launch.kind == PH_LAUNCH_NONE && !launcher.open, "losing focus does not pause a LAN game");
    net.status = PH_NET_OFF;
    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    ph_launcher_open(&launcher, PH_PAGE_MAPPINGS);
    ph_launcher_draw(&launcher, renderer);
    int binding_row = -1;
    for (int index = 0; index < launcher.menu.count && binding_row < 0; index++) {
        binding_row = launcher.menu.rows[index].kind == PH_ROW_PAIR ? index : -1;
    }
    launcher.menu.selected[PH_PAGE_MAPPINGS] = binding_row;
    event = key_event(SDL_SCANCODE_RETURN);
    ph_launcher_event(&launcher, renderer, &event);
    check(input.capturing && input.capture_action == launcher.menu.rows[binding_row].argument, "launcher starts capturing a binding");
    event = key_event(SDL_SCANCODE_W);
    launch = ph_launcher_event(&launcher, renderer, &event);
    check(launch.kind == PH_LAUNCH_SAVE_CONFIG && !input.capturing, "launcher capture finishes");
    check(input.bindings[0][ACTION_UP][PH_SLOT_KEY].code == SDL_SCANCODE_W, "launcher capture binds the key");
    ph_launcher_close(&launcher);
    ph_net_stop(&net);
    ph_touch_close(&touch);
}

static void save_png(SDL_Renderer *renderer, const char *name)
{
    char path[256];
    SDL_snprintf(path, sizeof path, "tests/out/%s.png", name);
    SDL_Surface *surface = SDL_RenderReadPixels(renderer, NULL);
    check(surface && SDL_SavePNG(surface, path), path);
    SDL_DestroySurface(surface);
}

static void draw_game(SDL_Renderer *renderer)
{
    SDL_FRect output = ph_view_output(renderer);
    SDL_SetRenderDrawColor(renderer, 30, 60, 110, 255);
    SDL_RenderClear(renderer);
    for (int row = 0; row < 12; row++) {
        for (int column = 0; column < 20; column++) {
            if ((row + column) % 2) {
                SDL_FRect tile = { output.w * column / 20, output.h * row / 12, output.w / 20, output.h / 12 };
                SDL_SetRenderDrawColor(renderer, 50, 90, 150, 255);
                SDL_RenderFillRect(renderer, &tile);
            }
        }
    }
}

static void clear(SDL_Renderer *renderer)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
}

static Uint8 red_at(SDL_Renderer *renderer, int x, int y)
{
    SDL_Surface *surface = SDL_RenderReadPixels(renderer, NULL);
    Uint8 red = 0;
    Uint8 green = 0;
    Uint8 blue = 0;
    if (surface) {
        SDL_ReadSurfacePixel(surface, x, y, &red, &green, &blue, NULL);
    }
    SDL_DestroySurface(surface);
    return red;
}

static SDL_Texture *white_picture(SDL_Renderer *renderer, int size)
{
    uint32_t pixels[64];
    for (int index = 0; index < size * size; index++) {
        pixels[index] = 0xFFFFFFFFu;
    }
    SDL_Texture *picture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, size, size);
    SDL_UpdateTexture(picture, NULL, pixels, size * (int)sizeof pixels[0]);
    return picture;
}

static void test_crt(SDL_Renderer *renderer)
{
    struct ph_crt crt = { 0 };
    struct ph_setting crt_settings[PH_CRT_SETTINGS];
    ph_crt_open(&crt, renderer);
    check(!crt.state, "the software renderer has no CRT shader");
    int count = ph_crt_settings(&crt, crt_settings, 2);
    ph_settings_reset(crt_settings, count);
    check(count == PH_CRT_SETTINGS && crt_settings[0].section == 2 && crt.filter == PH_FILTER_SHARP, "CRT settings fill five rows in the given section");
    check(ph_settings_take(crt_settings, count, "filter", "CRT") && crt.filter == PH_FILTER_CRT, "the filter setting reads CRT");
    ph_crt_settings(&crt, crt_settings, 2);
    check(crt_settings[2].show == PH_SHOW_HIDDEN && crt_settings[1].show == PH_SHOW_ALWAYS, "only scanlines show without the shader");
    SDL_Texture *picture = white_picture(renderer, 2);
    crt.scanlines = 3;
    clear(renderer);
    ph_crt_draw(&crt, picture, (SDL_FRect){ 0, 0, 8, 8 });
    check(red_at(renderer, 0, 0) == 255 && red_at(renderer, 0, 3) < 128, "scanlines darken the bottom of each line without the shader");
    crt.scanlines = 0;
    ph_crt_draw(&crt, picture, (SDL_FRect){ 0, 0, 8, 8 });
    check(red_at(renderer, 0, 3) == 255, "no scanlines leave the picture alone");
    crt.scanlines = 2;
    crt.curvature = 1;
    struct ph_crt_uniforms uniforms = ph_crt_uniforms_for(&crt, 320.0f, 224.0f);
    check(uniforms.source[0] == 320.0f && uniforms.source[1] == 224.0f && uniforms.tube[2] > 0.0f && ph_crt_scanline_alpha(3, 8, 2, 3) > 0,
          "CRT uniforms and scanline alpha helpers expose shader parameters");
    SDL_DestroyTexture(picture);
    ph_crt_close(&crt);
}

static void test_crt_shader(void)
{
    SDL_Window *window = SDL_InitSubSystem(SDL_INIT_VIDEO) ? SDL_CreateWindow("crt", 64, 64, SDL_WINDOW_HIDDEN) : NULL;
    SDL_Renderer *renderer = window ? ph_crt_create_renderer(window) : NULL;
    if (!renderer || SDL_strcmp(SDL_GetRendererName(renderer), SDL_GPU_RENDERER)) {
        printf("skipped the CRT shader check: no GPU renderer\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        return;
    }
    struct ph_crt crt = { .filter = PH_FILTER_CRT, .scanlines = 2, .curvature = 1, .glow = 1 };
    ph_crt_open(&crt, renderer);
    check(crt.state != NULL, "the CRT shader loads on the GPU renderer");
    SDL_Texture *picture = white_picture(renderer, 8);
    clear(renderer);
    ph_crt_draw(&crt, picture, (SDL_FRect){ 0, 0, 64, 64 });
    check(red_at(renderer, 36, 36) > 128 && red_at(renderer, 0, 0) == 0, "the CRT shader lights the middle and rounds the corners");
    SDL_DestroyTexture(picture);
    ph_crt_close(&crt);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

static void test_stick(SDL_Renderer *renderer)
{
    struct ph_touch touch;
    ph_touch_init(&touch, stick_layout, NULL, SDL_arraysize(stick_layout));
    ph_touch_layout(&touch, renderer);
    SDL_FRect safe = touch.safe;
    SDL_FPoint start = { safe.x + safe.w * 0.30f, safe.y + safe.h * 0.25f };
    SDL_Event event = finger_event(SDL_EVENT_FINGER_DOWN, renderer, 1, start);
    ph_touch_event(&touch, renderer, &event);
    check(touch.fingers[0].active && touch.fingers[0].kind == PH_TOUCH_STICK && touch.directions == 0, "the stick starts where the thumb lands on its half");
    float reach = touch.rects[0].w * 0.3f;
    event = finger_event(SDL_EVENT_FINGER_MOTION, renderer, 1, (SDL_FPoint){ start.x - reach, start.y - reach });
    ph_touch_event(&touch, renderer, &event);
    check(!touch.eight_way && touch.directions == (PH_TOUCH_UP | PH_TOUCH_LEFT), "the stick reads diagonals without the Diagonals setting");
    event = finger_event(SDL_EVENT_FINGER_UP, renderer, 1, start);
    ph_touch_event(&touch, renderer, &event);
    check(touch.directions == 0, "the stick lets go");
    event = finger_event(SDL_EVENT_FINGER_DOWN, renderer, 2, ph_rect_center(touch.rects[0]));
    ph_touch_event(&touch, renderer, &event);
    check(touch.fingers[0].active && touch.fingers[0].kind == PH_TOUCH_STICK, "the stick starts on its own resting place");
    event = finger_event(SDL_EVENT_FINGER_UP, renderer, 2, ph_rect_center(touch.rects[0]));
    ph_touch_event(&touch, renderer, &event);
    event = finger_event(SDL_EVENT_FINGER_DOWN, renderer, 3, (SDL_FPoint){ 1, safe.y + safe.h * 0.5f });
    ph_touch_event(&touch, renderer, &event);
    check(touch.fingers[0].active && touch.fingers[0].kind == PH_TOUCH_STICK, "the stick starts at the very edge of the screen");
    event = finger_event(SDL_EVENT_FINGER_UP, renderer, 3, (SDL_FPoint){ 1, safe.y + safe.h * 0.5f });
    ph_touch_event(&touch, renderer, &event);
    check(!diagonals_shown(&touch), "Diagonals is hidden when there is no pad");
    ph_touch_close(&touch);
    ph_touch_init(&touch, landscape, portrait, TOUCH_COUNT);
    check(diagonals_shown(&touch), "Diagonals shows for a pad");
    touch.mode = PH_TOUCH_ALWAYS;
    ph_touch_init(&touch, stick_layout, NULL, SDL_arraysize(stick_layout));
    touch.mode = PH_TOUCH_ALWAYS;
    clear(renderer);
    ph_touch_draw(&touch, renderer);
    save_png(renderer, "touch_stick");
    ph_touch_close(&touch);
}

static void test_notice_in_game(SDL_Renderer *renderer)
{
    struct ph_launcher launcher;
    ph_launcher_init(&launcher, NULL);
    ph_launcher_close(&launcher);
    clear(renderer);
    SDL_Surface *before = SDL_RenderReadPixels(renderer, NULL);
    ph_launcher_notice(&launcher, "Saved to slot 3");
    ph_launcher_draw(&launcher, renderer);
    SDL_Surface *after = SDL_RenderReadPixels(renderer, NULL);
    int changed = before && after && SDL_memcmp(before->pixels, after->pixels, (size_t)before->pitch * before->h) != 0;
    check(changed, "a notice shows during play with the launcher closed");
    save_png(renderer, "notice_in_game");
    SDL_DestroySurface(before);
    SDL_DestroySurface(after);
}

static SDL_Event drop_event(char *path)
{
    SDL_Event event;
    SDL_zero(event);
    event.type = SDL_EVENT_DROP_FILE;
    event.drop.data = path;
    return event;
}

static void test_launcher_game(SDL_Renderer *renderer)
{
    struct ph_launcher launcher;
    struct ph_input input;
    struct ph_touch touch;
    struct ph_net net;
    setup_input(&input);
    ph_touch_init(&touch, landscape, portrait, TOUCH_COUNT);
    touch.mode = PH_TOUCH_ALWAYS;
    ph_net_init(&net, "TEST", 47200, 1, 60);
    setup_launcher(&launcher, NULL, &input, &touch, &net);
    launcher.rom_import = "test-rom.bin";

    ph_launcher_close(&launcher);
    ph_touch_layout(&touch, renderer);
    SDL_Event event = finger_event(SDL_EVENT_FINGER_DOWN, renderer, 1, center_of(touch.rects[TOUCH_B]));
    ph_launcher_event(&launcher, renderer, &event);
    check(ph_touch_mask(&touch, &input) == 1u << ACTION_JUMP, "the launcher passes touches to the controls during play");
    event = finger_event(SDL_EVENT_FINGER_UP, renderer, 1, center_of(touch.rects[TOUCH_B]));
    ph_launcher_event(&launcher, renderer, &event);

    launcher.slots = 1;
    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    ph_menu_push(&launcher.menu, PH_PAGE_SETTINGS);
    ph_menu_push(&launcher.menu, PH_PAGE_SECTION_BASE + 1);
    ph_launcher_draw(&launcher, renderer);
    launcher.menu.selected[PH_PAGE_SECTION_BASE + 1] = row_named(&launcher.menu, "Volume");
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "launcher_settings_audio");
    ph_launcher_open(&launcher, PH_PAGE_PAUSE);
    ph_launcher_draw(&launcher, renderer);
    SDL_Event down_key = key_event(SDL_SCANCODE_DOWN);
    SDL_Event ok_key = key_event(SDL_SCANCODE_RETURN);
    ph_launcher_event(&launcher, renderer, &down_key);
    struct ph_launch one_save = ph_launcher_event(&launcher, renderer, &ok_key);
    check(one_save.kind == PH_LAUNCH_SAVE_STATE && one_save.value == 1, "single-slot pause menu saves directly without pushing a slot page");
    ph_launcher_event(&launcher, renderer, &down_key);
    struct ph_launch one_load = ph_launcher_event(&launcher, renderer, &ok_key);
    check(one_load.kind == PH_LAUNCH_LOAD_STATE && one_load.value == 1, "single-slot pause menu loads directly without pushing a slot page");
    launcher.slots = 4;
    ph_launcher_close(&launcher);

    char dropped[] = "/tmp/other.sfc";
    event = drop_event(dropped);
    ph_launcher_event(&launcher, renderer, &event);
    struct ph_launch launch = ph_launcher_update(&launcher);
    check(launch.kind == PH_LAUNCH_NONE && !SDL_strcmp(launcher.menu.notice, "Quit to the launcher to change the ROM."), "a ROM dropped during play is refused");
    ph_launcher_pause(&launcher);
    ph_menu_push(&launcher.menu, PH_PAGE_ROM);
    ph_launcher_draw(&launcher, renderer);
    check(row_named(&launcher.menu, "Choose file") < 0 && row_named(&launcher.menu, "Quit to the launcher to change the ROM.") >= 0, "the ROM page only explains itself during play");

    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    event = drop_event(dropped);
    ph_launcher_event(&launcher, renderer, &event);
    launch = ph_launcher_update(&launcher);
    check(launch.kind == PH_LAUNCH_ROM_PICKED && !SDL_strcmp(launcher.picked, dropped), "a ROM dropped on the launcher is picked");

    ph_launcher_rom_failed(&launcher, dropped, "is not the right game.");
    check(!launcher.rom_ready && !SDL_strcmp(launcher.rom_status, "other.sfc is not the right game."), "a failed ROM says why");
    char path[PH_PATH_SIZE];
    ph_import("rom", 3, launcher.rom_import);
    ph_user_path(launcher.rom_import, path, sizeof path);
    ph_launcher_open(&launcher, PH_PAGE_ROM);
    ph_launcher_rom_found(&launcher, path);
    check(launcher.rom_ready && launcher.rom_removable && !SDL_strcmp(launcher.rom_status, "test-rom.bin"), "an imported ROM is named and removable");
    ph_launcher_rom_imported(&launcher);
    check(launcher.menu.page == PH_PAGE_MAIN && !SDL_strcmp(launcher.menu.notice, "ROM loaded."), "an imported ROM returns to the main page");
    ph_menu_push(&launcher.menu, PH_PAGE_ROM);
    ph_launcher_draw(&launcher, renderer);
    launcher.menu.selected[PH_PAGE_ROM] = row_named(&launcher.menu, "Remove imported ROM");
    event = key_event(SDL_SCANCODE_RETURN);
    launch = ph_launcher_event(&launcher, renderer, &event);
    check(launch.kind == PH_LAUNCH_ROM_RESCAN && !launcher.rom_ready && !ph_readable(path), "removing the imported ROM deletes it and looks again");

    ph_launcher_close(&launcher);
    net.playing = 1;
    net.status = PH_NET_CONNECTED;
    net.stall_started = 1;
    launch = ph_launcher_update(&launcher);
    check(launch.kind == PH_LAUNCH_NONE && !SDL_strcmp(launcher.menu.notice, "Waiting for the other player..."), "a stalled LAN game says it is waiting");
    net.status = PH_NET_FAILED;
    SDL_strlcpy(net.message, "Guest left the game.", sizeof net.message);
    launch = ph_launcher_update(&launcher);
    check(launch.kind == PH_LAUNCH_END && launcher.open && launcher.menu.page == PH_PAGE_MAIN && !ph_net_playing(&net) && !SDL_strcmp(launcher.menu.notice, "Guest left the game."),
          "a lost LAN game ends and says why");
    ph_net_stop(&net);
    ph_touch_close(&touch);
}

static void render_launcher_pages(SDL_Window *window, SDL_Renderer *renderer)
{
    struct ph_launcher launcher;
    struct ph_input input;
    struct ph_touch touch;
    struct ph_net net;
    setup_input(&input);
    ph_touch_init(&touch, landscape, portrait, TOUCH_COUNT);
    ph_net_init(&net, "TEST", 47200, 1, 60);
    SDL_strlcpy(net.name, "Living room", sizeof net.name);
    setup_launcher(&launcher, window, &input, &touch, &net);
    ph_settings_reset(settings, SDL_arraysize(settings));
    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    ph_launcher_update(&launcher);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "launcher_main");

    ph_menu_push(&launcher.menu, PH_PAGE_SETTINGS);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "launcher_settings");

    ph_menu_push(&launcher.menu, PH_PAGE_SECTION_BASE + 0);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "launcher_settings_video");

    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    ph_menu_push(&launcher.menu, PH_PAGE_SETTINGS);
    ph_menu_push(&launcher.menu, PH_PAGE_CONTROLS_SEC);
    ph_menu_push(&launcher.menu, PH_PAGE_MAPPINGS);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "launcher_mappings");

    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    ph_menu_push(&launcher.menu, PH_PAGE_SETTINGS);
    ph_menu_push(&launcher.menu, PH_PAGE_SYSTEM_SEC);
    ph_menu_push(&launcher.menu, PH_PAGE_ROM);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "launcher_rom");

    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    ph_menu_push(&launcher.menu, PH_PAGE_LAN);
    ph_launcher_update(&launcher);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "launcher_lan");

    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    ph_menu_push(&launcher.menu, PH_PAGE_LOAD);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "launcher_load");

    ph_launcher_open(&launcher, PH_PAGE_PAUSE);
    draw_game(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "launcher_pause");
    launcher.menu.touch = 1;
    draw_game(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "launcher_pause_touch");
    launcher.menu.touch = 0;
    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    ph_menu_push(&launcher.menu, PH_PAGE_SETTINGS);
    ph_menu_push(&launcher.menu, PH_PAGE_CONTROLS_SEC);
    ph_launcher_draw(&launcher, renderer);
    SDL_Event event = key_event(SDL_SCANCODE_RETURN);
    launcher.menu.selected[PH_PAGE_CONTROLS_SEC] = row_named(&launcher.menu, "Arrange touch controls");
    ph_launcher_event(&launcher, renderer, &event);
    draw_game(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "touch_arrange");
    ph_menu_flavor = PH_FLAVOR_MINT;
    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "flavor_mint");
    ph_menu_flavor = PH_FLAVOR_STRAWBERRY;
    ph_menu_push(&launcher.menu, PH_PAGE_SETTINGS);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "flavor_strawberry");
    ph_menu_reset_flavor();
    /* Verify missing-ROM status renders in a separate bottom-right box */
    launcher.rom_ready = 0;
    launcher.rom_status[0] = 0;
    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "launcher_main_no_rom");
    int note_idx = row_named(&launcher.menu, "No ROM yet. Choose Play to add one.");
    check(note_idx >= 0 && launcher.menu.aside.w > 0 && launcher.menu.aside.x > launcher.menu.panel.x + launcher.menu.panel.w &&
          launcher.menu.boxes[note_idx].x >= launcher.menu.aside.x,
          "missing ROM hint sits in its own bottom-right window box");

    /* Verify per-game custom color themes */
    struct ph_menu_theme ocean_theme = ph_theme_make(0x1E4D7B, 0x173D63, 0x102136, 0xF2F1E8, 0xFFD23F);
    launcher.menu.theme = &ocean_theme;
    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "theme_ocean_main");
    ph_menu_push(&launcher.menu, PH_PAGE_SETTINGS);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    check(launcher.menu.aside.w > 0, "missing ROM box stays framed behind stacked submenus");
    save_png(renderer, "theme_ocean_settings");
    launcher.menu.theme = &ph_menu_default_theme;
    launcher.rom_ready = 1;

    /* Verify low-res 4:3 CRT rendering at 640x480 (480p) and 320x240 (240p) */
    const int crt_modes[2][2] = { { 640, 480 }, { 320, 240 } };
    const char *const crt_tags[2] = { "crt_480p", "crt_240p" };
    for (int m = 0; m < 2; m++) {
        SDL_Surface *crt_surf = SDL_CreateSurface(crt_modes[m][0], crt_modes[m][1], SDL_PIXELFORMAT_RGBA32);
        SDL_Renderer *crt_ren = SDL_CreateSoftwareRenderer(crt_surf);
        launcher.rom_ready = 0;
        launcher.rom_status[0] = 0;
        ph_launcher_open(&launcher, PH_PAGE_MAIN);
        clear(crt_ren);
        ph_launcher_draw(&launcher, crt_ren);
        check(launcher.menu.aside.w > 0 && launcher.menu.panel.x < launcher.menu.width * 0.15f &&
              launcher.menu.panel.y < launcher.menu.height * 0.15f &&
              launcher.menu.aside.x + launcher.menu.aside.w <= launcher.menu.width,
              "4:3 CRT main screen fits top-left menu and bottom-right ROM box");
        char name_buf[64];
        SDL_snprintf(name_buf, sizeof name_buf, "%s_main", crt_tags[m]);
        save_png(crt_ren, name_buf);

        launcher.rom_ready = 1;
        ph_menu_push(&launcher.menu, PH_PAGE_SETTINGS);
        ph_menu_push(&launcher.menu, PH_PAGE_SECTION_BASE + 0);
        clear(crt_ren);
        ph_launcher_draw(&launcher, crt_ren);
        check(launcher.menu.depth == 3 &&
              launcher.menu.layers[0].panel.x < launcher.menu.layers[1].panel.x &&
              launcher.menu.layers[1].panel.x < launcher.menu.layers[2].panel.x &&
              launcher.menu.panel.x + launcher.menu.panel.w <= launcher.menu.width &&
              launcher.menu.panel.y + launcher.menu.panel.h <= launcher.menu.height,
              "4:3 CRT nested settings stack cleanly in bounds");
        SDL_snprintf(name_buf, sizeof name_buf, "%s_settings", crt_tags[m]);
        save_png(crt_ren, name_buf);

        SDL_DestroyRenderer(crt_ren);
        SDL_DestroySurface(crt_surf);
    }

    ph_view_set_window(renderer, window);
    check(ph_view_density(renderer) > 0 && ph_view_points(renderer) > 0, "ph_view_set_window attaches window metrics to a software renderer");
    ph_launcher_close(&launcher);
    ph_net_stop(&net);
    ph_touch_close(&touch);
}

static const struct ph_menu_theme green_theme = {
    { 0, 0.322f, 0.149f, 1 },
    { 0, 0.118f, 0.055f, 0.706f },
    { 0, 0.165f, 0.075f, 0.933f },
    { 0.933f, 0.957f, 0.91f, 1 },
    { 1, 1, 1, 1 },
    { 0.604f, 0.769f, 0.651f, 1 },
    { 1, 0.663f, 0.239f, 1 },
    { 0.298f, 0.741f, 0.878f, 1 },
    { 1, 0.827f, 0.431f, 1 },
    { 1, 0.49f, 0.42f, 1 },
};

static SDL_Texture *make_logo(SDL_Renderer *renderer)
{
    SDL_Surface *surface = SDL_CreateSurface(200, 300, SDL_PIXELFORMAT_RGBA32);
    SDL_FillSurfaceRect(surface, NULL, SDL_MapSurfaceRGBA(surface, 0, 0, 0, 0));
    for (int ring = 0; ring < 6; ring++) {
        for (int step = 0; step < 720; step++) {
            float angle = step * SDL_PI_F / 360;
            float radius = 30.0f + ring * 14;
            SDL_Rect dot = { (int)(120 + SDL_cosf(angle) * radius), (int)(150 + SDL_sinf(angle) * radius * 1.3f), 3, 3 };
            SDL_FillSurfaceRect(surface, &dot, SDL_MapSurfaceRGBA(surface, 255, 255, 255, 255));
        }
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_SetTextureColorMod(texture, 0xFF, 0xA9, 0x3D);
    SDL_DestroySurface(surface);
    return texture;
}

static SDL_Texture *make_background(SDL_Renderer *renderer)
{
    SDL_Surface *surface = SDL_CreateSurface(320, 180, SDL_PIXELFORMAT_RGBA32);
    for (int y = 0; y < surface->h; y++) {
        for (int x = 0; x < surface->w; x++) {
            int stripe = (x + y) / 12 % 2;
            SDL_Rect dot = { x, y, 1, 1 };
            SDL_FillSurfaceRect(surface, &dot, SDL_MapSurfaceRGBA(surface, (Uint8)(20 + x / 4), (Uint8)(90 + stripe * 30), (Uint8)(60 + y / 2), 255));
        }
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);
    return texture;
}

static void render_green_theme(SDL_Renderer *renderer)
{
    struct ph_launcher launcher;
    struct ph_input input;
    struct ph_net net;
    setup_input(&input);
    ph_net_init(&net, "TEST", 47400, 1, 60);
    SDL_strlcpy(net.name, "Living room", sizeof net.name);
    setup_launcher(&launcher, NULL, &input, NULL, &net);
    ph_settings_reset(settings, SDL_arraysize(settings));
    launcher.menu.theme = &green_theme;
    launcher.menu.background = make_background(renderer);
    launcher.menu.logo = make_logo(renderer);
    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "green_main");
    ph_menu_push(&launcher.menu, PH_PAGE_SETTINGS);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "green_settings");
    ph_menu_push(&launcher.menu, PH_PAGE_CONTROLS_SEC);
    ph_menu_push(&launcher.menu, PH_PAGE_MAPPINGS);
    launcher.menu.selected[PH_PAGE_MAPPINGS] = 4;
    launcher.menu.column = 1;
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "green_mappings");
    ph_input_capture(&input, 0, ACTION_SHOOT, PH_SLOT_KEY);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "green_capture");
    input.capturing = 0;
    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    ph_menu_push(&launcher.menu, PH_PAGE_ROM);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "green_rom");
    launcher.rom_ready = 0;
    SDL_strlcpy(launcher.rom_status, "game.zip is missing part 3 (crc 1a2b3c4d).", sizeof launcher.rom_status);
    SDL_Texture *logo = launcher.menu.logo;
    launcher.menu.logo = NULL;
    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "green_main_no_rom");
    launcher.menu.logo = logo;
    launcher.rom_ready = 1;
    ph_net_host(&net);
    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    ph_menu_push(&launcher.menu, PH_PAGE_LAN);
    ph_menu_push(&launcher.menu, PH_PAGE_WAITING);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "green_hosting");
    ph_net_stop(&net);
    ph_launcher_open(&launcher, PH_PAGE_PAUSE);
    draw_game(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "green_pause");
    ph_menu_push(&launcher.menu, PH_PAGE_SETTINGS);
    launcher.menu.touch = 1;
    draw_game(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "green_pause_settings_touch");
    launcher.menu.touch = 0;
    ph_launcher_open(&launcher, PH_PAGE_MAIN);
    launcher.menu.touch = 1;
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "green_main_touch");
    SDL_DestroyTexture(launcher.menu.background);
    SDL_DestroyTexture(launcher.menu.logo);
    ph_launcher_close(&launcher);
}

static void render_launcher_portrait(SDL_Renderer *renderer)
{
    struct ph_launcher launcher;
    struct ph_input input;
    setup_input(&input);
    setup_launcher(&launcher, NULL, &input, NULL, NULL);
    ph_launcher_open(&launcher, PH_PAGE_MAPPINGS);
    clear(renderer);
    ph_launcher_draw(&launcher, renderer);
    save_png(renderer, "launcher_portrait");
}

static void render_touch(SDL_Renderer *renderer, const char *name)
{
    struct ph_touch touch;
    ph_touch_init(&touch, landscape, portrait, TOUCH_COUNT);
    touch.mode = PH_TOUCH_ALWAYS;
    ph_touch_layout(&touch, renderer);
    SDL_Event event = finger_event(SDL_EVENT_FINGER_DOWN, renderer, 1, center_of(touch.rects[TOUCH_A]));
    ph_touch_event(&touch, renderer, &event);
    SDL_FPoint pad = center_of(touch.rects[TOUCH_PAD]);
    event = finger_event(SDL_EVENT_FINGER_DOWN, renderer, 2, (SDL_FPoint){ pad.x, pad.y - touch.rects[TOUCH_PAD].h * 0.35f });
    ph_touch_event(&touch, renderer, &event);
    draw_game(renderer);
    ph_touch_draw(&touch, renderer);
    save_png(renderer, name);
    ph_touch_close(&touch);
}

static void render_debug(SDL_Renderer *renderer)
{
    struct ph_debug debug;
    struct ph_net net;
    static uint8_t memory[4096];
    int offset = 0;
    int wireframe = 1;
    int speed = 100;
    ph_net_init(&net, "TEST", 47300, 1, 60);
    ph_debug_init(&debug);
    debug.visible = 1;
    for (int index = 0; index < PH_DEBUG_HISTORY; index++) {
        debug.frame_ms[index] = 16.6f + (index % 30 == 0 ? 9.0f : 0.0f) + SDL_sinf(index * 0.2f);
        debug.work_ms[index] = 4.0f + SDL_sinf(index * 0.05f) * 2.0f;
        memory[index] = (uint8_t)(index * 13);
    }
    SDL_memcpy(memory + 32, "PORT HELPERS", 12);
    debug.history_count = PH_DEBUG_HISTORY;
    for (int tab = 0; tab < 3; tab++) {
        debug.tab = tab;
        for (int pass = 0; pass < 2; pass++) {
            draw_game(renderer);
            if (ph_debug_begin(&debug, renderer)) {
                if (ph_debug_tab(&debug, "Performance")) {
                    ph_debug_performance(&debug);
                    ph_debug_toggle(&debug, "Wireframe", &wireframe);
                    ph_debug_stepper(&debug, "Speed %", &speed, 10, 400, 10);
                    ph_debug_button(&debug, "Step frame");
                    ph_debug_same_line(&debug);
                    ph_debug_button(&debug, "Dump state");
                }
                if (ph_debug_tab(&debug, "Network")) {
                    ph_debug_network(&debug, &net);
                }
                if (ph_debug_tab(&debug, "Memory")) {
                    ph_debug_heading(&debug, "Work RAM");
                    ph_debug_memory(&debug, memory, sizeof memory, 0x100000, &offset);
                }
                ph_debug_end(&debug);
            }
        }
        char name[32];
        SDL_snprintf(name, sizeof name, "debug_%d", tab);
        save_png(renderer, name);
    }
    check(debug.content_height > 0, "debug overlay measured its content");
}

static void render_text(SDL_Renderer *renderer)
{
    clear(renderer);
    struct ph_render_state state = ph_overlay_begin(renderer);
    float y = 20;
    for (float size = 12; size <= 48; size += 9) {
        ph_text(renderer, "Pixelify Medium: The quick brown fox 0123456789 !?<>[] AVATAR", 20, y, size, PH_TEXT_MEDIUM, ph_color(0xFFFFFF, 1));
        y += size * 1.2f;
        ph_text(renderer, "Pixelify Bold: The quick brown fox 0123456789 !?<>[] AVATAR", 20, y, size, PH_TEXT_BOLD, ph_color(0xFFA93D, 1));
        y += size * 1.4f;
    }
    ph_fill_round(renderer, (SDL_FRect){ 40, y, 300, 80 }, 16, ph_color(0x3A6FD8, 1));
    ph_stroke_round(renderer, (SDL_FRect){ 380, y, 300, 80 }, 16, 3, ph_color(0xF2B33D, 1));
    ph_fill_circle(renderer, 800, y + 40, 40, ph_color(0xD84A3A, 0.8f));
    ph_line(renderer, (SDL_FPoint){ 900, y }, (SDL_FPoint){ 1200, y + 80 }, 6, ph_color(0x6BD968, 1));
    ph_overlay_end(renderer, state);
    save_png(renderer, "draw");
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_CreateDirectory("tests/out");
    test_hash();
    test_zip();
    test_file_name();
    test_settings();
    test_config();
    test_net();
    test_net_lossy();
    test_block_held();
    test_text();
    test_fullscreen_key();
    SDL_Surface *wide = SDL_CreateSurface(1280, 720, SDL_PIXELFORMAT_XRGB8888);
    SDL_Surface *tall = SDL_CreateSurface(720, 1280, SDL_PIXELFORMAT_XRGB8888);
    SDL_Renderer *renderer = wide ? SDL_CreateSoftwareRenderer(wide) : NULL;
    SDL_Renderer *portrait_renderer = tall ? SDL_CreateSoftwareRenderer(tall) : NULL;
    check(renderer && portrait_renderer, "software renderers");
    if (renderer && portrait_renderer) {
        test_touch(renderer);
        test_crt(renderer);
        test_script(renderer);
        test_stick(renderer);
        test_notice_in_game(renderer);
        test_launcher(NULL, renderer);
        test_launcher_game(renderer);
        render_text(renderer);
        render_launcher_pages(NULL, renderer);
        render_green_theme(renderer);
        render_touch(renderer, "touch_landscape");
        render_debug(renderer);
        render_touch(portrait_renderer, "touch_portrait");
        render_launcher_portrait(portrait_renderer);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyRenderer(portrait_renderer);
    SDL_DestroySurface(wide);
    SDL_DestroySurface(tall);
    test_crt_shader();
    char folder[PH_PATH_SIZE];
    if (ph_user_path("", folder, sizeof folder)) {
        SDL_RemovePath(folder);
    }
    SDL_Quit();
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
