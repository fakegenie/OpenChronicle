#ifndef PH_FILES_H
#define PH_FILES_H

#include <SDL3/SDL.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PH_PATH_SIZE = 1024,
};

enum {
    PH_FILES_NOTHING,
    PH_FILES_PICKED,
    PH_FILES_LOOK_AGAIN,
};

struct ph_search {
    const char *const *names;
    int name_count;
    const char *const *paths;
    int path_count;
    int step;
};

void ph_files_init(const char *organization, const char *application);
int ph_user_path(const char *name, char *path, int size);
int ph_in_user_folder(const char *path);
int ph_readable(const char *path);
const char *ph_file_name(const char *path);
int ph_search_next(struct ph_search *search, char *path, int size);

void ph_pick_file(SDL_Window *window, const char *description, const char *patterns);
int ph_picking(void);
int ph_take_pick(char *path, int size);
void ph_file_picked(const char *path);
int ph_files_event(const SDL_Event *event);

int ph_copy_file(const char *source, const char *destination);
int ph_import(const void *data, size_t size, const char *name);
int ph_import_file(const char *source, const char *name);
int ph_remove_import(const char *name);

int ph_slot_path(int slot, char *path, int size);
int64_t ph_file_time(const char *path);
void ph_time_text(int64_t seconds, char *text, int size);

#if defined(SDL_PLATFORM_IOS)
int ph_ios_pick_file(SDL_Window *window);
#endif

#ifdef __cplusplus
}
#endif

#endif

#if defined(PH_IMPLEMENTATION) && !defined(PH_FILES_IMPLEMENTED)
#define PH_FILES_IMPLEMENTED

enum {
    PH_PICK_IDLE,
    PH_PICK_OPEN,
    PH_PICK_DONE,
    PH_SEARCH_FOLDERS = 4,
    PH_FILTER_TEXT = 128,
};

static char ph_user_folder[PH_PATH_SIZE];
static char ph_picked_path[PH_PATH_SIZE];
static char ph_filter_description[PH_FILTER_TEXT];
static char ph_filter_patterns[PH_FILTER_TEXT];
static SDL_AtomicInt ph_pick_state;

void ph_files_init(const char *organization, const char *application)
{
    char *folder = SDL_GetPrefPath(organization, application);
    SDL_strlcpy(ph_user_folder, folder ? folder : "", sizeof ph_user_folder);
    SDL_free(folder);
}

int ph_user_path(const char *name, char *path, int size)
{
    return ph_user_folder[0] && SDL_snprintf(path, (size_t)size, "%s%s", ph_user_folder, name) < size;
}

int ph_in_user_folder(const char *path)
{
    size_t length = SDL_strlen(ph_user_folder);
    return length && path && !SDL_strncmp(path, ph_user_folder, length);
}

int ph_readable(const char *path)
{
    SDL_PathInfo info;
    if (!path || !*path) {
        return 0;
    }
    if (SDL_GetPathInfo(path, &info)) {
        return info.type == SDL_PATHTYPE_FILE;
    }
    if (!SDL_strncmp(path, "content://", 10)) {
        SDL_IOStream *stream = SDL_IOFromFile(path, "rb");
        if (stream) {
            SDL_CloseIO(stream);
            return 1;
        }
    }
    return 0;
}

const char *ph_file_name(const char *path)
{
    if (!path) {
        return "";
    }
    const char *slash = SDL_strrchr(path, '/');
    const char *backslash = SDL_strrchr(path, '\\');
    const char *name = slash && backslash ? (slash > backslash ? slash : backslash) : (slash ? slash : backslash);
    const char *base = name ? name + 1 : path;
    if (!SDL_strncmp(path, "content://", 10)) {
        for (const char *cursor = base; cursor[0] && cursor[1] && cursor[2]; cursor++) {
            int encoded_sep = cursor[0] == '%' &&
                              ((cursor[1] == '2' && (cursor[2] == 'F' || cursor[2] == 'f')) ||
                               (cursor[1] == '3' && (cursor[2] == 'A' || cursor[2] == 'a')));
            if (encoded_sep && cursor[3]) {
                base = cursor + 3;
                cursor += 2;
            }
        }
    }
    return base;
}

static const char *ph_search_folder(int folder)
{
    if (folder == 0) {
        return ph_user_folder[0] ? ph_user_folder : NULL;
    }
    if (folder == 1) {
        return SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS);
    }
    if (folder == 2) {
        return SDL_GetBasePath();
    }
    return "";
}

int ph_search_next(struct ph_search *search, char *path, int size)
{
    int total = search->path_count + PH_SEARCH_FOLDERS * search->name_count;
    while (search->step < total) {
        int step = search->step++;
        if (step < search->path_count) {
            const char *candidate = search->paths[step];
            if (ph_readable(candidate) && SDL_strlcpy(path, candidate, (size_t)size) < (size_t)size) {
                return 1;
            }
            continue;
        }
        step -= search->path_count;
        const char *folder = ph_search_folder(step / search->name_count);
        const char *name = search->names[step % search->name_count];
        if (folder && SDL_snprintf(path, (size_t)size, "%s%s", folder, name) < size && ph_readable(path)) {
            return 1;
        }
    }
    return 0;
}

void ph_file_picked(const char *path)
{
    if (path && *path) {
        SDL_strlcpy(ph_picked_path, path, sizeof ph_picked_path);
        SDL_SetAtomicInt(&ph_pick_state, PH_PICK_DONE);
    } else {
        SDL_SetAtomicInt(&ph_pick_state, PH_PICK_IDLE);
    }
}

#if !defined(SDL_PLATFORM_IOS)
static void SDLCALL ph_dialog_finished(void *userdata, const char *const *files, int filter)
{
    (void)userdata;
    (void)filter;
    ph_file_picked(files ? files[0] : NULL);
}
#endif

void ph_pick_file(SDL_Window *window, const char *description, const char *patterns)
{
    if (SDL_GetAtomicInt(&ph_pick_state) == PH_PICK_OPEN) {
        return;
    }
    SDL_SetAtomicInt(&ph_pick_state, PH_PICK_OPEN);
    SDL_strlcpy(ph_filter_description, description ? description : "Game files", sizeof ph_filter_description);
    SDL_strlcpy(ph_filter_patterns, patterns ? patterns : "*", sizeof ph_filter_patterns);
    static SDL_DialogFileFilter filters[2];
    filters[0] = (SDL_DialogFileFilter){ ph_filter_description, ph_filter_patterns };
    filters[1] = (SDL_DialogFileFilter){ "All files", "*" };
#if defined(SDL_PLATFORM_IOS)
    if (!ph_ios_pick_file(window)) {
        SDL_SetAtomicInt(&ph_pick_state, PH_PICK_IDLE);
    }
#elif defined(SDL_PLATFORM_ANDROID)
    SDL_ShowOpenFileDialog(ph_dialog_finished, NULL, window, NULL, 0, NULL, false);
#else
    SDL_ShowOpenFileDialog(ph_dialog_finished, NULL, window, filters, 2, NULL, false);
#endif
}

int ph_picking(void)
{
    return SDL_GetAtomicInt(&ph_pick_state) == PH_PICK_OPEN;
}

int ph_take_pick(char *path, int size)
{
    if (SDL_GetAtomicInt(&ph_pick_state) != PH_PICK_DONE) {
        return 0;
    }
    SDL_strlcpy(path, ph_picked_path, (size_t)size);
    SDL_SetAtomicInt(&ph_pick_state, PH_PICK_IDLE);
    return 1;
}

int ph_files_event(const SDL_Event *event)
{
    if (event->type == SDL_EVENT_DROP_FILE && event->drop.data) {
        ph_file_picked(event->drop.data);
        return PH_FILES_PICKED;
    }
#if defined(SDL_PLATFORM_IOS) || defined(SDL_PLATFORM_ANDROID)
    if (event->type == SDL_EVENT_DID_ENTER_FOREGROUND) {
        return PH_FILES_LOOK_AGAIN;
    }
#endif
    return PH_FILES_NOTHING;
}

int ph_copy_file(const char *source, const char *destination)
{
    if (!source || !*source || !destination || !*destination) {
        return 0;
    }
    SDL_IOStream *in = SDL_IOFromFile(source, "rb");
    if (!in) {
        return 0;
    }
    SDL_IOStream *out = SDL_IOFromFile(destination, "wb");
    if (!out) {
        SDL_CloseIO(in);
        return 0;
    }
    uint8_t chunk[65536];
    int ok = 1;
    for (;;) {
        size_t got = SDL_ReadIO(in, chunk, sizeof chunk);
        if (got > 0 && SDL_WriteIO(out, chunk, got) != got) {
            ok = 0;
            break;
        }
        if (got < sizeof chunk) {
            if (SDL_GetIOStatus(in) != SDL_IO_STATUS_EOF) {
                ok = 0;
            }
            break;
        }
    }
    if (ok && !SDL_FlushIO(out)) {
        ok = 0;
    }
    SDL_CloseIO(in);
    if (!SDL_CloseIO(out)) {
        ok = 0;
    }
    if (!ok) {
        SDL_RemovePath(destination);
    }
    return ok;
}

int ph_import(const void *data, size_t size, const char *name)
{
    char path[PH_PATH_SIZE];
    return ph_user_path(name, path, sizeof path) && SDL_SaveFile(path, data, size);
}

int ph_import_file(const char *source, const char *name)
{
    char path[PH_PATH_SIZE];
    return ph_user_path(name, path, sizeof path) && ph_copy_file(source, path);
}

int ph_remove_import(const char *name)
{
    char path[PH_PATH_SIZE];
    return ph_user_path(name, path, sizeof path) && SDL_RemovePath(path);
}

int ph_slot_path(int slot, char *path, int size)
{
    char name[32];
    SDL_snprintf(name, sizeof name, "state%d.bin", slot);
    return ph_user_path(name, path, size);
}

int64_t ph_file_time(const char *path)
{
    SDL_PathInfo info;
    if (!path || !SDL_GetPathInfo(path, &info) || info.type != SDL_PATHTYPE_FILE) {
        return 0;
    }
    return (int64_t)(info.modify_time / SDL_NS_PER_SECOND);
}

void ph_time_text(int64_t seconds, char *text, int size)
{
    SDL_DateTime moment;
    if (seconds <= 0 || !SDL_TimeToDateTime((SDL_Time)seconds * SDL_NS_PER_SECOND, &moment, true)) {
        SDL_strlcpy(text, "Empty", (size_t)size);
        return;
    }
    SDL_snprintf(text, (size_t)size, "%04d-%02d-%02d %02d:%02d", moment.year, moment.month, moment.day, moment.hour, moment.minute);
}

#endif
