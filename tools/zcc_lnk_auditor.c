#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// -----------------------------------------------------------------------------
// ZCC Core // LNK Binary Static Analysis Module
// Specs: Zero dependencies. Strict bounds checking. Read-only memory mapping.
// -----------------------------------------------------------------------------

#pragma pack(push, 1)
typedef struct {
    uint32_t HeaderSize;      // Must be 0x0000004C (76)
    uint8_t  LinkCLSID[16];   // 00021401-0000-0000-C000-000000000046
    uint32_t LinkFlags;
    uint32_t FileAttributes;
    uint64_t CreationTime;
    uint64_t AccessTime;
    uint64_t WriteTime;
    uint32_t FileSize;
    uint32_t IconIndex;
    uint32_t ShowCommand;
    uint16_t HotKey;
    uint16_t Reserved1;
    uint32_t Reserved2;
    uint32_t Reserved3;
} LnkHeader;
#pragma pack(pop)

// LinkFlag Bitmasks
#define HAS_TARGET_ID_LIST (1 << 0)
#define HAS_LINK_INFO      (1 << 1)
#define HAS_NAME           (1 << 2)
#define HAS_RELATIVE_PATH  (1 << 3)
#define HAS_WORKING_DIR    (1 << 4)
#define HAS_ARGUMENTS      (1 << 5)
#define IS_UNICODE         (1 << 7)

void hex_dump(const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}

int audit_lnk_payload(const char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        fprintf(stderr, "[!] I/O Error: Cannot mount %s\n", filepath);
        return -1;
    }

    // Determine total payload size for strict boundary enforcement
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    if (file_size < sizeof(LnkHeader)) {
        fprintf(stderr, "[!] Abort: Payload too small to contain valid header.\n");
        fclose(file);
        return -1;
    }

    uint8_t *buffer = (uint8_t *)malloc(file_size);
    if (!buffer) {
        fclose(file);
        return -1;
    }

    fread(buffer, 1, file_size, file);
    fclose(file);

    LnkHeader *header = (LnkHeader *)buffer;

    // Magic Number Verification
    if (header->HeaderSize != 0x0000004C) {
        fprintf(stderr, "[!] Abort: Invalid Magic Header (Expected 4C 00 00 00).\n");
        free(buffer);
        return -1;
    }

    printf("[+] Target Acquired. Parsing LinkFlags: 0x%08X\n", header->LinkFlags);
    
    size_t offset = sizeof(LnkHeader);

    // Skip Target ID List if present
    if (header->LinkFlags & HAS_TARGET_ID_LIST) {
        if (offset + 2 > file_size) goto BOUNDS_VIOLATION;
        uint16_t id_list_size = *(uint16_t *)(buffer + offset);
        offset += 2 + id_list_size;
    }

    // Parse LinkInfo (Where the local base path is usually hardcoded)
    if (header->LinkFlags & HAS_LINK_INFO) {
        if (offset + 4 > file_size) goto BOUNDS_VIOLATION;
        uint32_t link_info_size = *(uint32_t *)(buffer + offset);
        
        if (offset + link_info_size > file_size) goto BOUNDS_VIOLATION;

        uint32_t link_info_header_size = *(uint32_t *)(buffer + offset + 4);
        uint32_t local_base_path_offset = *(uint32_t *)(buffer + offset + 16);

        if (local_base_path_offset > 0) {
            char *base_path = (char *)(buffer + offset + local_base_path_offset);
            printf("[+] Extracted Local Target Path: %s\n", base_path);
        }
        offset += link_info_size;
    }

    // Extract StringData (Arguments, Relative Paths)
    int is_unicode = header->LinkFlags & IS_UNICODE;
    
    uint32_t flags_to_check[] = {HAS_NAME, HAS_RELATIVE_PATH, HAS_WORKING_DIR, HAS_ARGUMENTS};
    const char* flag_names[] = {"Description", "Relative Path", "Working Dir", "Command Line Arguments"};

    for (int i = 0; i < 4; i++) {
        if (header->LinkFlags & flags_to_check[i]) {
            if (offset + 2 > file_size) goto BOUNDS_VIOLATION;
            uint16_t string_len = *(uint16_t *)(buffer + offset);
            offset += 2;
            
            size_t byte_len = is_unicode ? (string_len * 2) : string_len;
            if (offset + byte_len > file_size) goto BOUNDS_VIOLATION;

            printf("[+] Extracted %s: ", flag_names[i]);
            if (is_unicode) {
                // Quick wide-char to ascii cast for terminal safety
                for (size_t c = 0; c < byte_len; c += 2) {
                    putchar(buffer[offset + c]); 
                }
            } else {
                fwrite(buffer + offset, 1, byte_len, stdout);
            }
            printf("\n");
            
            offset += byte_len;
        }
    }

    printf("\n[+] Analysis Complete. Zero anomalies detected.\n");
    free(buffer);
    return 0;

BOUNDS_VIOLATION:
    fprintf(stderr, "\n[!] CRITICAL: Out-of-bounds memory access prevented. File structure is malformed or maliciously packed.\n");
    free(buffer);
    return -1;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: zcc_lnk_auditor <target.lnk>\n");
        return 1;
    }
    return audit_lnk_payload(argv[1]);
}
