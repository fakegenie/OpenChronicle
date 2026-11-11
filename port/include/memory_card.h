#pragma once

#include "sce/libmc.h"

#define PORT_MC_BUSY (-200)

#ifdef __cplusplus
extern "C" {
#endif

int port_memory_card_init(const char *root);
const char *port_memory_card_default_root(void);
const char *port_memory_card_root(void);
void port_memory_card_choose_root(const char *chosen);
void port_memory_card_watch(void (*changed)(void));
void port_memory_card_changed(void);

#ifdef __cplusplus
}
#endif
