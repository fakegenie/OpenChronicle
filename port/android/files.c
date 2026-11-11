#include "android_files.h"
#include "memory_card.h"
#include <SDL3/SDL.h>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>

static void (*folder_done)(const char *path);

const char *port_android_documents(void) {
    static char path[1024];
    if (path[0]) return path;
    const char *external = SDL_GetAndroidExternalStoragePath();
    snprintf(path, sizeof path, "%s/", external ? external : SDL_GetAndroidInternalStoragePath());
    return path;
}

static void run_sync_saves(void *unused) {
    (void)unused;
    JNIEnv *env = SDL_GetAndroidJNIEnv();
    jobject activity = SDL_GetAndroidActivity();
    if (!env || !activity) return;
    jclass kind = (*env)->GetObjectClass(env, activity);
    jmethodID method = (*env)->GetMethodID(env, kind, "syncSaves", "()V");
    (*env)->CallVoidMethod(env, activity, method);
    (*env)->DeleteLocalRef(env, kind);
    (*env)->DeleteLocalRef(env, activity);
}

static void sync_saves(void) {
    SDL_RunOnMainThread(run_sync_saves, NULL, false);
}

void port_android_pick_saves(int has_custom, void (*done)(const char *path)) {
    folder_done = done;
    JNIEnv *env = SDL_GetAndroidJNIEnv();
    jobject activity = SDL_GetAndroidActivity();
    if (!env || !activity) {
        if (done) done("");
        return;
    }
    jclass kind = (*env)->GetObjectClass(env, activity);
    jmethodID method = (*env)->GetMethodID(env, kind, "pickSaveFolder", "(Z)V");
    (*env)->CallVoidMethod(env, activity, method, (jboolean)(has_custom != 0));
    (*env)->DeleteLocalRef(env, kind);
    (*env)->DeleteLocalRef(env, activity);
}

JNIEXPORT void JNICALL Java_dc2toolkit_dc2_DC2_nativePickedFolder(JNIEnv *env, jclass kind, jstring uri) {
    (void)kind;
    const char *text = uri ? (*env)->GetStringUTFChars(env, uri, NULL) : NULL;
    if (folder_done) folder_done(text ? text : "");
    if (text) (*env)->ReleaseStringUTFChars(env, uri, text);
}

void port_android_saves(const char *chosen) {
    char local_saves[1100];
    snprintf(local_saves, sizeof local_saves, "%ssaves", port_android_documents());
    JNIEnv *env = SDL_GetAndroidJNIEnv();
    jobject activity = SDL_GetAndroidActivity();
    if (env && activity) {
        jclass kind = (*env)->GetObjectClass(env, activity);
        jstring fallback = (*env)->NewStringUTF(env, local_saves);
        jmethodID migrate = (*env)->GetMethodID(env, kind, "migrateLegacySaves", "(Ljava/lang/String;)V");
        (*env)->CallVoidMethod(env, activity, migrate, fallback);
        if (chosen && chosen[0]) {
            jmethodID prepare = (*env)->GetMethodID(env, kind, "prepareSaves", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
            jstring uri = (*env)->NewStringUTF(env, chosen);
            jstring prepared = (*env)->CallObjectMethod(env, activity, prepare, uri, fallback);
            const char *path = prepared ? (*env)->GetStringUTFChars(env, prepared, NULL) : NULL;
            int ready = path && path[0];
            if (ready) {
                SDL_setenv_unsafe("DC2_SAVE_ROOT", path, 1);
                port_memory_card_watch(sync_saves);
            } else {
                fprintf(stderr, "saves: cannot open %s, using %s\n", chosen, local_saves);
            }
            if (path) (*env)->ReleaseStringUTFChars(env, prepared, path);
            if (prepared) (*env)->DeleteLocalRef(env, prepared);
            (*env)->DeleteLocalRef(env, uri);
            if (ready) {
                (*env)->DeleteLocalRef(env, fallback);
                (*env)->DeleteLocalRef(env, kind);
                (*env)->DeleteLocalRef(env, activity);
                return;
            }
        }
        (*env)->DeleteLocalRef(env, fallback);
        (*env)->DeleteLocalRef(env, kind);
        (*env)->DeleteLocalRef(env, activity);
    }
    SDL_CreateDirectory(local_saves);
    SDL_setenv_unsafe("DC2_SAVE_ROOT", local_saves, 1);
}

void port_android_name(const char *path, char *name, size_t size) {
    JNIEnv *env = SDL_GetAndroidJNIEnv();
    jobject activity = SDL_GetAndroidActivity();
    jclass kind = (*env)->GetObjectClass(env, activity);
    jmethodID method = (*env)->GetMethodID(env, kind, "displayName", "(Ljava/lang/String;)Ljava/lang/String;");
    jstring uri = (*env)->NewStringUTF(env, path);
    jstring shown = (*env)->CallObjectMethod(env, activity, method, uri);
    const char *text = shown ? (*env)->GetStringUTFChars(env, shown, NULL) : NULL;
    snprintf(name, size, "%s", text ? text : "");
    if (text) (*env)->ReleaseStringUTFChars(env, shown, text);
    if (shown) (*env)->DeleteLocalRef(env, shown);
    (*env)->DeleteLocalRef(env, uri);
    (*env)->DeleteLocalRef(env, kind);
    (*env)->DeleteLocalRef(env, activity);
}
