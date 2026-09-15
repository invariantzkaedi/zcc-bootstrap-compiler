#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *test_file = "/tmp/zcc_posix_test.txt";
    const char *payload = "ZCC_POSIX_SYSTEM_BRIDGE_VERIFIED_7701";
    char read_buf[64];

    /* Test 1: POSIX Process Interrogation */
    pid_t my_pid = getpid();
    if (my_pid <= 0) {
        printf("[FAIL] Invalid PID: %d\n", my_pid);
        return 1;
    }
    printf("[PASS] POSIX getpid() -> %d\n", my_pid);

    /* Test 2: POSIX File Creation and Write (open, write, close) */
    int fd = open(test_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        printf("[FAIL] Failed to open test file for writing\n");
        return 2;
    }

    size_t payload_len = strlen(payload);
    ssize_t written = write(fd, payload, payload_len);
    close(fd);

    if (written != (ssize_t)payload_len) {
        printf("[FAIL] Wrote %ld bytes, expected %lu\n", (long)written, (unsigned long)payload_len);
        return 3;
    }
    printf("[PASS] POSIX write() -> %ld bytes written\n", (long)written);

    /* Test 3: POSIX File Read and Offset Seeking (open, lseek, read, close) */
    fd = open(test_file, O_RDONLY);
    if (fd < 0) {
        printf("[FAIL] Failed to open test file for reading\n");
        return 4;
    }

    off_t offset = lseek(fd, 0, SEEK_SET);
    if (offset != 0) {
        printf("[FAIL] lseek returned %ld, expected 0\n", (long)offset);
        close(fd);
        return 5;
    }

    memset(read_buf, 0, sizeof(read_buf));
    ssize_t n_read = read(fd, read_buf, sizeof(read_buf) - 1);
    close(fd);

    if (n_read != (ssize_t)payload_len) {
        printf("[FAIL] Read %ld bytes, expected %lu\n", (long)n_read, (unsigned long)payload_len);
        return 6;
    }

    if (strcmp(read_buf, payload) != 0) {
        printf("[FAIL] Content mismatch: '%s' vs '%s'\n", read_buf, payload);
        return 7;
    }
    printf("[PASS] POSIX read() payload match -> '%s'\n", read_buf);

    /* Test 4: POSIX File Unlink */
    int unlinked = unlink(test_file);
    if (unlinked != 0) {
        printf("[FAIL] unlink() failed\n");
        return 8;
    }
    printf("[PASS] POSIX unlink() -> successfully cleaned up\n");

    printf("\n=== ALL POSIX BRIDGE TESTS PASSED CLEANLY (exit 0) ===\n");
    return 0;
}
