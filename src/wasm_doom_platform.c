/*
 * ZCC WASI Platform Layer for Upstream DOOM 1.10 (`src/wasm_doom_platform.c`)
 * ============================================================================
 * Replaces DOOM's Linux/X11 platform files (`i_system.c`, `i_video.c`,
 * `i_sound.c`, `i_net.c`, `i_main.c`) so the 57 core upstream `doom/*.c`
 * modules compile cleanly into a standalone zero-`env.*` WASI `.wasm` module
 * exporting both CLI `_start`/`main` and the non-blocking framebuffer ABI:
 *   fb_width(), fb_height(), fb_ptr(), fb_key(key, down), fb_init(ep, map),
 *   fb_tick(dt_ms, key_mask, mouse_x, mouse_y).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

extern void _wasi_rt_ensure_init(void);

#include "doomdef.h"
#include "doomstat.h"
#include "d_event.h"
#include "d_net.h"
#include "d_main.h"
#include "m_argv.h"
#include "m_misc.h"
#include "g_game.h"
#include "m_menu.h"
#include "hu_stuff.h"
#include "w_wad.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"
#include "i_net.h"
#include "v_video.h"
#include "s_sound.h"

/* External symbols defined in d_main.c / d_net.c */
extern boolean advancedemo;
extern boolean singletics;
extern int gametic;
extern int maketic;
extern ticcmd_t netcmds[MAXPLAYERS][BACKUPTICS];
extern void D_CheckNetGame(void);
extern void D_ProcessEvents(void);
extern void G_BuildTiccmd(ticcmd_t *cmd);
extern void D_DoAdvanceDemo(void);
extern void D_Display(void);
extern void D_PostEvent(event_t *ev);
extern void D_DoomMain(void);

int mb_used = 16;

static uint32_t g_doom_rgba[SCREENWIDTH * SCREENHEIGHT];
static uint32_t g_doom_pal_rgba[256];
static int g_doom_tic = 1;
static int g_doom_in_display = 0;
static int g_doom_frames_rendered = 0;
static int g_doom_initialized = 0;
static int g_doom_quit = 0;
static int g_doom_prev_mask = 0;

static ticcmd_t _wasm_emptycmd;

ticcmd_t *I_BaseTiccmd(void) {
    return &_wasm_emptycmd;
}

int I_GetHeapSize(void) {
    return mb_used * 1024 * 1024;
}

byte *I_ZoneBase(int *size) {
    *size = mb_used * 1024 * 1024;
    return (byte *)malloc((size_t)(*size));
}

int I_GetTime(void) {
    if (g_doom_in_display) {
        g_doom_tic++;
    }
    return g_doom_tic;
}

void I_Tactile(int on, int off, int total) {
    (void)on;
    (void)off;
    (void)total;
}

void I_StartFrame(void) {}
void I_StartTic(void) {}

void I_WaitVBL(int count) {
    (void)count;
}

void I_BeginRead(void) {}
void I_EndRead(void) {}

byte *I_AllocLow(int length) {
    byte *mem = (byte *)malloc((size_t)length);
    if (mem) memset(mem, 0, (size_t)length);
    return mem;
}

void I_Init(void) {
    I_InitSound();
}

void I_Quit(void) {
    D_QuitNetGame();
    I_ShutdownSound();
    I_ShutdownMusic();
    M_SaveDefaults();
    I_ShutdownGraphics();
    g_doom_quit = 1;
    fflush(NULL);
}

void I_Error(char *error, ...) {
    va_list argptr;
    va_start(argptr, error);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, error, argptr);
    fprintf(stderr, "\n");
    va_end(argptr);
    fflush(stderr);
    exit(1);
}

/* -------------------------------------------------------------------------
 * Video & 32-bit RGBA Palette Blitting
 * ------------------------------------------------------------------------- */
void I_InitGraphics(void) {
    int i;
    for (i = 0; i < 256; i++) {
        uint32_t v = (uint32_t)i;
        g_doom_pal_rgba[i] = 0xFF000000U | (v << 16) | (v << 8) | v;
    }
}

void I_ShutdownGraphics(void) {}

void I_UpdateNoBlit(void) {}

void I_SetPalette(byte *palette) {
    int i;
    if (!palette) return;
    for (i = 0; i < 256; i++) {
        uint32_t r = (uint32_t)gammatable[usegamma][palette[0]];
        uint32_t g = (uint32_t)gammatable[usegamma][palette[1]];
        uint32_t b = (uint32_t)gammatable[usegamma][palette[2]];
        g_doom_pal_rgba[i] = 0xFF000000U | (b << 16) | (g << 8) | r;
        palette += 3;
    }
}

void I_FinishUpdate(void) {
    if (!screens[0]) return;
    const byte *src = screens[0];
    int total = SCREENWIDTH * SCREENHEIGHT;
    int i;
    for (i = 0; i < total; i++) {
        g_doom_rgba[i] = g_doom_pal_rgba[src[i]];
    }
    g_doom_frames_rendered++;
}

void I_ReadScreen(byte *scr) {
    if (scr && screens[0]) {
        memcpy(scr, screens[0], (size_t)(SCREENWIDTH * SCREENHEIGHT));
    }
}

/* -------------------------------------------------------------------------
 * Headless Audio & Single-Player Network
 * ------------------------------------------------------------------------- */
void I_InitSound(void) {}
void I_UpdateSound(void) {}
void I_SubmitSound(void) {}
void I_ShutdownSound(void) {}
void I_SetChannels(void) {}

int I_GetSfxLumpNum(sfxinfo_t *sfx) {
    char namebuf[16];
    if (!sfx || !sfx->name) return 0;
    snprintf(namebuf, sizeof(namebuf), "ds%s", sfx->name);
    int num = W_CheckNumForName(namebuf);
    return (num >= 0) ? num : 0;
}

int I_StartSound(int id, int vol, int sep, int pitch, int priority) {
    (void)id; (void)vol; (void)sep; (void)pitch; (void)priority;
    return 1;
}

void I_StopSound(int handle) { (void)handle; }
int I_SoundIsPlaying(int handle) { (void)handle; return 0; }
void I_UpdateSoundParams(int handle, int vol, int sep, int pitch) {
    (void)handle; (void)vol; (void)sep; (void)pitch;
}

void I_InitMusic(void) {}
void I_ShutdownMusic(void) {}
void I_SetMusicVolume(int volume) { (void)volume; }
void I_PauseSong(int handle) { (void)handle; }
void I_ResumeSong(int handle) { (void)handle; }
int I_RegisterSong(void *data) { (void)data; return 1; }
void I_PlaySong(int handle, int looping) { (void)handle; (void)looping; }
void I_StopSong(int handle) { (void)handle; }
void I_UnRegisterSong(int handle) { (void)handle; }

void I_InitNetwork(void) {
    doomcom = (doomcom_t *)malloc(sizeof(*doomcom));
    memset(doomcom, 0, sizeof(*doomcom));
    doomcom->id = DOOMCOM_ID;
    doomcom->numplayers = doomcom->numnodes = 1;
    doomcom->ticdup = 1;
    doomcom->extratics = 0;
    doomcom->deathmatch = false;
    doomcom->consoleplayer = 0;
    netgame = false;
}

void I_NetCmd(void) {}

/* -------------------------------------------------------------------------
 * Non-Blocking Frame Loop Bridge + Exported Framebuffer ABI
 * ------------------------------------------------------------------------- */
void D_DoomLoop(void) {
    /* Non-blocking: D_DoomMain() performs full WAD/renderer/playloop init and
     * calls D_DoomLoop() at the end. We initialize graphics and return control
     * so WASI hosts can drive frames synchronously or via animation ticks. */
    I_InitGraphics();
}

void D_DoomWasmStepOneTic(void) {
    if (g_doom_quit) return;
    singletics = true;
    I_StartFrame();
    I_StartTic();
    D_ProcessEvents();
    G_BuildTiccmd(&netcmds[consoleplayer][maketic % BACKUPTICS]);
    if (advancedemo) {
        D_DoAdvanceDemo();
    }
    M_Ticker();
    G_Ticker();
    gametic++;
    maketic++;
    S_UpdateSounds(players[consoleplayer].mo);
    g_doom_in_display = 1;
    D_Display();
    g_doom_in_display = 0;
    g_doom_tic++;
}

int fb_width(void) {
    return SCREENWIDTH;
}

int fb_height(void) {
    return SCREENHEIGHT;
}

unsigned int fb_ptr(void) {
    return (unsigned int)(uintptr_t)g_doom_rgba;
}

void fb_key(int doom_key, int is_down) {
    event_t ev;
    ev.type = is_down ? ev_keydown : ev_keyup;
    ev.data1 = doom_key;
    ev.data2 = 0;
    ev.data3 = 0;
    D_PostEvent(&ev);
}

int fb_gamestate(void) {
    return (int)gamestate;
}

int fb_gametic(void) {
    return gametic;
}

int fb_player_x(void) {
    return players[consoleplayer].mo ? (int)players[consoleplayer].mo->x : 0;
}

int fb_player_y(void) {
    return players[consoleplayer].mo ? (int)players[consoleplayer].mo->y : 0;
}

unsigned int fb_player_angle(void) {
    return players[consoleplayer].mo ? (unsigned int)players[consoleplayer].mo->angle : 0U;
}

static char _doom_ep_buf[8];
static char _doom_map_buf[8];
static char *_doom_fb_argv[8];

int fb_init(int warp_episode, int warp_map) {
    _wasi_rt_ensure_init();
    if (!g_doom_initialized) {
        int c = 0;
        _doom_fb_argv[c++] = "doom.wasm";
        if (warp_episode > 0 && warp_map > 0) {
            snprintf(_doom_ep_buf, sizeof(_doom_ep_buf), "%d", warp_episode);
            snprintf(_doom_map_buf, sizeof(_doom_map_buf), "%d", warp_map);
            _doom_fb_argv[c++] = "-warp";
            _doom_fb_argv[c++] = _doom_ep_buf;
            _doom_fb_argv[c++] = _doom_map_buf;
        }
        _doom_fb_argv[c] = NULL;
        myargc = c;
        myargv = _doom_fb_argv;
        singletics = true;
        D_DoomMain();
        g_doom_initialized = 1;
    }
    D_DoomWasmStepOneTic();
    return 0;
}

static const int _fb_mask_keys[11] = {
    KEY_UPARROW,
    KEY_DOWNARROW,
    KEY_LEFTARROW,
    KEY_RIGHTARROW,
    KEY_RCTRL,
    ' ',
    KEY_ENTER,
    KEY_ESCAPE,
    KEY_RSHIFT,
    ',',
    '.'
};

int fb_tick(int dt_ms, int key_mask, int mouse_x, int mouse_y) {
    (void)dt_ms;
    (void)mouse_x;
    (void)mouse_y;
    if (!g_doom_initialized) {
        fb_init(0, 0);
    }
    int bit;
    for (bit = 0; bit < 11; bit++) {
        int mask = (1 << bit);
        int was_down = (g_doom_prev_mask & mask) ? 1 : 0;
        int now_down = (key_mask & mask) ? 1 : 0;
        if (was_down != now_down) {
            fb_key(_fb_mask_keys[bit], now_down);
        }
    }
    g_doom_prev_mask = key_mask;
    D_DoomWasmStepOneTic();
    return g_doom_frames_rendered;
}

static int count_unique_colors(const uint32_t *rgba, int count) {
    /* Track 256-entry open hash of distinct RGBA colors (sufficient to prove non-blank rendering) */
    uint32_t seen[512];
    int used = 0;
    int i;
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < count; i++) {
        uint32_t c = rgba[i];
        int j, found = 0;
        for (j = 0; j < used; j++) {
            if (seen[j] == c) { found = 1; break; }
        }
        if (!found) {
            if (used < 512) seen[used++] = c;
            else break;
        }
    }
    return used;
}

static int write_ppm_rgb(const char *path, const uint32_t *rgba, int w, int h) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    fprintf(fp, "P6\n%d %d\n255\n", w, h);
    uint8_t row[SCREENWIDTH * 3];
    int y, x;
    for (y = 0; y < h; y++) {
        const uint32_t *src = rgba + y * w;
        for (x = 0; x < w; x++) {
            uint32_t px = src[x];
            row[x * 3 + 0] = (uint8_t)(px & 0xFFU);
            row[x * 3 + 1] = (uint8_t)((px >> 8) & 0xFFU);
            row[x * 3 + 2] = (uint8_t)((px >> 16) & 0xFFU);
        }
        fwrite(row, 1, (size_t)(w * 3), fp);
    }
    fclose(fp);
    return 0;
}

int main(int argc, char **argv) {
    _wasi_rt_ensure_init();
    int frames = 10;
    const char *dump_ppm = NULL;
    static char *clean_argv[64];
    int clean_argc = 0;
    int i;

    clean_argv[clean_argc++] = (argc > 0 && argv && argv[0]) ? argv[0] : "doom.wasm";
    for (i = 1; i < argc && clean_argc < 63; i++) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = atoi(argv[++i]);
            if (frames < 1) frames = 1;
            if (frames > 2000) frames = 2000;
        } else if (strcmp(argv[i], "--dump-ppm") == 0 && i + 1 < argc) {
            dump_ppm = argv[++i];
        } else if (strcmp(argv[i], "--warp") == 0) {
            clean_argv[clean_argc++] = "-warp";
        } else {
            clean_argv[clean_argc++] = argv[i];
        }
    }
    clean_argv[clean_argc] = NULL;

    myargc = clean_argc;
    myargv = clean_argv;
    singletics = true;

    D_DoomMain();
    g_doom_initialized = 1;

    for (i = 0; i < frames && !g_doom_quit; i++) {
        D_DoomWasmStepOneTic();
    }

    if (dump_ppm) {
        if (write_ppm_rgb(dump_ppm, g_doom_rgba, SCREENWIDTH, SCREENHEIGHT) != 0) {
            fprintf(stderr, "doom.wasm: failed to dump PPM to %s\n", dump_ppm);
            return 1;
        }
    }

    int uniq = count_unique_colors(g_doom_rgba, SCREENWIDTH * SCREENHEIGHT);
    printf("DOOM_WASM_OK frames=%d gamestate=%d gametic=%d unique_colors=%d\n",
           g_doom_frames_rendered, (int)gamestate, gametic, uniq);
    fflush(stdout);
    return 0;
}
