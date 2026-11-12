#define PH_IMPLEMENTATION
#include "ph.h"

#include <stdio.h>

enum { FIELD_WIDTH = 320, FIELD_HEIGHT = 224, PLAYER_SIZE = 12, COIN_SIZE = 8 };
enum { ACTION_UP, ACTION_DOWN, ACTION_LEFT, ACTION_RIGHT, ACTION_DASH, ACTION_COUNT };
enum { ITEM_ABOUT = 1 };
enum { SETTING_SCALING, SETTING_FULLSCREEN, SETTING_CRT, SETTING_SHOW_FPS = SETTING_CRT + PH_CRT_SETTINGS, SETTING_COUNT };
enum { DEMO_PORT = 47010, MAX_CATCH_UP = 4 };

struct player {
    int32_t x;
    int32_t y;
    int32_t score;
};

struct world {
    struct player players[2];
    int32_t coin_x;
    int32_t coin_y;
    uint32_t seed;
    uint32_t frame;
};

struct demo {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *screen;
    struct ph_crt crt;
    struct ph_input input;
    struct ph_touch touch;
    struct ph_net net;
    struct ph_launcher launcher;
    struct ph_debug debug;
    struct world world;
    int running;
    int netplay;
    int quit;
    int scaling;
    int fullscreen;
    int show_fps;
    int paused_frames;
    uint64_t next_frame_ns;
};

static const char *const scaling_names[] = { "Integer", "Fit", "Stretch" };
static const char *const section_names[] = { "Video", "Interface" };
static const char *const action_keys[] = { "up", "down", "left", "right", "dash" };
static const char *const action_labels[] = { "Up", "Down", "Left", "Right", "Dash" };
static const int player_one_keys[] = { SDL_SCANCODE_W, SDL_SCANCODE_S, SDL_SCANCODE_A, SDL_SCANCODE_D, SDL_SCANCODE_LSHIFT };
static const int player_two_keys[] = { SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_RSHIFT };
static const int pad_buttons[] = { SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, SDL_GAMEPAD_BUTTON_SOUTH };

static const struct ph_touch_control landscape_controls[] = {
    { "pad", "", PH_TOUCH_PAD, 0.13f, 0.70f, 150, 0, 0, 0 },
    { "dash", "Dash", PH_TOUCH_BUTTON, 0.87f, 0.72f, 78, 0, 0, 1u << ACTION_DASH },
    { "menu", "", PH_TOUCH_PAUSE, 0.05f, 0.09f, 44, 44, 0, 0 },
};

static const struct ph_touch_control portrait_controls[] = {
    { "pad", "", PH_TOUCH_PAD, 0.26f, 0.82f, 150, 0, 0, 0 },
    { "dash", "Dash", PH_TOUCH_BUTTON, 0.76f, 0.84f, 78, 0, 0, 1u << ACTION_DASH },
    { "menu", "", PH_TOUCH_PAUSE, 0.08f, 0.05f, 44, 44, 0, 0 },
};

static const struct ph_launcher_item launcher_items[] = {
    { PH_PAGE_MAIN, ITEM_ABOUT, "About" },
};

static struct demo demo;

static struct ph_setting settings[SETTING_COUNT] = {
    [SETTING_SCALING] = { .key = "scaling", .label = "Scaling", .section = 0, .value = &demo.scaling, .fallback = PH_SCALE_INTEGER, .choices = scaling_names, .choice_count = 3 },
    [SETTING_FULLSCREEN] = { .key = "fullscreen", .label = "Fullscreen", .section = 0, .show = PH_SHOW_DESKTOP, .value = &demo.fullscreen, .choices = ph_off_on, .choice_count = 2 },
    [SETTING_SHOW_FPS] = { .key = "show_fps", .label = "Show FPS", .section = 1, .value = &demo.show_fps, .choices = ph_off_on, .choice_count = 2 },
};

static const struct ph_config config = { settings, SETTING_COUNT, &demo.input, &demo.touch, &demo.net };

static uint32_t next_random(uint32_t *seed)
{
    *seed = *seed * 1664525u + 1013904223u;
    return *seed >> 8;
}

static void place_coin(struct world *world)
{
    world->coin_x = (int32_t)(next_random(&world->seed) % (FIELD_WIDTH - COIN_SIZE * 4)) + COIN_SIZE * 2;
    world->coin_y = (int32_t)(next_random(&world->seed) % (FIELD_HEIGHT - COIN_SIZE * 4)) + COIN_SIZE * 2;
}

static struct world new_world(uint32_t seed)
{
    struct world world;
    SDL_zero(world);
    world.players[0] = (struct player){ FIELD_WIDTH / 3, FIELD_HEIGHT / 2, 0 };
    world.players[1] = (struct player){ FIELD_WIDTH * 2 / 3, FIELD_HEIGHT / 2, 0 };
    world.seed = seed | 1;
    place_coin(&world);
    return world;
}

static int held(uint32_t inputs, int action)
{
    return (inputs >> action & 1) != 0;
}

static void move_player(struct player *player, uint32_t inputs)
{
    int speed = held(inputs, ACTION_DASH) ? 4 : 2;
    player->x += (held(inputs, ACTION_RIGHT) - held(inputs, ACTION_LEFT)) * speed;
    player->y += (held(inputs, ACTION_DOWN) - held(inputs, ACTION_UP)) * speed;
    player->x = SDL_clamp(player->x, 0, FIELD_WIDTH - PLAYER_SIZE);
    player->y = SDL_clamp(player->y, 0, FIELD_HEIGHT - PLAYER_SIZE);
}

static int touches_coin(const struct world *world, const struct player *player)
{
    return player->x < world->coin_x + COIN_SIZE && world->coin_x < player->x + PLAYER_SIZE && player->y < world->coin_y + COIN_SIZE &&
        world->coin_y < player->y + PLAYER_SIZE;
}

static void step_world(struct world *world, const uint32_t inputs[2])
{
    for (int index = 0; index < 2; index++) {
        move_player(&world->players[index], inputs[index]);
        if (touches_coin(world, &world->players[index])) {
            world->players[index].score++;
            place_coin(world);
        }
    }
    world->frame++;
}

static uint32_t world_hash(const struct world *world)
{
    return ph_fnv32(PH_FNV32_START, world, sizeof *world);
}

static void save_config(void)
{
    ph_config_save(&config, "demo.cfg");
}

static void apply_settings(void)
{
    SDL_SetWindowFullscreen(demo.window, demo.fullscreen != 0);
    ph_crt_settings(&demo.crt, &settings[SETTING_CRT], 0);
}

static void start_game(uint32_t seed, int netplay)
{
    demo.world = new_world(seed);
    demo.running = 1;
    demo.netplay = netplay;
    demo.next_frame_ns = SDL_GetTicksNS();
    if (netplay) {
        ph_net_begin(&demo.net);
    }
    ph_launcher_close(&demo.launcher);
}

static void slot_notice(const char *verb, int slot, int ok)
{
    char text[96];
    SDL_snprintf(text, sizeof text, ok ? "%s slot %d." : "Could not %s slot %d.", verb, slot);
    ph_launcher_notice(&demo.launcher, text);
}

static void save_state(int slot)
{
    char path[PH_PATH_SIZE];
    int ok = ph_slot_path(slot, path, sizeof path) && SDL_SaveFile(path, &demo.world, sizeof demo.world);
    slot_notice(ok ? "Saved to" : "save to", slot, ok);
}

static void load_state(int slot)
{
    char path[PH_PATH_SIZE];
    size_t size = 0;
    void *data = ph_slot_path(slot, path, sizeof path) ? SDL_LoadFile(path, &size) : NULL;
    if (data && size == sizeof demo.world) {
        SDL_memcpy(&demo.world, data, sizeof demo.world);
        ph_launcher_close(&demo.launcher);
    } else {
        slot_notice("load", slot, 0);
    }
    SDL_free(data);
}

static void use_rom(const char *path)
{
    size_t size = 0;
    void *data = SDL_LoadFile(path, &size);
    if (!data) {
        ph_launcher_notice(&demo.launcher, "Could not read that file.");
        return;
    }
    char sha1[PH_SHA1_TEXT_SIZE];
    ph_sha1_text(data, size, sha1);
    demo.net.identity = ph_fnv32(PH_FNV32_START, sha1, PH_SHA1_SIZE * 2);
    SDL_snprintf(demo.launcher.rom_status, sizeof demo.launcher.rom_status, "%s (%.8s)", ph_file_name(path), sha1);
    ph_launcher_notice(&demo.launcher, "LAN games now only match players using the same file.");
    SDL_free(data);
}

static void act(struct ph_launch launch)
{
    switch (launch.kind) {
    case PH_LAUNCH_PLAY:
        start_game((uint32_t)SDL_GetTicks(), 0);
        break;
    case PH_LAUNCH_RESUME:
        ph_launcher_close(&demo.launcher);
        demo.next_frame_ns = SDL_GetTicksNS();
        break;
    case PH_LAUNCH_RESET:
        start_game(demo.world.seed, 0);
        break;
    case PH_LAUNCH_END:
        demo.running = 0;
        demo.netplay = 0;
        break;
    case PH_LAUNCH_QUIT:
        demo.quit = 1;
        break;
    case PH_LAUNCH_SAVE_STATE:
        save_state(launch.value);
        break;
    case PH_LAUNCH_LOAD_STATE:
        load_state(launch.value);
        break;
    case PH_LAUNCH_NETPLAY:
        start_game(demo.net.session, 1);
        break;
    case PH_LAUNCH_SETTING:
        apply_settings();
        save_config();
        break;
    case PH_LAUNCH_SAVE_CONFIG:
        save_config();
        break;
    case PH_LAUNCH_ROM_PICKED:
        use_rom(demo.launcher.picked);
        break;
    case PH_LAUNCH_CUSTOM:
        ph_launcher_notice(&demo.launcher, "Port helpers demo: chase the coin, F1 shows the debug overlay.");
        break;
    default:
        break;
    }
}

static void handle_event(const SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        demo.quit = 1;
        return;
    }
    if (ph_debug_event(&demo.debug, demo.renderer, event)) {
        return;
    }
    if (ph_view_fullscreen_key(event)) {
        demo.fullscreen = !demo.fullscreen;
        apply_settings();
        save_config();
        return;
    }
    act(ph_launcher_event(&demo.launcher, demo.renderer, event));
}

static int run_frame(void)
{
    uint32_t local = demo.launcher.open ? 0 : ph_input_mask(&demo.input, 0) | ph_touch_mask(&demo.touch, &demo.input);
    if (!demo.netplay) {
        uint32_t inputs[2] = { local, ph_input_mask(&demo.input, 1) };
        step_world(&demo.world, inputs);
        return 1;
    }
    uint32_t inputs[2];
    if (ph_net_frame(&demo.net, local, inputs) != PH_NET_READY) {
        return 0;
    }
    step_world(&demo.world, inputs);
    if (ph_net_check_due(&demo.net)) {
        ph_net_check(&demo.net, world_hash(&demo.world));
    }
    return 1;
}

static void update(void)
{
    act(ph_launcher_update(&demo.launcher));
    int playing = demo.running && (demo.netplay || !demo.launcher.open);
    uint64_t frame_ns = SDL_NS_PER_SECOND / 60;
    uint64_t now = SDL_GetTicksNS();
    if (!playing) {
        demo.next_frame_ns = now;
        return;
    }
    if (now - demo.next_frame_ns > frame_ns * MAX_CATCH_UP) {
        demo.next_frame_ns = now - frame_ns;
    }
    uint64_t started = now;
    while (demo.next_frame_ns <= now && demo.running && run_frame()) {
        demo.next_frame_ns += frame_ns;
    }
    ph_debug_frame(&demo.debug, (float)(SDL_GetTicksNS() - started) / 1000000.0f);
}

static void draw_box(int x, int y, int size, uint32_t rgb)
{
    SDL_FRect rect = { (float)x, (float)y, (float)size, (float)size };
    SDL_SetRenderDrawColor(demo.renderer, rgb >> 16 & 0xFF, rgb >> 8 & 0xFF, rgb & 0xFF, 255);
    SDL_RenderFillRect(demo.renderer, &rect);
}

static void draw_field(void)
{
    SDL_SetRenderTarget(demo.renderer, demo.screen);
    SDL_SetRenderDrawColor(demo.renderer, 18, 28, 52, 255);
    SDL_RenderClear(demo.renderer);
    for (int x = 0; x < FIELD_WIDTH; x += 16) {
        for (int y = (x / 16) % 2 * 16; y < FIELD_HEIGHT; y += 32) {
            draw_box(x, y, 16, 0x16223F);
        }
    }
    const struct world *world = &demo.world;
    draw_box(world->coin_x, world->coin_y, COIN_SIZE, 0xF2C94C);
    draw_box(world->players[0].x, world->players[0].y, PLAYER_SIZE, 0x4FA3FF);
    draw_box(world->players[1].x, world->players[1].y, PLAYER_SIZE, 0xFF6B6B);
    char score[32];
    SDL_snprintf(score, sizeof score, "%d", world->players[0].score);
    SDL_SetRenderDrawColor(demo.renderer, 0x4F, 0xA3, 0xFF, 255);
    SDL_RenderDebugText(demo.renderer, 8, 8, score);
    SDL_snprintf(score, sizeof score, "%d", world->players[1].score);
    SDL_SetRenderDrawColor(demo.renderer, 0xFF, 0x6B, 0x6B, 255);
    SDL_RenderDebugText(demo.renderer, FIELD_WIDTH - 8 - 8 * (float)SDL_strlen(score), 8, score);
    SDL_SetRenderTarget(demo.renderer, NULL);
}

static void draw_debug(void)
{
    if (!ph_debug_begin(&demo.debug, demo.renderer)) {
        return;
    }
    if (ph_debug_tab(&demo.debug, "Performance")) {
        ph_debug_performance(&demo.debug);
        ph_debug_toggle(&demo.debug, "Show FPS", &demo.show_fps);
    }
    if (ph_debug_tab(&demo.debug, "Network")) {
        ph_debug_network(&demo.debug, &demo.net);
    }
    if (ph_debug_tab(&demo.debug, "Game")) {
        ph_debug_text(&demo.debug, "Frame %u  seed %08X  hash %08X", demo.world.frame, demo.world.seed, world_hash(&demo.world));
        for (int index = 0; index < 2; index++) {
            const struct player *player = &demo.world.players[index];
            ph_debug_text(&demo.debug, "Player %d at %d,%d score %d", index + 1, player->x, player->y, player->score);
        }
        if (ph_debug_button(&demo.debug, "Move coin") && !demo.netplay) {
            place_coin(&demo.world);
        }
        ph_debug_stepper(&demo.debug, "Player 1 score", &demo.world.players[0].score, 0, 999, 1);
        ph_debug_heading(&demo.debug, "World bytes");
        static int offset;
        ph_debug_memory(&demo.debug, (const uint8_t *)&demo.world, sizeof demo.world, 0, &offset);
    }
    ph_debug_end(&demo.debug);
}

static void draw_fps(void)
{
    if (!demo.show_fps || !demo.debug.history_count) {
        return;
    }
    int last = (demo.debug.history_at + PH_DEBUG_HISTORY - 1) % PH_DEBUG_HISTORY;
    float frame_ms = demo.debug.frame_ms[last];
    char text[32];
    SDL_snprintf(text, sizeof text, "%.0f FPS", frame_ms > 0 ? 1000.0 / frame_ms : 0.0);
    struct ph_render_state state = ph_overlay_begin(demo.renderer);
    SDL_FRect safe = ph_view_safe(demo.renderer);
    float points = ph_view_points(demo.renderer);
    float size = 16 * points;
    ph_text(demo.renderer, text, safe.x + safe.w - ph_text_width(text, size, PH_TEXT_MEDIUM) - 10 * points, safe.y + 8 * points, size, PH_TEXT_MEDIUM, ph_color(0xFFFFFF, 0.8f));
    ph_overlay_end(demo.renderer, state);
}

static void draw(void)
{
    SDL_SetRenderDrawColor(demo.renderer, 0, 0, 0, 255);
    SDL_RenderClear(demo.renderer);
    if (demo.running) {
        draw_field();
        SDL_FRect view = ph_view_fit(ph_view_safe(demo.renderer), FIELD_WIDTH, FIELD_HEIGHT, 1, demo.scaling);
        ph_crt_draw(&demo.crt, demo.screen, view);
        if (!demo.launcher.open) {
            ph_touch_draw(&demo.touch, demo.renderer);
        }
    }
    ph_launcher_draw(&demo.launcher, demo.renderer);
    draw_fps();
    draw_debug();
    SDL_RenderPresent(demo.renderer);
}

static void setup(void)
{
    ph_files_init("port-helpers", "demo");
    ph_view_prefer_landscape();
    demo.window = SDL_CreateWindow("Port Helpers Demo", 960, 672, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    demo.renderer = ph_crt_create_renderer(demo.window);
    SDL_SetRenderVSync(demo.renderer, 1);
    demo.screen = SDL_CreateTexture(demo.renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_TARGET, FIELD_WIDTH, FIELD_HEIGHT);
    ph_crt_open(&demo.crt, demo.renderer);
    ph_crt_settings(&demo.crt, &settings[SETTING_CRT], 0);
    ph_input_init(&demo.input, 2, ACTION_COUNT, action_keys, action_labels);
    ph_input_defaults(&demo.input, 0, player_one_keys, pad_buttons);
    ph_input_defaults(&demo.input, 1, player_two_keys, pad_buttons);
    ph_touch_init(&demo.touch, landscape_controls, portrait_controls, SDL_arraysize(landscape_controls));
    ph_net_init(&demo.net, "PHDM", DEMO_PORT, ph_fnv32(PH_FNV32_START, "demo 1", 6), 60);
    ph_debug_init(&demo.debug);
    ph_config_load(&config, "demo.cfg");
    apply_settings();
    ph_launcher_init(&demo.launcher, demo.window);
    demo.launcher.settings = settings;
    demo.launcher.setting_count = SETTING_COUNT;
    demo.launcher.sections = section_names;
    demo.launcher.section_count = SDL_arraysize(section_names);
    demo.launcher.input = &demo.input;
    demo.launcher.touch = &demo.touch;
    demo.launcher.net = &demo.net;
    demo.launcher.items = launcher_items;
    demo.launcher.item_count = SDL_arraysize(launcher_items);
    demo.launcher.slots = 3;
    demo.launcher.rom_ready = 1;
    demo.launcher.rom_hint = "Any file works here; LAN players must pick the same one.";
    demo.launcher.rom_description = "Any file";
    demo.launcher.rom_patterns = "*";
    SDL_strlcpy(demo.launcher.rom_status, "Built in", sizeof demo.launcher.rom_status);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    setup();
    while (!demo.quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            handle_event(&event);
        }
        update();
        draw();
    }
    ph_net_stop(&demo.net);
    ph_touch_close(&demo.touch);
    ph_crt_close(&demo.crt);
    SDL_DestroyTexture(demo.screen);
    SDL_DestroyRenderer(demo.renderer);
    SDL_DestroyWindow(demo.window);
    SDL_Quit();
    return 0;
}
