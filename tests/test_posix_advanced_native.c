#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("====================================================================\n");
    printf("  🔱 ZCC NATIVE ADVANCED POSIX EXPANSION GAUNTLET 🔱\n");
    printf("====================================================================\n");

    /* Test 1: Anonymous Virtual Memory Allocation with mmap & munmap */
    size_t page_sz = 4096;
    char *anon_mem = (char *)mmap(NULL, page_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (anon_mem == MAP_FAILED) {
        printf("[FAIL] mmap(MAP_ANONYMOUS) failed\n");
        return 1;
    }
    const char *mmap_msg = "ZCC_ANONYMOUS_MMAP_VIRTUAL_PAGE_VERIFIED_9901";
    strcpy(anon_mem, mmap_msg);
    if (strcmp(anon_mem, mmap_msg) != 0) {
        printf("[FAIL] Virtual page memory integrity corrupted\n");
        munmap(anon_mem, page_sz);
        return 2;
    }
    printf("[PASS] mmap() allocated 4096 bytes: '%s'\n", anon_mem);
    if (munmap(anon_mem, page_sz) != 0) {
        printf("[FAIL] munmap() failed\n");
        return 3;
    }
    printf("[PASS] munmap() unmapped virtual memory page cleanly\n");

    /* Test 2: POSIX Inter-Process Communication via pipe() */
    int fds[2];
    if (pipe(fds) != 0) {
        printf("[FAIL] pipe() creation failed\n");
        return 4;
    }
    const char *pipe_payload = "IPC_PIPE_PAYLOAD_8820";
    write(fds[1], pipe_payload, strlen(pipe_payload));
    close(fds[1]);

    char pipe_buf[32];
    memset(pipe_buf, 0, sizeof(pipe_buf));
    read(fds[0], pipe_buf, sizeof(pipe_buf) - 1);
    close(fds[0]);

    if (strcmp(pipe_buf, pipe_payload) != 0) {
        printf("[FAIL] pipe() payload mismatch: '%s'\n", pipe_buf);
        return 5;
    }
    printf("[PASS] pipe() IPC payload roundtrip verified -> '%s'\n", pipe_buf);

    /* Test 3: Process Fork & Synchronization via fork() and waitpid() */
    pid_t pid = fork();
    if (pid < 0) {
        printf("[FAIL] fork() failed\n");
        return 6;
    } else if (pid == 0) {
        /* Child process: exit with deterministic status 42 */
        _exit(42);
    } else {
        /* Parent process: wait for child termination */
        int status = 0;
        pid_t waited_pid = waitpid(pid, &status, 0);
        if (waited_pid != pid) {
            printf("[FAIL] waitpid returned %d, expected %d\n", waited_pid, pid);
            return 7;
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 42) {
            printf("[FAIL] Child exit status mismatch: status=0x%x exit=%d\n", status, WEXITSTATUS(status));
            return 8;
        }
        printf("[PASS] fork() & waitpid() verified: Child %d exited with code %d\n", pid, WEXITSTATUS(status));
    }

    printf("\n=== ALL ADVANCED POSIX EXPANSION TESTS PASSED CLEANLY (exit 0) ===\n");
    return 0;
}
