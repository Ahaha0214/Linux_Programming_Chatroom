#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/select.h>
#include <time.h>

#include "common.h"
#include "proto.h"

#define NUM_WORKERS 8
#define TICK_RATE_MS 200

int shmid;
GameState *game_state;
int server_fd;
pid_t workers[NUM_WORKERS];
pid_t game_loop_pid;
int running = 1;

void cleanup_resources() {
    printf("Cleaning up resources...\n");
    if (game_state) {
        // Destroy mutex
        pthread_mutex_destroy(&game_state->lock);
        // Detach shared memory
        shmdt(game_state);
    }
    // Remove shared memory
    if (shmid != -1) {
        shmctl(shmid, IPC_RMID, NULL);
    }
    if (server_fd != -1) {
        close(server_fd);
    }
}

void handle_sigint(int sig) {
    running = 0;
    // Master process kills workers
    for (int i = 0; i < NUM_WORKERS; i++) {
        if (workers[i] > 0) {
            kill(workers[i], SIGTERM);
        }
    }
    if (game_loop_pid > 0) kill(game_loop_pid, SIGTERM);
    while (wait(NULL) > 0); // Wait for all children
    cleanup_resources();
    exit(0);
}

void init_game_map() {
    memset(game_state, 0, sizeof(GameState));
    
    // Initialize Mutex with PTHREAD_PROCESS_SHARED
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&game_state->lock, &attr);
    pthread_mutexattr_destroy(&attr);

    // Initialize Map
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (x == 0 || x == MAP_WIDTH - 1 || y == 0 || y == MAP_HEIGHT - 1) {
                game_state->map[y][x] = CELL_WALL;
            } else {
                game_state->map[y][x] = CELL_EMPTY;
            }
        }
    }

    // Place some initial food
    srand(time(NULL));
    for (int i = 0; i < 20; i++) {
        int rx = rand() % (MAP_WIDTH - 2) + 1;
        int ry = rand() % (MAP_HEIGHT - 2) + 1;
        game_state->map[ry][rx] = CELL_FOOD;
    }
}

void spawn_food() {
    // Assumes lock is held
    int placed = 0;
    while (!placed) {
        int rx = rand() % (MAP_WIDTH - 2) + 1;
        int ry = rand() % (MAP_HEIGHT - 2) + 1;
        if (game_state->map[ry][rx] == CELL_EMPTY) {
            game_state->map[ry][rx] = CELL_FOOD;
            placed = 1;
        }
    }
}

void game_tick_loop() {
    printf("Game Loop Process Started (PID: %d)\n", getpid());
    while (running) {
        pthread_mutex_lock(&game_state->lock);
        
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game_state->active_players[i] && game_state->snakes[i].alive) {
                Snake *s = &game_state->snakes[i];
                Point head = s->body[0];
                Point new_head = head;

                if (s->direction == DIR_UP) new_head.y--;
                else if (s->direction == DIR_DOWN) new_head.y++;
                else if (s->direction == DIR_LEFT) new_head.x--;
                else if (s->direction == DIR_RIGHT) new_head.x++;

                // Check collisions
                int collision = 0;
                if (game_state->map[new_head.y][new_head.x] == CELL_WALL) collision = 1;
                else if (game_state->map[new_head.y][new_head.x] >= CELL_PLAYER_BASE) {
                     // Hit self or other
                     collision = 1;
                }

                if (collision) {
                    // Die
                    s->alive = 0;
                    game_state->active_players[i] = 0; // Mark inactive so workers know
                    // Clear body
                    for (int j = 0; j < s->length; j++) {
                        game_state->map[s->body[j].y][s->body[j].x] = CELL_EMPTY;
                    }
                    printf("Player %d died.\n", i);
                } else {
                    int grow = 0;
                    if (game_state->map[new_head.y][new_head.x] == CELL_FOOD) {
                        grow = 1;
                        game_state->scores[i]++;
                        spawn_food();
                    }

                    // Move Body
                    // If not growing, clear tail
                    if (!grow) {
                        Point tail = s->body[s->length - 1];
                        game_state->map[tail.y][tail.x] = CELL_EMPTY;
                    } else {
                        if (s->length < MAX_SNAKE_LENGTH) {
                            s->length++;
                        }
                    }

                    // Shift body segments
                    for (int j = s->length - 1; j > 0; j--) {
                        s->body[j] = s->body[j - 1];
                    }
                    s->body[0] = new_head;
                    game_state->map[new_head.y][new_head.x] = CELL_PLAYER_BASE + i;
                }
            }
        }
        
        game_state->version++;
        pthread_mutex_unlock(&game_state->lock);
        usleep(TICK_RATE_MS * 1000);
    }
}

void handle_client_message(int client_fd, int *player_id) {
    uint16_t opcode;
    void *payload = NULL;
    uint32_t len;

    if (recv_packet(client_fd, &opcode, &payload, &len) < 0) {
        // Disconnect
        if (*player_id >= 0) {
            pthread_mutex_lock(&game_state->lock);
            game_state->active_players[*player_id] = 0;
            // Remove player from map
            for(int y=0; y<MAP_HEIGHT; y++) {
                for(int x=0; x<MAP_WIDTH; x++) {
                    if(game_state->map[y][x] == CELL_PLAYER_BASE + *player_id) {
                        game_state->map[y][x] = CELL_EMPTY;
                    }
                }
            }
            pthread_mutex_unlock(&game_state->lock);
            printf("Player %d disconnected.\n", *player_id);
        }
        close(client_fd);
        *player_id = -1; // Signal to remove from select set
        return;
    }

    if (opcode == OP_LOGIN_REQ) {
        pthread_mutex_lock(&game_state->lock);
        int new_id = -1;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!game_state->active_players[i]) {
                game_state->active_players[i] = 1;
                game_state->scores[i] = 0;
                new_id = i;
                
                // Initialize Snake
                game_state->snakes[i].length = 1;
                game_state->snakes[i].alive = 1;
                game_state->snakes[i].direction = DIR_RIGHT; // Default

                // Spawn player
                int placed = 0;
                while(!placed) {
                    int rx = rand() % (MAP_WIDTH - 2) + 1;
                    int ry = rand() % (MAP_HEIGHT - 2) + 1;
                    if (game_state->map[ry][rx] == CELL_EMPTY) {
                        game_state->map[ry][rx] = CELL_PLAYER_BASE + new_id;
                        game_state->snakes[i].body[0].x = rx;
                        game_state->snakes[i].body[0].y = ry;
                        placed = 1;
                    }
                }
                break;
            }
        }
        pthread_mutex_unlock(&game_state->lock);

        if (new_id != -1) {
            *player_id = new_id;
            send_packet(client_fd, OP_LOGIN_RESP, &new_id, sizeof(int));
            printf("Player %d logged in.\n", new_id);
        } else {
            // Server full
            send_packet(client_fd, OP_ERROR, "Server Full", 11);
            close(client_fd);
            *player_id = -1;
        }
    } else if (opcode == OP_MOVE && *player_id >= 0) {
        char dir = *((char*)payload);
        pthread_mutex_lock(&game_state->lock);
        
        if (game_state->active_players[*player_id] && game_state->snakes[*player_id].alive) {
            // Prevent 180 turn
            char current = game_state->snakes[*player_id].direction;
            if (!((current == DIR_UP && dir == DIR_DOWN) ||
                  (current == DIR_DOWN && dir == DIR_UP) ||
                  (current == DIR_LEFT && dir == DIR_RIGHT) ||
                  (current == DIR_RIGHT && dir == DIR_LEFT))) {
                game_state->snakes[*player_id].direction = dir;
            }
        }
        pthread_mutex_unlock(&game_state->lock);
    } else if (opcode == OP_HEARTBEAT) {
        // Respond with heartbeat ACK
        send_packet(client_fd, OP_HEARTBEAT_ACK, NULL, 0);
    } else if (opcode == OP_LOGOUT && *player_id >= 0) {
        // Client requested logout
        pthread_mutex_lock(&game_state->lock);
        game_state->active_players[*player_id] = 0;
        for(int y=0; y<MAP_HEIGHT; y++) {
            for(int x=0; x<MAP_WIDTH; x++) {
                if(game_state->map[y][x] == CELL_PLAYER_BASE + *player_id) {
                    game_state->map[y][x] = CELL_EMPTY;
                }
            }
        }
        pthread_mutex_unlock(&game_state->lock);
        printf("Player %d logged out.\n", *player_id);
        close(client_fd);
        *player_id = -1;
    }

    if (payload) free(payload);
}

void worker_process(int worker_id) {
    fd_set readfds, masterfds;
    int max_fd = server_fd;
    int client_ids[FD_SETSIZE]; // Map fd to player_id
    uint64_t client_versions[FD_SETSIZE]; // Track last sent version
    time_t client_last_activity[FD_SETSIZE]; // Track last activity for timeout

    for (int i = 0; i < FD_SETSIZE; i++) {
        client_ids[i] = -1;
        client_versions[i] = 0;
        client_last_activity[i] = 0;
    }

    FD_ZERO(&masterfds);
    FD_SET(server_fd, &masterfds);

    printf("Worker %d started.\n", worker_id);

    while (1) {
        readfds = masterfds;
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 50000; // 50ms

        int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        if (activity == -1) {
            if (errno == EINTR) continue;
            perror("select");
            exit(1);
        }

        time_t now = time(NULL);

        // Check for timeouts and updates to send
        uint64_t current_version = 0;
        pthread_mutex_lock(&game_state->lock);
        current_version = game_state->version;
        pthread_mutex_unlock(&game_state->lock);

        for (int i = 0; i <= max_fd; i++) {
            if (i != server_fd && FD_ISSET(i, &masterfds)) {
                // Check for client timeout
                if (client_last_activity[i] > 0 && 
                    (now - client_last_activity[i]) > CLIENT_TIMEOUT_SEC) {
                    printf("Worker %d: Client fd %d timed out.\n", worker_id, i);
                    if (client_ids[i] >= 0) {
                        pthread_mutex_lock(&game_state->lock);
                        game_state->active_players[client_ids[i]] = 0;
                        for(int y=0; y<MAP_HEIGHT; y++) {
                            for(int x=0; x<MAP_WIDTH; x++) {
                                if(game_state->map[y][x] == CELL_PLAYER_BASE + client_ids[i]) {
                                    game_state->map[y][x] = CELL_EMPTY;
                                }
                            }
                        }
                        pthread_mutex_unlock(&game_state->lock);
                    }
                    close(i);
                    FD_CLR(i, &masterfds);
                    client_ids[i] = -1;
                    client_last_activity[i] = 0;
                    continue;
                }

                if (client_ids[i] != -1) {
                    // Check if player is dead
                    if (game_state->active_players[client_ids[i]] == 0) {
                         send_packet(i, OP_DIE, NULL, 0);
                         close(i);
                         FD_CLR(i, &masterfds);
                         client_ids[i] = -1;
                         client_last_activity[i] = 0;
                         continue;
                    }

                    if (client_versions[i] < current_version) {
                        int map_copy[MAP_HEIGHT][MAP_WIDTH];
                        pthread_mutex_lock(&game_state->lock);
                        memcpy(map_copy, game_state->map, sizeof(game_state->map));
                        pthread_mutex_unlock(&game_state->lock);
                        
                        if (send_packet(i, OP_UPDATE, map_copy, sizeof(map_copy)) == -1) {
                             // Error sending, maybe close?
                        } else {
                            client_versions[i] = current_version;
                        }
                    }
                }
            }
        }

        if (activity > 0) {
            for (int i = 0; i <= max_fd; i++) {
                if (FD_ISSET(i, &readfds)) {
                    if (i == server_fd) {
                        // New connection
                        struct sockaddr_in client_addr;
                        socklen_t addr_len = sizeof(client_addr);
                        int new_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
                        if (new_fd == -1) {
                            perror("accept");
                        } else {
                            FD_SET(new_fd, &masterfds);
                            if (new_fd > max_fd) max_fd = new_fd;
                            client_last_activity[new_fd] = time(NULL);
                            printf("Worker %d accepted new connection (fd=%d).\n", worker_id, new_fd);
                        }
                    } else {
                        // Handle client data
                        int pid = client_ids[i];
                        handle_client_message(i, &pid);
                        client_ids[i] = pid; // Update ID (in case of login)
                        client_last_activity[i] = time(NULL);  // Update last activity
                        
                        if (pid == -1 && client_ids[i] == -1) { 
                            // Disconnected or died
                            FD_CLR(i, &masterfds);
                            client_last_activity[i] = 0;
                        }
                    }
                }
            }
        }
    }
}

int main() {
    signal(SIGINT, handle_sigint);

    // Create Shared Memory
    key_t key = ftok(SHM_KEY_FILE, SHM_KEY_ID);
    shmid = shmget(key, sizeof(GameState), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    game_state = (GameState *)shmat(shmid, NULL, 0);
    if (game_state == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    init_game_map();

    // Create Socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(server_fd, 100) < 0) {
        perror("listen");
        exit(1);
    }

    printf("Server listening on port %d\n", PORT);

    // Prefork Workers
    for (int i = 0; i < NUM_WORKERS; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            worker_process(i);
            exit(0);
        } else if (pid > 0) {
            workers[i] = pid;
        } else {
            perror("fork");
            exit(1);
        }
    }

    // Fork Game Loop
    pid_t pid = fork();
    if (pid == 0) {
        game_tick_loop();
        exit(0);
    } else if (pid > 0) {
        game_loop_pid = pid;
    } else {
        perror("fork game loop");
        exit(1);
    }

    // Master waits
    while (running) {
        pause();
    }

    return 0;
}
