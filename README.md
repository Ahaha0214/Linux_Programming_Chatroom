# High-Concurrency Snake Game Server

A multi-player snake game demonstrating high-concurrency client-server architecture with custom protocol, IPC mechanisms, and fault tolerance.

## Technical Specifications

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         Master Process                          │
│                    (Signal Handling, Cleanup)                   │
└─────────────────────────────────────────────────────────────────┘
                                │
                    ┌───────────┼───────────┐
                    │           │           │
                    ▼           ▼           ▼
            ┌───────────┐ ┌───────────┐ ┌───────────┐
            │  Worker 0 │ │  Worker 1 │ │  Worker N │  (Prefork)
            │  select() │ │  select() │ │  select() │
            └─────┬─────┘ └─────┬─────┘ └─────┬─────┘
                  │             │             │
                  └─────────────┼─────────────┘
                                │
                    ┌───────────┴───────────┐
                    │    Shared Memory      │
                    │  (GameState + Mutex)  │
                    └───────────┬───────────┘
                                │
                    ┌───────────┴───────────┐
                    │   Game Loop Process   │
                    │   (200ms tick rate)   │
                    └───────────────────────┘
```

### Client Architecture

```
┌─────────────────────────────────────────┐
│              Main Thread                │
│         (Login, Coordination)           │
└─────────────────────────────────────────┘
         │           │           │
         ▼           ▼           ▼
    ┌─────────┐ ┌─────────┐ ┌─────────┐
    │  Input  │ │ Receive │ │Heartbeat│
    │ Thread  │ │ Thread  │ │ Thread  │
    └─────────┘ └─────────┘ └─────────┘
```

## Features Checklist

### Client Side 
- [x] Multi-threaded architecture (Input, Receive, Heartbeat threads)
- [x] Stress testing with 100+ concurrent connections
- [x] Latency statistics (microseconds)
- [x] Throughput statistics (requests/second)

### Server Side 
- [x] Multi-Process architecture (Prefork pattern with 8 workers)
- [x] IPC via Shared Memory (`shmget`/`shmat`)
- [x] Process-shared mutex (`PTHREAD_PROCESS_SHARED`)
- [x] Game loop in separate process

### Protocol Design 
- [x] Custom application-layer protocol (NOT HTTP/WebSocket)
- [x] Packet structure: `[Length 4B][OpCode 2B][Checksum 2B][Data]`

### Security & Reliability 
- [x] Integrity Check: Checksum verification
- [x] Encryption: XOR cipher on payload
- [x] Authentication: Login handshake (`OP_LOGIN_REQ`/`OP_LOGIN_RESP`)
- [x] Keep-Alive: Heartbeat mechanism (`OP_HEARTBEAT`/`OP_HEARTBEAT_ACK`)
- [x] Graceful Shutdown: SIGINT handler with resource cleanup
- [x] Timeout Handling: Client timeout after 10 seconds of inactivity

### Modularity 
- [x] Static library (`libgame.a`) containing:
  - Protocol module (`proto.c`)
  - Logging module (`logging.c`)
- [x] Makefile build system

## Protocol Specification

### Packet Header (8 bytes)
```c
typedef struct {
    uint32_t length;   // Payload length (network byte order)
    uint16_t opcode;   // Operation code
    uint16_t checksum; // Checksum of decrypted payload
} __attribute__((packed)) PacketHeader;
```

### OpCodes
| OpCode | Name | Direction | Description |
|--------|------|-----------|-------------|
| 0x0001 | `OP_LOGIN_REQ` | C→S | Login request |
| 0x0002 | `OP_LOGIN_RESP` | S→C | Login response (player ID) |
| 0x0003 | `OP_MOVE` | C→S | Direction change (W/A/S/D) |
| 0x0004 | `OP_UPDATE` | S→C | Map state update |
| 0x0005 | `OP_ERROR` | S→C | Error message |
| 0x0006 | `OP_LOGOUT` | C→S | Logout request |
| 0x0007 | `OP_DIE` | S→C | Player death notification |
| 0x0008 | `OP_HEARTBEAT` | C→S | Keep-alive ping |
| 0x0009 | `OP_HEARTBEAT_ACK` | S→C | Keep-alive response |

### Security
- **Checksum**: Sum of all payload bytes, stored as uint16
- **Encryption**: XOR cipher with key `0x5A` applied to payload
- **Process**: Sender: Calculate checksum → Encrypt → Send
- **Process**: Receiver: Receive → Decrypt → Verify checksum

## Building

```bash
# Build all
make all

# Build specific target
make server
make client

# Clean
make clean
```

### Dependencies
- GCC
- pthread library
- POSIX shared memory

## Running

### Start Server
```bash
./server
```
Output:
```
Server listening on port 8888
Worker 0 started.
Worker 1 started.
...
Game Loop Process Started (PID: xxxx)
```

### Start Client (Game Mode)
```bash
./client
```
Controls:
- `W/A/S/D` - Move snake
- `Q` - Quit

### Stress Test
```bash
# Default 100 clients
./client -stress

# Custom number of clients
./client -stress 200
```

Output:
```
========================================
  Stress Test - 100 Concurrent Clients
========================================

========================================
  Stress Test Results
========================================
  Concurrent Clients:    100
  Successful Connections: 100
  Total Requests:        5000
  Total Time:            12.34 seconds
  Avg Latency:           2500 us (2.50 ms)
  Throughput:            405.19 requests/sec
========================================
```

## File Structure

```
.
├── Makefile          # Build system
├── README.md         # This file
├── common.h          # Shared constants and structures
├── proto.h           # Protocol function declarations
├── proto.c           # Protocol implementation
├── logging.h         # Logging module header
├── logging.c         # Logging module implementation
├── server.c          # Server implementation
├── client.c          # Client implementation
└── libgame.a         # Static library (generated)
```

## Team Contributions

| Member | Modules |
|--------|---------|
| Member A | Server (Multi-process, IPC, Game Loop) |
| Member B | Client (Multi-threaded, Stress Test) |
| Member C | Protocol (Encryption, Checksum, Packet Handling) |
| Member D | Documentation, Testing, Integration |

## Design Rationale

### Why Prefork Pattern?
- Avoids fork() overhead for each connection
- Workers are ready to accept connections immediately
- Shared memory allows efficient state synchronization

### Why Shared Memory + Mutex?
- Fastest IPC mechanism for large state (6400 byte map)
- Single mutex with `PTHREAD_PROCESS_SHARED` ensures consistency
- Game loop and workers can access state concurrently

### Why Custom Protocol?
- Binary protocol is more efficient than text (HTTP)
- Fixed header size allows easy parsing
- OpCode-based design is extensible

### Why Heartbeat?
- Detects dead clients (network issues, crashes)
- Server can clean up resources for inactive clients
- Prevents resource exhaustion from zombie connections

## License

MIT License
