#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <pthread.h>

// Game Constants
#define MAP_WIDTH 40
#define MAP_HEIGHT 40
#define MAX_PLAYERS 100
#define PORT 8888
#define MAX_SNAKE_LENGTH 100

// Map Cell Types
#define CELL_EMPTY 0
#define CELL_WALL 1
#define CELL_FOOD 2
#define CELL_PLAYER_BASE 10 // Player ID x is represented as 10 + x

// Protocol Constants
#define PROTO_MAGIC 0xABCD // Optional, but good for sanity check
#define XOR_KEY 0x5A

// OpCodes
#define OP_LOGIN_REQ    0x0001
#define OP_LOGIN_RESP   0x0002
#define OP_MOVE         0x0003
#define OP_UPDATE       0x0004
#define OP_ERROR        0x0005
#define OP_LOGOUT       0x0006
#define OP_DIE          0x0007

// Directions
#define DIR_UP    'W'
#define DIR_DOWN  'S'
#define DIR_LEFT  'A'
#define DIR_RIGHT 'D'

#define MAX_PAYLOAD_SIZE (1024 * 256) // 256KB

// Shared Memory Key (File path for ftok)
#define SHM_KEY_FILE "."
#define SHM_KEY_ID 65

// Structures

typedef struct {
    int x, y;
} Point;

typedef struct {
    Point body[MAX_SNAKE_LENGTH];
    int length;
    char direction; // 'W', 'A', 'S', 'D'
    int alive;
} Snake;

// Packet Header
typedef struct {
    uint32_t length;   // Length of payload
    uint16_t opcode;
    uint16_t checksum;
} __attribute__((packed)) PacketHeader;

// Shared Game State (Stored in Shared Memory)
typedef struct {
    int map[MAP_HEIGHT][MAP_WIDTH];
    int scores[MAX_PLAYERS];
    int active_players[MAX_PLAYERS]; // 0 = inactive, 1 = active
    Snake snakes[MAX_PLAYERS];
    uint64_t version;
    pthread_mutex_t lock;
} GameState;

#endif
