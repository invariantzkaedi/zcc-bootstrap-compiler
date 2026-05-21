#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "i_system.h"
#include "d_event.h"
#include "d_net.h"
#include "m_argv.h"
#include "doomstat.h"
#include "i_net.h"

void I_InitNetwork (void) {
    doomcom = malloc(sizeof(*doomcom));
    memset(doomcom, 0, sizeof(*doomcom));
    doomcom->id = DOOMCOM_ID;
    doomcom->numplayers = doomcom->numnodes = 1;
    doomcom->deathmatch = false;
    doomcom->consoleplayer = 0;
    netgame = false;
}
void I_NetCmd (void) {}
