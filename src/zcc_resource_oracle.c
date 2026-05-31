#include "zcc_resource_oracle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef _WIN32
/* Windows doesn't easily support raw POSIX socket calls in this environment without WS2_32 link */
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

/* ── Global Observability State ────────────────────────────────────────── */
static FILE *s_ledger_fp = NULL;
static const char *s_source_file = "unknown";
static size_t s_malloc_count = 0;
static size_t s_free_count = 0;
static size_t s_total_alloc_bytes = 0;
static size_t s_peak_alloc_bytes = 0;
static long s_start_time = 0;

static int s_udp_enabled = 0;
static int s_sock_fd = -1;
#ifndef _WIN32
static struct sockaddr_in s_addr;
#endif

/* Dispatch UDP event wrapper matching the Gods Eye envelope */
static void send_telemetry_event(const char *body_escaped) {
#ifndef _WIN32
    if (s_udp_enabled && s_sock_fd >= 0) {
        char pkt[2048];
        int len = snprintf(pkt, sizeof(pkt),
            "{\"_body\":\"%s\",\"_sig\":\"ir_telemetry\"}", body_escaped);
        if (len > 0 && len < (int)sizeof(pkt)) {
            sendto(s_sock_fd, pkt, len, MSG_DONTWAIT,
                   (struct sockaddr *)&s_addr, sizeof(s_addr));
        }
    }
#endif
}

void zcc_oracle_init(const char *source_file) {
    s_source_file = source_file ? source_file : "unknown";
    s_malloc_count = 0;
    s_free_count = 0;
    s_total_alloc_bytes = 0;
    s_peak_alloc_bytes = 0;
    s_start_time = (long)clock();

    /* Open the ledger in overwrite mode for each unique compilation run */
    s_ledger_fp = fopen("zcc_resource_events.jsonl", "w");
    if (s_ledger_fp) {
        fprintf(s_ledger_fp, "{\"type\":\"metadata\",\"source_file\":\"%s\",\"timestamp\":%ld}\n", 
                s_source_file, (long)time(NULL));
        fflush(s_ledger_fp);
    }

#ifndef _WIN32
    /* Setup UDP Socket matching Gods Eye telemetry */
    const char *env = getenv("ZCC_EMIT_TELEMETRY");
    if (env && env[0] != '0' && env[0] != '\0') {
        s_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (s_sock_fd >= 0) {
            memset(&s_addr, 0, sizeof(s_addr));
            s_addr.sin_family = AF_INET;
            s_addr.sin_port = htons(41337);
            inet_pton(AF_INET, "127.0.0.1", &s_addr.sin_addr);
            s_udp_enabled = 1;
        }
    }
#endif
}

void zcc_oracle_shutdown(void) {
    long end_time = (long)clock();
    double duration_ms = (double)(end_time - s_start_time) * 1000.0 / CLOCKS_PER_SEC;

    if (s_ledger_fp) {
        fprintf(s_ledger_fp, "{\"type\":\"summary\",\"source_file\":\"%s\",\"malloc_count\":%zu,\"free_count\":%zu,\"leak_count\":%zu,\"peak_bytes\":%zu,\"duration_ms\":%.2f}\n",
                s_source_file, s_malloc_count, s_free_count, 
                (s_malloc_count >= s_free_count) ? (s_malloc_count - s_free_count) : 0, 
                s_peak_alloc_bytes, duration_ms);
        fclose(s_ledger_fp);
        s_ledger_fp = NULL;
    }

#ifndef _WIN32
    if (s_sock_fd >= 0) {
        close(s_sock_fd);
        s_sock_fd = -1;
    }
    s_udp_enabled = 0;
#endif
}

void zcc_oracle_log_prediction(const char *filename, int loops, int indirections, int structs) {
    const char *risk = "low";
    if (loops > 25 || indirections > 5 || structs > 15) {
        risk = "high";
    } else if (loops > 5 || indirections > 1 || structs > 3) {
        risk = "medium";
    }

    if (s_ledger_fp) {
        fprintf(s_ledger_fp, "{\"type\":\"preprocess_prediction\",\"file\":\"%s\",\"risk\":\"%s\",\"loops\":%d,\"indirections\":%d,\"structs\":%d}\n",
                filename ? filename : "unknown", risk, loops, indirections, structs);
        fflush(s_ledger_fp);
    }

    /* Broadcast prediction (op = 1) to visualizer */
    char body[512];
    snprintf(body, sizeof(body), 
        "{\\\"op\\\":1,\\\"file\\\":\\\"%s\\\",\\\"risk\\\":\\\"%s\\\",\\\"loops\\\":%d,\\\"indirections\\\":%d,\\\"structs\\\":%d}",
        filename ? filename : "unknown", risk, loops, indirections, structs);
    send_telemetry_event(body);
}

void zcc_oracle_log_allocation(void *ptr, size_t size) {
    if (!ptr) return;
    s_malloc_count++;
    s_total_alloc_bytes += size;
    if (s_total_alloc_bytes > s_peak_alloc_bytes) {
        s_peak_alloc_bytes = s_total_alloc_bytes;
    }

    static int s_verbose_checked = 0;
    static int s_verbose = 0;
    if (!s_verbose_checked) {
        const char *env = getenv("ZCC_ORACLE_VERBOSE");
        s_verbose = (env && env[0] != '0' && env[0] != '\0');
        s_verbose_checked = 1;
    }

    if (s_verbose) {
        if (s_ledger_fp) {
            fprintf(s_ledger_fp, "{\"type\":\"alloc\",\"ptr\":\"%p\",\"size\":%zu,\"total_allocated\":%zu}\n",
                    ptr, size, s_total_alloc_bytes);
            fflush(s_ledger_fp);
        }

        /* Broadcast OP_ALLOC (op = 2) to visualizer */
        char body[512];
        snprintf(body, sizeof(body), 
            "{\\\"op\\\":2,\\\"ptr\\\":\\\"%p\\\",\\\"size\\\":%zu,\\\"ts\\\":%ld}",
            ptr, size, (long)time(NULL));
        send_telemetry_event(body);
    }
}

void zcc_oracle_log_free(void *ptr) {
    if (!ptr) return;
    s_free_count++;
    
    static int s_verbose_checked = 0;
    static int s_verbose = 0;
    if (!s_verbose_checked) {
        const char *env = getenv("ZCC_ORACLE_VERBOSE");
        s_verbose = (env && env[0] != '0' && env[0] != '\0');
        s_verbose_checked = 1;
    }

    if (s_verbose) {
        if (s_ledger_fp) {
            fprintf(s_ledger_fp, "{\"type\":\"free\",\"ptr\":\"%p\"}\n", ptr);
            fflush(s_ledger_fp);
        }

        /* Broadcast OP_FREE (op = 3) to visualizer */
        char body[512];
        snprintf(body, sizeof(body), 
            "{\\\"op\\\":3,\\\"ptr\\\":\\\"%p\\\",\\\"ts\\\":%ld}",
            ptr, (long)time(NULL));
        send_telemetry_event(body);
    }
}

void zcc_oracle_log_pass(const char *pass_name, const char *function_name, 
                         double duration_ms, size_t heap_bytes, 
                         int spills, int virtual_regs, int physical_regs) {
    if (s_ledger_fp) {
        fprintf(s_ledger_fp, "{\"type\":\"pass\",\"pass_name\":\"%s\",\"function\":\"%s\",\"duration_ms\":%.2f,\"heap_bytes\":%zu,\"spills\":%d,\"virtual_regs\":%d,\"physical_regs\":%d}\n",
                pass_name, function_name ? function_name : "_global", 
                duration_ms, heap_bytes, spills, virtual_regs, physical_regs);
        fflush(s_ledger_fp);
    }

    /* Broadcast OP_PASS_ENTER (op = 10) / OP_PASS_EXIT (op = 11) to visualizer */
    char body[1024];
    snprintf(body, sizeof(body), 
        "{\\\"op\\\":10,\\\"pass\\\":\\\"%s\\\",\\\"fn\\\":\\\"%s\\\",\\\"duration\\\":%.2f,\\\"heap\\\":%zu,\\\"spills\\\":%d,\\\"vregs\\\":%d}",
        pass_name, function_name ? function_name : "_global", duration_ms, heap_bytes, spills, virtual_regs);
    send_telemetry_event(body);
}

void zcc_oracle_log_elf(const char *obj_name, size_t text_bytes, 
                        int rela_entries, int symtab_entries, 
                        size_t strtab_bytes, size_t shstrtab_bytes, 
                        size_t padding_bytes) {
    double relocation_density = text_bytes > 0 ? (double)rela_entries / text_bytes : 0.0;
    size_t total_size = text_bytes + (rela_entries * 24) + (symtab_entries * 24) + strtab_bytes + shstrtab_bytes + padding_bytes;
    double padding_ratio = total_size > 0 ? (double)padding_bytes / total_size : 0.0;

    if (s_ledger_fp) {
        fprintf(s_ledger_fp, "{\"type\":\"elf_geometry\",\"object\":\"%s\",\"text_bytes\":%zu,\"rela_entries\":%d,\"symtab_entries\":%d,\"strtab_bytes\":%zu,\"shstrtab_bytes\":%zu,\"padding_bytes\":%zu,\"relocation_density\":%.4f,\"padding_ratio\":%.4f}\n",
                obj_name, text_bytes, rela_entries, symtab_entries, 
                strtab_bytes, shstrtab_bytes, padding_bytes, 
                relocation_density, padding_ratio);
        fflush(s_ledger_fp);
    }

    /* Broadcast OP_ELF_SECTION (op = 18) to visualizer */
    char body[1024];
    snprintf(body, sizeof(body), 
        "{\\\"op\\\":18,\\\"object\\\":\\\"%s\\\",\\\"text\\\":%zu,\\\"relas\\\":%d,\\\"syms\\\":%d,\\\"padding\\\":%zu,\\\"ratio\\\":%.4f}",
        obj_name, text_bytes, rela_entries, symtab_entries, padding_bytes, padding_ratio);
    send_telemetry_event(body);
}

void zcc_oracle_log_sentinel(size_t idt_size, size_t gdt_size, 
                             size_t base_offset, size_t idt_entry_size, 
                             size_t gdt_entry_size, size_t multiboot_offset) {
    int idt_ok = (idt_size == 10) && (idt_entry_size == 16);
    int gdt_ok = (gdt_size == 10) && (gdt_entry_size == 8);
    int offset_ok = (base_offset == 2);
    int multiboot_ok = (multiboot_offset < 8192);

    if (s_ledger_fp) {
        fprintf(s_ledger_fp, "{\"type\":\"kernel_sentinel\",\"idt_ok\":%d,\"gdt_ok\":%d,\"offset_ok\":%d,\"multiboot_ok\":%d}\n",
                idt_ok, gdt_ok, offset_ok, multiboot_ok);
        fflush(s_ledger_fp);
    }
}
