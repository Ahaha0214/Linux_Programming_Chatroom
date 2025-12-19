#ifndef PROTO_H
#define PROTO_H

#include "common.h"
#include <stddef.h>

// Function Prototypes
uint16_t calculate_checksum(const unsigned char *data, size_t len);
void xor_cipher(unsigned char *data, size_t len);

// Returns 0 on success, -1 on failure
int send_packet(int sockfd, uint16_t opcode, const void *payload, uint32_t payload_len);

// Returns 0 on success, -1 on failure/disconnect. 
// Allocates memory for *payload which must be freed by caller.
int recv_packet(int sockfd, uint16_t *opcode, void **payload, uint32_t *payload_len);

#endif
