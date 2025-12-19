#include "proto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

uint16_t calculate_checksum(const unsigned char *data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return (uint16_t)(sum & 0xFFFF);
}

void xor_cipher(unsigned char *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        data[i] ^= XOR_KEY;
    }
}

int send_packet(int sockfd, uint16_t opcode, const void *payload, uint32_t payload_len) {
    PacketHeader header;
    unsigned char *buffer = NULL;
    
    // Prepare payload (Encrypt)
    if (payload_len > 0 && payload != NULL) {
        buffer = (unsigned char *)malloc(payload_len);
        if (!buffer) return -1;
        memcpy(buffer, payload, payload_len);
        
        // Calculate checksum BEFORE encryption (or after? usually checksum is for integrity of transport, so maybe after? 
        // But prompt says "Implement Checksum... Encapsulate...". 
        // Let's do: Checksum of RAW data, then Encrypt. 
        // Receiver: Decrypt, then Checksum.
        header.checksum = htons(calculate_checksum((unsigned char*)payload, payload_len));
        
        xor_cipher(buffer, payload_len);
    } else {
        header.checksum = 0;
    }

    header.length = htonl(payload_len);
    header.opcode = htons(opcode);

    // Send Header
    if (send(sockfd, &header, sizeof(header), 0) != sizeof(header)) {
        free(buffer);
        return -1;
    }

    // Send Payload
    if (payload_len > 0 && buffer != NULL) {
        size_t total_sent = 0;
        while (total_sent < payload_len) {
            ssize_t sent = send(sockfd, buffer + total_sent, payload_len - total_sent, 0);
            if (sent <= 0) {
                free(buffer);
                return -1;
            }
            total_sent += sent;
        }
        free(buffer);
    }

    return 0;
}

int recv_packet(int sockfd, uint16_t *opcode, void **payload, uint32_t *payload_len) {
    PacketHeader header;
    ssize_t received = recv(sockfd, &header, sizeof(header), MSG_WAITALL);
    
    if (received <= 0) return -1;
    if (received != sizeof(header)) return -1;

    uint32_t len = ntohl(header.length);
    *opcode = ntohs(header.opcode);
    uint16_t received_checksum = ntohs(header.checksum);
    *payload_len = len;

    if (len > MAX_PAYLOAD_SIZE) {
        return -1;
    }

    if (len > 0) {
        *payload = malloc(len);
        if (!*payload) return -1;

        size_t total_received = 0;
        while (total_received < len) {
            ssize_t r = recv(sockfd, (unsigned char*)*payload + total_received, len - total_received, 0);
            if (r <= 0) {
                free(*payload);
                *payload = NULL;
                return -1;
            }
            total_received += r;
        }

        // Decrypt
        xor_cipher((unsigned char*)*payload, len);

        // Verify Checksum
        uint16_t calc_checksum = calculate_checksum((unsigned char*)*payload, len);
        if (calc_checksum != received_checksum) {
            fprintf(stderr, "Checksum mismatch! Expected %04x, got %04x\n", received_checksum, calc_checksum);
            free(*payload);
            *payload = NULL;
            return -1;
        }
    } else {
        *payload = NULL;
    }

    return 0;
}
