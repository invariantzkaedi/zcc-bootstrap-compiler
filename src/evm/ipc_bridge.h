#ifndef ZCC_IPC_BRIDGE_H
#define ZCC_IPC_BRIDGE_H

int ipc_bridge_init(void);
int ipc_bridge_stream(const char *payload);
#include <stdio.h>
FILE *ipc_bridge_open_tx(void);
void ipc_bridge_close_tx(FILE *f);
char *ipc_bridge_receive(void);
void ipc_bridge_compile_and_strike(char *payload);

#endif
