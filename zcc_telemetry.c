#include "zcc_telemetry.h"

#ifndef __zcc__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/socket.h>

static int telemetry_sock = -1;

static uint64_t get_timestamp_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;
}

void telemetry_init(const char* host, int port) {
    struct sockaddr_in serv_addr;
    telemetry_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (telemetry_sock < 0) return;

    // Set non-blocking to prevent compiler stalls
    int flags = fcntl(telemetry_sock, F_GETFL, 0);
    fcntl(telemetry_sock, F_SETFL, flags | O_NONBLOCK);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &serv_addr.sin_addr);

    // Fire and forget connection
    connect(telemetry_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
}

void telemetry_emit_node(TelemetryOp op, const char* node_type, int depth) {
    if (telemetry_sock < 0) return;
    char buffer[256];
    int len = snprintf(buffer, sizeof(buffer), 
        "{\"op\": %d, \"node\": \"%s\", \"depth\": %d, \"ts\": %llu}\n", 
        op, node_type, depth, (unsigned long long)get_timestamp_ms());
    send(telemetry_sock, buffer, len, MSG_NOSIGNAL);
}

void telemetry_emit_mem(TelemetryOp op, uint64_t ptr_address, size_t size) {
    if (telemetry_sock < 0) return;
    char buffer[256];
    int len = snprintf(buffer, sizeof(buffer), 
        "{\"op\": %d, \"ptr\": \"0x%llx\", \"size\": %zu, \"ts\": %llu}\n", 
        op, (unsigned long long)ptr_address, size, (unsigned long long)get_timestamp_ms());
    send(telemetry_sock, buffer, len, MSG_NOSIGNAL);
}

void telemetry_emit_coverage(float coverage_pct) {
    if (telemetry_sock < 0) return;
    char buffer[128];
    int len = snprintf(buffer, sizeof(buffer), 
        "{\"op\": %d, \"coverage\": %.3f, \"ts\": %llu}\n", 
        COVERAGE_SYNC, coverage_pct, (unsigned long long)get_timestamp_ms());
    send(telemetry_sock, buffer, len, MSG_NOSIGNAL);
}

void telemetry_close(void) {
    if (telemetry_sock >= 0) {
        close(telemetry_sock);
        telemetry_sock = -1;
    }
}

#else

// Stubs for ZCC self-hosting compilation path
void telemetry_init(const char* host, int port) {
    (void)host;
    (void)port;
}

void telemetry_emit_node(TelemetryOp op, const char* node_type, int depth) {
    (void)op;
    (void)node_type;
    (void)depth;
}

void telemetry_emit_mem(TelemetryOp op, uint64_t ptr_address, size_t size) {
    (void)op;
    (void)ptr_address;
    (void)size;
}

void telemetry_emit_coverage(float coverage_pct) {
    (void)coverage_pct;
}

void telemetry_close(void) {
}

#endif
