#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    printf("====================================================================\n");
    printf("  🔱 ZCC NATIVE POSIX SOCKETPAIR IPC GAUNTLET 🔱\n");
    printf("====================================================================\n");

    int sv[2];
    /* Test 1: Full-duplex bidirectional socketpair creation */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        printf("[FAIL] socketpair(AF_UNIX, SOCK_STREAM) failed\n");
        return 1;
    }
    printf("[PASS] socketpair created: fd0=%d, fd1=%d\n", sv[0], sv[1]);

    /* Test 2: Direction A -> B (send on sv[0], recv on sv[1]) */
    const char *msg_ab = "ZCC_SOCKETPAIR_FORWARD_CHANNEL_SIGIL_4410";
    ssize_t sent = send(sv[0], msg_ab, strlen(msg_ab), 0);
    if (sent != (ssize_t)strlen(msg_ab)) {
        printf("[FAIL] Direction A->B send incomplete\n");
        close(sv[0]);
        close(sv[1]);
        return 2;
    }

    char recv_buf[64];
    memset(recv_buf, 0, sizeof(recv_buf));
    ssize_t received = recv(sv[1], recv_buf, sizeof(recv_buf) - 1, 0);
    if (received != (ssize_t)strlen(msg_ab) || strcmp(recv_buf, msg_ab) != 0) {
        printf("[FAIL] Direction A->B recv mismatch: '%s'\n", recv_buf);
        close(sv[0]);
        close(sv[1]);
        return 3;
    }
    printf("[PASS] Direction A->B verified: '%s'\n", recv_buf);

    /* Test 3: Direction B -> A (send on sv[1], recv on sv[0] - Full Duplex) */
    const char *msg_ba = "ZCC_SOCKETPAIR_REVERSE_CHANNEL_SIGIL_9921";
    sent = send(sv[1], msg_ba, strlen(msg_ba), 0);
    if (sent != (ssize_t)strlen(msg_ba)) {
        printf("[FAIL] Direction B->A send incomplete\n");
        close(sv[0]);
        close(sv[1]);
        return 4;
    }

    memset(recv_buf, 0, sizeof(recv_buf));
    received = recv(sv[0], recv_buf, sizeof(recv_buf) - 1, 0);
    if (received != (ssize_t)strlen(msg_ba) || strcmp(recv_buf, msg_ba) != 0) {
        printf("[FAIL] Direction B->A recv mismatch: '%s'\n", recv_buf);
        close(sv[0]);
        close(sv[1]);
        return 5;
    }
    printf("[PASS] Direction B->A full-duplex verified: '%s'\n", recv_buf);

    close(sv[0]);
    close(sv[1]);
    printf("\n=== ALL POSIX SOCKETPAIR TESTS PASSED CLEANLY (exit 0) ===\n");
    return 0;
}
