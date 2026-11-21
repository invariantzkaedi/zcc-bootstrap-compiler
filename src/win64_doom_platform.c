/*
 * ZCC Native Win64 GUI + Headless Platform Layer for Upstream DOOM 1.10
 * (`src/win64_doom_platform.c`)
 * =====================================================================
 * Replaces DOOM's Linux/X11 platform modules (`i_system.c`, `i_video.c`,
 * `i_sound.c`, `i_net.c`, `i_main.c`) so all 57 unmodified core `doom/*.c`
 * modules compile cleanly into a dual-mode standalone Win64 PE32+ executable
 * using ONLY System32 DLLs (`msvcrt.dll`, `KERNEL32.dll`, `USER32.dll`, `GDI32.dll`)
 * — zero MSVC, zero MinGW, zero `<windows.h>`.
 *
 * Modes:
 *   doom.exe [--warp EP MAP] [--auto-close-frames N]
 *       Opens a scaled 960x600 Win32 window and runs a live 35 Hz GDI
 *       StretchDIBits interactive game loop with Win32 keyboard input.
 *   doom.exe --headless [--frames N] [--warp EP MAP] [--keys-mask M] [--dump-ppm PATH]
 *       Runs deterministically without opening a window and emits `DOOM_WIN64_OK ...`.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

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

/* -------------------------------------------------------------------------
 * Minimal Win64 ABI Win32 Declarations (zero <windows.h>)
 * Note: ZCC uses 64-bit `long`, so Win32 32-bit LONG/DWORD/UINT map to `int`/`unsigned int`.
 * ------------------------------------------------------------------------- */
typedef struct {
    unsigned int cbSize;
    unsigned int style;
    long long (*lpfnWndProc)(void *, unsigned int, unsigned long long, long long);
    int cbClsExtra;
    int cbWndExtra;
    void *hInstance;
    void *hIcon;
    void *hCursor;
    void *hbrBackground;
    const char *lpszMenuName;
    const char *lpszClassName;
    void *hIconSm;
} W64_WNDCLASSEXA;

typedef struct {
    void *hwnd;
    unsigned int message;
    unsigned long long wParam;
    long long lParam;
    unsigned int time;
    int pt_x;
    int pt_y;
    unsigned int lPrivate;
} W64_MSG;

typedef struct {
    int left;
    int top;
    int right;
    int bottom;
} W64_RECT;

typedef struct {
    unsigned int biSize;
    int biWidth;
    int biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned int biCompression;
    unsigned int biSizeImage;
    int biXPelsPerMeter;
    int biYPelsPerMeter;
    unsigned int biClrUsed;
    unsigned int biClrImportant;
} W64_BITMAPINFOHEADER;

typedef struct {
    W64_BITMAPINFOHEADER bmiHeader;
    unsigned int bmiColors[3];
} W64_BITMAPINFO;

/* KERNEL32.dll */
extern void *GetModuleHandleA(const char *lpModuleName);
extern unsigned long long GetTickCount64(void);
extern void Sleep(unsigned int dwMilliseconds);

/* USER32.dll */
extern unsigned short RegisterClassExA(const W64_WNDCLASSEXA *lpWndClass);
extern void *CreateWindowExA(
    unsigned int dwExStyle,
    const char *lpClassName,
    const char *lpWindowName,
    unsigned int dwStyle,
    int x,
    int y,
    int nWidth,
    int nHeight,
    void *hWndParent,
    void *hMenu,
    void *hInstance,
    void *lpParam
);
extern int AdjustWindowRect(W64_RECT *lpRect, unsigned int dwStyle, int bMenu);
extern int ShowWindow(void *hWnd, int nCmdShow);
extern int UpdateWindow(void *hWnd);
extern int DestroyWindow(void *hWnd);
extern int PeekMessageA(W64_MSG *lpMsg, void *hWnd, unsigned int wMsgFilterMin, unsigned int wMsgFilterMax, unsigned int wRemoveMsg);
extern int TranslateMessage(const W64_MSG *lpMsg);
extern long long DispatchMessageA(const W64_MSG *lpMsg);
extern long long DefWindowProcA(void *hWnd, unsigned int msg, unsigned long long wParam, long long lParam);
extern void PostQuitMessage(int nExitCode);
extern void *GetDC(void *hWnd);
extern int ReleaseDC(void *hWnd, void *hDC);
extern int GetClientRect(void *hWnd, W64_RECT *lpRect);
extern void *LoadCursorA(void *hInstance, const char *lpCursorName);
extern int SetWindowTextA(void *hWnd, const char *lpString);

/* GDI32.dll */
extern int StretchDIBits(
    void *hdc,
    int xDest,
    int yDest,
    int DestWidth,
    int DestHeight,
    int xSrc,
    int ySrc,
    int SrcWidth,
    int SrcHeight,
    const void *lpBits,
    const W64_BITMAPINFO *lpbmi,
    unsigned int iUsage,
    unsigned int rop
);
extern void *GetStockObject(int i);

int mb_used = 16;
char *sndserver_filename = "sndserver";

/* Windows GDI 32-bit BI_RGB DIB buffer (`0x00RRGGBB`) */
static uint32_t g_doom_dib[SCREENWIDTH * SCREENHEIGHT];
static uint32_t g_doom_pal_dib[256];
static int g_doom_tic = 1;
static int g_doom_in_display = 0;
static int g_doom_frames_rendered = 0;
static int g_doom_quit = 0;
static int g_doom_prev_mask = 0;

static void *g_hwnd = NULL;
static void *g_hdc = NULL;
static int g_hwnd_ok = 0;
static int g_blit_ok = 0;
static W64_BITMAPINFO g_bmi;

static ticcmd_t _win64_emptycmd;

ticcmd_t *I_BaseTiccmd(void) {
    return &_win64_emptycmd;
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
    if (count > 0) Sleep((unsigned int)(count * 14));
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
 * Video & 32-bit Top-Down GDI DIB Palette Blitting
 * ------------------------------------------------------------------------- */
void I_InitGraphics(void) {
    int i;
    for (i = 0; i < 256; i++) {
        uint32_t v = (uint32_t)i;
        g_doom_pal_dib[i] = (v << 16) | (v << 8) | v;
    }
    memset(&g_bmi, 0, sizeof(g_bmi));
    g_bmi.bmiHeader.biSize = (unsigned int)sizeof(W64_BITMAPINFOHEADER);
    g_bmi.bmiHeader.biWidth = SCREENWIDTH;
    g_bmi.bmiHeader.biHeight = -SCREENHEIGHT; /* negative height => top-down DIB */
    g_bmi.bmiHeader.biPlanes = 1;
    g_bmi.bmiHeader.biBitCount = 32;
    g_bmi.bmiHeader.biCompression = 0; /* BI_RGB */
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
        g_doom_pal_dib[i] = (r << 16) | (g << 8) | b;
        palette += 3;
    }
}

void I_FinishUpdate(void) {
    if (!screens[0]) return;
    const byte *src = screens[0];
    int total = SCREENWIDTH * SCREENHEIGHT;
    int i;
    for (i = 0; i < total; i++) {
        g_doom_dib[i] = g_doom_pal_dib[src[i]];
    }
    g_doom_frames_rendered++;
    if (g_hwnd && g_hdc) {
        W64_RECT rc;
        rc.left = 0; rc.top = 0; rc.right = SCREENWIDTH * 3; rc.bottom = SCREENHEIGHT * 3;
        GetClientRect(g_hwnd, &rc);
        int dw = rc.right - rc.left;
        int dh = rc.bottom - rc.top;
        if (dw < 1) dw = SCREENWIDTH * 3;
        if (dh < 1) dh = SCREENHEIGHT * 3;
        int lines = StretchDIBits(
            g_hdc,
            0, 0, dw, dh,
            0, 0, SCREENWIDTH, SCREENHEIGHT,
            g_doom_dib, &g_bmi,
            0U,         /* DIB_RGB_COLORS */
            0x00CC0020U /* SRCCOPY */
        );
        if (lines > 0) g_blit_ok = 1;
    }
}

void I_ReadScreen(byte *scr) {
    if (scr && screens[0]) {
        memcpy(scr, screens[0], (size_t)(SCREENWIDTH * SCREENHEIGHT));
    }
}

/* -------------------------------------------------------------------------
 * Null Audio & Single-Player Network
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
 * Engine Step Loop & Input Mapping
 * ------------------------------------------------------------------------- */
void D_DoomLoop(void) {
    /* Non-blocking: D_DoomMain() performs full WAD/renderer/playloop init. */
    I_InitGraphics();
}

static void _win64_send_doom_key(int doom_key, int is_down) {
    event_t ev;
    ev.type = is_down ? ev_keydown : ev_keyup;
    ev.data1 = doom_key;
    ev.data2 = 0;
    ev.data3 = 0;
    D_PostEvent(&ev);
}

static const int _win64_mask_keys[11] = {
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

static void _win64_apply_keys_mask(int key_mask) {
    int bit;
    for (bit = 0; bit < 11; bit++) {
        int mask = (1 << bit);
        int was_down = (g_doom_prev_mask & mask) ? 1 : 0;
        int now_down = (key_mask & mask) ? 1 : 0;
        if (was_down != now_down) {
            _win64_send_doom_key(_win64_mask_keys[bit], now_down);
        }
    }
    g_doom_prev_mask = key_mask;
}

static void D_DoomWin64StepOneTic(void) {
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

static int _win64_vk_to_doom_key(unsigned long long vk) {
    switch (vk) {
        case 0x26: case 'W': return KEY_UPARROW;
        case 0x28: case 'S': return KEY_DOWNARROW;
        case 0x25:           return KEY_LEFTARROW;
        case 0x27:           return KEY_RIGHTARROW;
        case 'A':  case 0xBC: return ',';
        case 'D':  case 0xBE: return '.';
        case 0x11:           return KEY_RCTRL;
        case 0x10:           return KEY_RSHIFT;
        case 0x12:           return KEY_RALT;
        case 0x20: case 'E': return ' ';
        case 0x0D:           return KEY_ENTER;
        case 0x1B:           return KEY_ESCAPE;
        case 0x09:           return KEY_TAB;
        case 0x08:           return KEY_BACKSPACE;
        case 0xBB:           return KEY_EQUALS;
        case 0xBD:           return KEY_MINUS;
        case '1': case '2': case '3': case '4': case '5': case '6': case '7':
            return (int)vk;
        case 'Y':            return 'y';
        case 'N':            return 'n';
        default:             return 0;
    }
}

static long long _win64_doom_wndproc(void *hwnd, unsigned int msg, unsigned long long wParam, long long lParam) {
    if (msg == 0x0010U /* WM_CLOSE */ || msg == 0x0002U /* WM_DESTROY */) {
        g_doom_quit = 1;
        PostQuitMessage(0);
        return 0;
    }
    if (msg == 0x0012U /* WM_QUIT */) {
        g_doom_quit = 1;
        return 0;
    }
    if (msg == 0x0100U /* WM_KEYDOWN */ || msg == 0x0104U /* WM_SYSKEYDOWN */) {
        if ((lParam & (1LL << 30)) == 0) {
            int dk = _win64_vk_to_doom_key(wParam);
            if (dk != 0) _win64_send_doom_key(dk, 1);
        }
        return 0;
    }
    if (msg == 0x0101U /* WM_KEYUP */ || msg == 0x0105U /* WM_SYSKEYUP */) {
        int dk = _win64_vk_to_doom_key(wParam);
        if (dk != 0) _win64_send_doom_key(dk, 0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static int count_unique_colors(const uint32_t *dib, int count) {
    uint32_t seen[512];
    int used = 0;
    int i;
    memset(seen, 0, sizeof(seen));
    for (i = 0; i < count; i++) {
        uint32_t c = dib[i] & 0x00FFFFFFU;
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

static uint32_t fnv1a_dib_rgb(const uint32_t *dib, int count) {
    uint32_t h = 2166136261U;
    int i;
    for (i = 0; i < count; i++) {
        uint32_t px = dib[i];
        uint32_t r = (px >> 16) & 0xFFU;
        uint32_t g = (px >> 8) & 0xFFU;
        uint32_t b = px & 0xFFU;
        h ^= r; h *= 16777619U;
        h ^= g; h *= 16777619U;
        h ^= b; h *= 16777619U;
    }
    return h;
}

static int write_ppm_rgb_from_dib(const char *path, const uint32_t *dib, int w, int h) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    fprintf(fp, "P6\n%d %d\n255\n", w, h);
    uint8_t row[SCREENWIDTH * 3];
    int y, x;
    for (y = 0; y < h; y++) {
        const uint32_t *src = dib + y * w;
        for (x = 0; x < w; x++) {
            uint32_t px = src[x];
            row[x * 3 + 0] = (uint8_t)((px >> 16) & 0xFFU);
            row[x * 3 + 1] = (uint8_t)((px >> 8) & 0xFFU);
            row[x * 3 + 2] = (uint8_t)(px & 0xFFU);
        }
        fwrite(row, 1U, (size_t)(w * 3), fp);
    }
    fclose(fp);
    return 0;
}

int main(int argc, char **argv) {
    int headless = 0;
    int frames = 10;
    int auto_close_frames = 0;
    int keys_mask = 0;
    const char *dump_ppm = NULL;
    static char *clean_argv[64];
    int clean_argc = 0;
    int i;

    clean_argv[clean_argc++] = (argc > 0 && argv && argv[0]) ? argv[0] : "doom.exe";
    for (i = 1; i < argc && clean_argc < 63; i++) {
        if (strcmp(argv[i], "--headless") == 0) {
            headless = 1;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = atoi(argv[++i]);
            if (frames < 1) frames = 1;
            if (frames > 5000) frames = 5000;
        } else if (strcmp(argv[i], "--auto-close-frames") == 0 && i + 1 < argc) {
            auto_close_frames = atoi(argv[++i]);
            if (auto_close_frames < 1) auto_close_frames = 1;
        } else if (strcmp(argv[i], "--keys-mask") == 0 && i + 1 < argc) {
            keys_mask = atoi(argv[++i]);
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

    if (headless) {
        if (keys_mask != 0) {
            _win64_apply_keys_mask(keys_mask);
        }
        for (i = 0; i < frames && !g_doom_quit; i++) {
            D_DoomWin64StepOneTic();
        }
        if (dump_ppm) {
            if (write_ppm_rgb_from_dib(dump_ppm, g_doom_dib, SCREENWIDTH, SCREENHEIGHT) != 0) {
                fprintf(stderr, "doom.exe: failed to dump PPM to %s\n", dump_ppm);
                return 1;
            }
        }
        int uniq = count_unique_colors(g_doom_dib, SCREENWIDTH * SCREENHEIGHT);
        int px = players[consoleplayer].mo ? (int)players[consoleplayer].mo->x : 0;
        int py = players[consoleplayer].mo ? (int)players[consoleplayer].mo->y : 0;
        unsigned int ang = players[consoleplayer].mo ? (unsigned int)players[consoleplayer].mo->angle : 0U;
        uint32_t fnv = fnv1a_dib_rgb(g_doom_dib, SCREENWIDTH * SCREENHEIGHT);
        printf(
            "DOOM_WIN64_OK frames=%d gamestate=%d gametic=%d unique_colors=%d px=%d py=%d angle=%u fnv=%08x\n",
            g_doom_frames_rendered, (int)gamestate, gametic, uniq, px, py, ang, fnv
        );
        fflush(stdout);
        return 0;
    }

    /* Interactive Win32 GDI GUI loop */
    void *hinst = GetModuleHandleA(NULL);
    W64_WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = (unsigned int)sizeof(W64_WNDCLASSEXA);
    wc.style = 0x0023U; /* CS_VREDRAW | CS_HREDRAW | CS_OWNDC */
    wc.lpfnWndProc = _win64_doom_wndproc;
    wc.hInstance = hinst;
    wc.hCursor = LoadCursorA(NULL, (const char *)(uintptr_t)32512); /* IDC_ARROW */
    wc.hbrBackground = GetStockObject(4); /* BLACK_BRUSH */
    wc.lpszClassName = "ZCC_DOOM_W64";
    RegisterClassExA(&wc);

    unsigned int style = 0x10CF0000U; /* WS_OVERLAPPEDWINDOW | WS_VISIBLE */
    W64_RECT rc;
    rc.left = 0; rc.top = 0; rc.right = SCREENWIDTH * 3; rc.bottom = SCREENHEIGHT * 3;
    AdjustWindowRect(&rc, style, 0);

    g_hwnd = CreateWindowExA(
        0U,
        "ZCC_DOOM_W64",
        "DOOM 1.10 (ZCC Win64 PE32+ Sovereign)",
        style,
        (int)0x80000000U, /* CW_USEDEFAULT */
        (int)0x80000000U,
        rc.right - rc.left,
        rc.bottom - rc.top,
        NULL,
        NULL,
        hinst,
        NULL
    );
    if (g_hwnd) {
        g_hwnd_ok = 1;
        ShowWindow(g_hwnd, 1 /* SW_SHOWNORMAL */);
        UpdateWindow(g_hwnd);
        g_hdc = GetDC(g_hwnd);
    }

    unsigned long long last_tic_ms = GetTickCount64();
    while (!g_doom_quit) {
        W64_MSG msg;
        while (PeekMessageA(&msg, NULL, 0U, 0U, 1U /* PM_REMOVE */)) {
            if (msg.message == 0x0012U /* WM_QUIT */) {
                g_doom_quit = 1;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (g_doom_quit) break;

        unsigned long long now_ms = GetTickCount64();
        if (auto_close_frames > 0 || (now_ms - last_tic_ms) >= 28ULL) {
            if (keys_mask != 0) _win64_apply_keys_mask(keys_mask);
            D_DoomWin64StepOneTic();
            last_tic_ms = now_ms;
            if (auto_close_frames > 0 && g_doom_frames_rendered >= auto_close_frames) {
                break;
            }
        }
        if (auto_close_frames <= 0) {
            Sleep(1U);
        }
    }

    if (dump_ppm) {
        write_ppm_rgb_from_dib(dump_ppm, g_doom_dib, SCREENWIDTH, SCREENHEIGHT);
    }

    int uniq = count_unique_colors(g_doom_dib, SCREENWIDTH * SCREENHEIGHT);
    if (g_hwnd && g_hdc) ReleaseDC(g_hwnd, g_hdc);
    if (g_hwnd) DestroyWindow(g_hwnd);
    g_hdc = NULL;
    g_hwnd = NULL;

    printf(
        "DOOM_WIN64_GUI_OK hwnd_ok=%d blit_ok=%d frames=%d gamestate=%d gametic=%d unique_colors=%d\n",
        g_hwnd_ok, g_blit_ok, g_doom_frames_rendered, (int)gamestate, gametic, uniq
    );
    fflush(stdout);
    return (g_hwnd_ok && g_blit_ok) ? 0 : 1;
}
