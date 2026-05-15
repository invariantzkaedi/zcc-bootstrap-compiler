#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define IPC_PORT_IN 8888
#define IPC_PORT_OUT 8889
#define TIMEOUT_MS 50
#define MAX_BYTECODE_SIZE 65536

// Stealth Counter-Defense Jump Buffer
jmp_buf watchdog_jump;

// Signal Handler for Gas-Bomb Detonation
void handle_alarm(int sig) {
    longjmp(watchdog_jump, 1);
}

// Dummy prototype for the EVM lifter
// In reality, this links to evm_lifter.c and ir_gemma_forward.c
extern void run_evm_lifter_analysis(const char* bytecode_hex);

int main() {
    printf("[ZKAEDI WATCHDOG] ARMING STEALTH COUNTER-MEASURES.\n");
    printf("[ZKAEDI WATCHDOG] 50ms HARD-KILL TIMER ENGAGED.\n");

    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    char buffer[MAX_BYTECODE_SIZE];

    // Create UDP IPC Socket (Non-blocking, zero dependency)
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(IPC_PORT_IN);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Set up POSIX Microsecond Timer Signal Handler
    signal(SIGALRM, handle_alarm);

    struct itimerval timer;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 0;

    printf("[ZKAEDI WATCHDOG] LISTENING ON UDP IPC PORT %d...\n", IPC_PORT_IN);

    while (1) {
        socklen_t len = sizeof(cliaddr);
        int n = recvfrom(sockfd, (char *)buffer, MAX_BYTECODE_SIZE, MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
        if (n > 0) {
            buffer[n] = '\0';

            // Arm the 50ms Watchdog
            timer.it_value.tv_sec = 0;
            timer.it_value.tv_usec = TIMEOUT_MS * 1000;
            setitimer(ITIMER_REAL, &timer, NULL);

            if (setjmp(watchdog_jump) == 0) {
                // EXECUTE LIFTER (The Dangerous Part)
                // run_evm_lifter_analysis(buffer); 
                
                // Disarm timer if successful
                timer.it_value.tv_usec = 0;
                setitimer(ITIMER_REAL, &timer, NULL);
                // printf("[ZKAEDI WATCHDOG] Analysis clean.\n");
            } else {
                // COUNTER-ATTACK PHASE
                printf("\n[!!!] GAS BOMB DETECTED [!!!]\n");
                printf("[ZKAEDI WATCHDOG] Execution exceeded %dms. Thread aborted instantly.\n", TIMEOUT_MS);
                
                // Send counter-attack telemetry back to Python
                int out_sock = socket(AF_INET, SOCK_DGRAM, 0);
                struct sockaddr_in out_addr;
                out_addr.sin_family = AF_INET;
                out_addr.sin_port = htons(IPC_PORT_OUT);
                inet_pton(AF_INET, "127.0.0.1", &out_addr.sin_addr);
                
                const char* counter_payload = "{\"event\":\"GAS_BOMB\", \"action\":\"BAN_DEPLOYER\"}";
                sendto(out_sock, counter_payload, strlen(counter_payload), 0, (const struct sockaddr *)&out_addr, sizeof(out_addr));
                close(out_sock);

                printf("[ZKAEDI WATCHDOG] Stealth counter-attack dispatched. Waiting for next payload...\n");
            }
        }
    }
    close(sockfd);
    return 0;
}
