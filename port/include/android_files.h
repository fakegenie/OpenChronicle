#pragma once

#include <stddef.h>

const char *port_android_documents(void);
void port_android_saves(const char *chosen);
void port_android_pick_saves(int has_custom, void (*done)(const char *path));
void port_android_name(const char *path, char *name, size_t size);
