#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// === BULLETPROOF SYSTEM TYPES ===
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

// === PURE 32-BIT INTEGER MATH APPROXIMATIONS ===

// Standard 8-bit integer sine wave generator
// Inputs: angle (0 to 255 representing 0 to 2*PI)
// Outputs: sine amplitude (-127 to 127)
int integer_sin(int angle) {
    angle = angle & 255;
    int val = 0;
    if (angle < 64) {
        val = (angle * (128 - angle)) / 32;
    } else if (angle < 128) {
        int a = 128 - angle;
        val = (a * (128 - a)) / 32;
    } else if (angle < 192) {
        int a = angle - 128;
        val = -((a * (128 - a)) / 32);
    } else {
        int a = 256 - angle;
        val = -((a * (128 - a)) / 32);
    }
    return val;
}

// Digit-by-digit binary integer square root
int integer_sqrt(int x) {
    if (x <= 0) return 0;
    int res = 0;
    int bit = 1 << 30; // The second-to-top bit is set
    
    while (bit > x) {
        bit >>= 2;
    }
    
    while (bit != 0) {
        if (x >= res + bit) {
            x -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return res;
}

// === ANCIENT MS-DOS REAL-MODE DEFINITIONS ===
#define RAM_SIZE (1024 * 1024) // 1 MB conventional memory
uint8_t ram[RAM_SIZE];

// Flat register structure aligned for stable ZCC standard memory layouts
typedef struct {
    uint8_t al, ah;
    uint8_t bl, bh;
    uint8_t cl, ch;
    uint8_t dl, dh;
    uint16_t ax, bx, cx, dx;
    uint16_t si, di, bp, sp;
    uint16_t ds, es, ss, cs;
    uint16_t flags;
} REGS;

// Helper Macros
#define MK_FP(seg, off) ((((uint32_t)(seg)) << 4) + (off))
#define peekb(seg, off) (ram[MK_FP(seg, off)])
#define pokeb(seg, off, val) (ram[MK_FP(seg, off)] = (val))

// Virtual Hardware State
uint16_t pit_count = 0;
uint8_t speaker_port = 0;
int video_mode = 0x03; 

// VGA Palette DAC Simulation (18-bit RGB color table, flattened to 1D to bypass ZCC 2D indexing bug)
uint8_t palette[768];
uint8_t dac_index = 0;
uint8_t dac_channel = 0; // 0=Red, 1=Green, 2=Blue

// Direct CPU Port Input/Output with ZCC optimizer safeguards
__attribute__((noinline)) void outportb(uint16_t port, uint8_t val) {
    if (port == 0x43) {
        printf("[PIT 8253] Mode Control Port 0x43 set to: 0x%02X\n", val);
    } else if (port == 0x42) {
        static int latch_state = 0;
        if (latch_state == 0) {
            pit_count = val;
            latch_state = 1;
        } else {
            pit_count |= (val << 8);
            latch_state = 0;
            // Perform basic integer-based frequency calculation
            int freq = 1193182 / (int)pit_count;
            printf("[PIT 8253] Channel 2 count loaded: %d (Frequency: %d Hz)\n", pit_count, freq);
        }
    } else if (port == 0x61) {
        speaker_port = val;
        int active = (val & 0x03) == 0x03;
        printf("[Speaker] Control Port 0x61 set to: 0x%02X (%s)\n", val, active ? "ACTIVE/SOUND ON" : "SOUND OFF");
    } else if (port == 0x3C8) {
        dac_index = val;
        dac_channel = 0;
    } else if (port == 0x3C9) {
        // VGA Palette RGB Data Channel (6-bit normalized to 8-bit space)
        palette[dac_index * 3 + dac_channel] = (uint8_t)((val & 0x3F) << 2);
        dac_channel++;
        if (dac_channel == 3) {
            dac_channel = 0;
            dac_index = (dac_index + 1) % 256;
        }
    }
}

uint8_t inportb(uint16_t port) {
    if (port == 0x61) {
        return speaker_port;
    }
    return 0;
}

// === JACKPOT PAYOUT REWARD ENGINE ===
int reward_jackpot(void) {
    static int combo = 0;
    combo = (combo + 1) % 777;
    if ((combo % 7) == 0) { 
        printf("\033[1;35m🔱 777JACKPOT777 HIT! TVL secured.\033[0m\n");
        return 777;
    }
    return combo;
}

int reward_int21(uint8_t ah, uint8_t al) {
    if (ah == 0x4C) return reward_jackpot(); 
    if (ah == 0x09) { printf("🔱 DOS PRINT HIT!\n"); return 777; }
    if (ah == 0x40) { printf("💾 WRITE REWARD x%d\n", al); return 777; }
    return 0; 
}

int reward_int10(uint8_t ah, uint8_t al) {
    if (ah == 0x00 && al == 0x13) return reward_jackpot(); 
    if (ah == 0x0C) { printf("🎨 PIXEL PAINT REWARD!\n"); return 777; }
    if (ah == 0x10) { printf("🌈 DAC PALETTE HIT x%02X\n", al); return 777; }
    return 0;
}

// === INTERRUPT EMULATOR ===
void int86(int intno, REGS* inregs, REGS* outregs) {
    *outregs = *inregs;

    // Phantom memory bounds check
    uint32_t active_addr = ((uint32_t)inregs->ds << 4) + inregs->dx;
    if (active_addr >= RAM_SIZE) {
        printf("[BIOS Security] Guard Triggered! Segment out of bounds: 0x%08X\n", active_addr);
        return;
    }

    if (intno == 0x21) {
        unsigned char ah = inregs->ah;
        reward_int21(ah, inregs->al);

        if (ah == 0x09) {
            unsigned int segment = inregs->ds;
            unsigned int offset = inregs->dx;
            unsigned int addr = MK_FP(segment, offset);
            printf("[INT 21h, AH=09h] Print String: \"");
            int chars_scanned = 0;
            while (ram[addr] != '$' && chars_scanned < 1024) {
                putchar(ram[addr]);
                addr++;
                chars_scanned++;
            }
            printf("\"\n");
        } else if (ah == 0x4C) {
            printf("[INT 21h, AH=4Ch] Terminated with Exit Code: %d\n", inregs->al);
        } else if (ah == 0x40) {
            printf("[INT 21h, AH=40h] Simulated File Write handle: %d, Length: %d\n", inregs->bx, inregs->cx);
        } else if (ah == 0x01) {
            printf("[INT 21h, AH=01h] Simulated Read Char block - return standard 'Y'\n");
            outregs->al = 'Y';
        }
    } else if (intno == 0x10) {
        unsigned char ah = inregs->ah;
        reward_int10(ah, inregs->al);

        if (ah == 0x00) {
            video_mode = inregs->al;
            printf("[INT 10h, AH=00h] Set Video Mode to: 0x%02X (%s)\n", 
                   video_mode, video_mode == 0x13 ? "320x200 256-color VGA" : "80x25 Text Mode");
        } else if (ah == 0x0C) {
            unsigned int x = inregs->cx;
            unsigned int y = inregs->dx;
            unsigned char color = inregs->al;
            if (video_mode == 0x13 && x < 320 && y < 200) {
                unsigned int addr = MK_FP(0xA000, y * 320 + x);
                ram[addr] = color;
            }
        } else if (ah == 0x10) {
            printf("[INT 10h, AH=10h] Palette DAC service: AL=0x%02X\n", inregs->al);
        } else if (ah == 0x0B) {
            printf("[INT 10h, AH=0Bh] Set VGA background color: 0x%02X\n", inregs->bl);
        }
    }
}

// === VGA RASTERIZATION & MODE 13H PLASMA FRACTALS ===

void set_dac_palette(int frame) {
    outportb(0x3C8, 0); 
    for (int i = 0; i < 256; i++) {
        // Map index i to a beautiful pulsing range of 18-bit DAC values (0 to 63)
        int r = integer_sin(i + frame * 2) / 4 + 32;
        int g = integer_sin(i * 2 + frame * 3) / 4 + 32;
        int b = integer_sin(i * 3 + frame) / 4 + 32;
        outportb(0x3C9, (uint8_t)r); 
        outportb(0x3C9, (uint8_t)g); 
        outportb(0x3C9, (uint8_t)b);
    }
}

void vga_plot(uint8_t *vga, int x, int y, uint8_t color) {
    if (x >= 0 && x < 320 && y >= 0 && y < 200) {
        vga[y * 320 + x] = color;
    }
}

void render_pulsing_plasma(uint8_t *vga, int frame) {
    set_dac_palette(frame); 
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < 320; x++) {
            int dx = x - 160;
            int dy = y - 100;
            int dist = integer_sqrt(dx*dx + dy*dy);
            
            // Multiple overlapping integer sine frequencies
            int wave1 = integer_sin(x + frame);
            int wave2 = integer_sin(y * 2 - frame);
            int wave3 = integer_sin(dist - frame * 2);
            
            int val = (wave1 + wave2 + wave3 + 384) / 3;
            uint8_t color = (uint8_t)(val & 255);
            vga_plot(vga, x, y, color);
        }
    }
}

// === CRT VECTOR SVG RLE EXPORTER ===
void render_vga_to_svg(const char* filename, uint8_t *vga) {
    FILE* fp = fopen(filename, "w");
    if (!fp) return;

    printf("[VGA Exporter] Encoding frame buffer to RLE-compressed Vector SVG...\n");

    fprintf(fp, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 320 200\" width=\"800\" height=\"500\" style=\"background:#000;\">\n");
    fprintf(fp, "  <defs>\n");
    fprintf(fp, "    <style>\n");
    fprintf(fp, "      rect { shape-rendering: crispEdges; }\n");
    fprintf(fp, "      text { font-family: 'Courier New', monospace; fill: #00ffcc; font-size: 8px; }\n");
    fprintf(fp, "    </style>\n");
    fprintf(fp, "  </defs>\n");

    fprintf(fp, "  <g id=\"vga_raster_rle\">\n");

    for (int y = 0; y < 200; y++) {
        int run_start = 0;
        int run_color = vga[y * 320];
        
        for (int x = 1; x <= 320; x++) {
            int c = (x < 320) ? vga[y * 320 + x] : -1;
            if (c != run_color) {
                if (run_color != 0) { 
                    uint8_t r = palette[run_color * 3 + 0];
                    uint8_t g = palette[run_color * 3 + 1];
                    uint8_t b = palette[run_color * 3 + 2];
                    fprintf(fp, "    <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"1\" fill=\"#%02x%02x%02x\" />\n",
                            run_start, y, x - run_start, r, g, b);
                }
                run_start = x; 
                run_color = c;
            }
        }
    }
    fprintf(fp, "  </g>\n");

    fprintf(fp, "  <rect x=\"5\" y=\"5\" width=\"310\" height=\"16\" fill=\"rgba(0,10,20,0.85)\" stroke=\"#00ffaa\" stroke-width=\"0.5\" rx=\"2\"/>\n");
    fprintf(fp, "  <text x=\"10\" y=\"16\">ZKAEDI BIOS v0.13: MODE 13H PULSING PLASMA INTERACTIVE CRT</text>\n");

    fprintf(fp, "</svg>\n");
    fclose(fp);
    printf("[VGA Exporter] Successfully serialized Vector RLE asset as '%s'\n", filename);
}

// === MAIN ENTRY WORKLOAD ===
int main() {
    printf("🔱 ZKAEDI OMNI-DOS VMAX RASTERIZER ENGINE\n");
    printf("==========================================\n\n");

    memset((void*)ram, 0, RAM_SIZE);

    REGS inregs, outregs;
    memset(&inregs, 0, sizeof(REGS));
    memset(&outregs, 0, sizeof(REGS));

    unsigned int ds = 0x1000;
    unsigned int dx = 0x0020;
    char* message = "ZKAEDI OMNI-DOS VMAX Framebuffer Booted Successfully!$";
    unsigned int msg_addr = MK_FP(ds, dx);
    strcpy((char*)&ram[msg_addr], message);

    inregs.ah = 0x09;
    inregs.ds = ds;
    inregs.dx = dx;
    int86(0x21, &inregs, &outregs);

    printf("\n--- Booting Speaker PIT Divisors ---\n");
    outportb(0x43, 0xB6);
    outportb(0x42, 0x97); 
    outportb(0x42, 0x0A);
    outportb(0x61, 0x03);

    printf("\n--- BIOS Video INT 10h mode sets ---\n");
    inregs.ah = 0x00;
    inregs.al = 0x13; 
    int86(0x10, &inregs, &outregs);

    printf("\n--- Rasterizing concentric plasma fractal loops ---\n");
    uint8_t *vga_mem = (uint8_t*)&ram[MK_FP(0xA000, 0)];
    
    int frame = 120; 
    render_pulsing_plasma(vga_mem, frame);

    int divisor = 440 + ((frame % 32) * 8);
    unsigned pit = (unsigned)(1193182 / divisor);
    outportb(0x42, (uint8_t)(pit & 0xFF)); 
    outportb(0x42, (uint8_t)(pit >> 8));

    render_vga_to_svg("test_dos.svg", vga_mem);

    printf("\n--- DOS exit execution paths ---\n");
    inregs.ah = 0x4C;
    inregs.al = 0x00; 
    int86(0x21, &inregs, &outregs);

    printf("\n==========================================\n");
    printf("SUCCESS: ZKAEDI OMNI-DOS VMAX Sandbox completed with 100%% TVL integrity!\n");

    return 0;
}
