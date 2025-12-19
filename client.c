#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <termios.h>
#include <sys/time.h>

#include "common.h"
#include "proto.h"

int sockfd;
int my_id = -1;
int running = 1;
int stress_mode = 0;

// For stress test stats
long total_rtt = 0;
long total_requests = 0;
long successful_connections = 0;
struct timeval stress_start_time, stress_end_time;
pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;

void set_nonblocking_input(int enable) {
    static struct termios oldt, newt;
    if (enable) {
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    } else {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    }
}

void render_map(int map[MAP_HEIGHT][MAP_WIDTH]) {
    printf("\033[H\033[J"); // Clear screen
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            int cell = map[y][x];
            if (cell == CELL_WALL) printf("#");
            else if (cell == CELL_FOOD) printf("@");
            else if (cell == CELL_EMPTY) printf(" ");
            else if (cell == CELL_PLAYER_BASE + my_id) printf("O"); // Me
            else printf("X"); // Others
        }
        printf("\n");
    }
    printf("Player ID: %d | Controls: W/A/S/D | Q to Quit\n", my_id);
}

void *recv_thread_func(void *arg) {
    uint16_t opcode;
    void *payload = NULL;
    uint32_t len;

    while (running) {
        payload = NULL;  // Always initialize before recv_packet
        if (recv_packet(sockfd, &opcode, &payload, &len) < 0) {
            printf("Disconnected from server.\n");
            running = 0;
            break;
        }

        if (opcode == OP_UPDATE) {
            if (!stress_mode) {
                if (len == sizeof(int) * MAP_HEIGHT * MAP_WIDTH) {
                    int (*map)[MAP_WIDTH] = (int (*)[MAP_WIDTH])payload;
                    render_map(map);
                }
            }
        } else if (opcode == OP_DIE) {
            printf("You Died!\n");
            running = 0;
        } else if (opcode == OP_ERROR) {
            printf("Error: %.*s\n", len, (char*)payload);
            running = 0;
        } else if (opcode == OP_HEARTBEAT_ACK) {
            // Server acknowledged our heartbeat - connection is alive
        }

        if (payload) free(payload);
    }
    return NULL;
}

void *heartbeat_thread_func(void *arg) {
    while (running) {
        sleep(HEARTBEAT_INTERVAL_SEC);
        if (running && my_id >= 0) {
            if (send_packet(sockfd, OP_HEARTBEAT, NULL, 0) < 0) {
                printf("Failed to send heartbeat, connection may be lost.\n");
                running = 0;
                break;
            }
        }
    }
    return NULL;
}

void *input_thread_func(void *arg) {
    set_nonblocking_input(1);
    while (running) {
        char c = getchar();
        if (c == 'q' || c == 'Q') {
            running = 0;
            break;
        }
        if (c == 'w' || c == 'a' || c == 's' || c == 'd' || 
            c == 'W' || c == 'A' || c == 'S' || c == 'D') {
            
            char dir = c;
            if (dir >= 'a' && dir <= 'z') dir -= 32; // To upper
            send_packet(sockfd, OP_MOVE, &dir, 1);
        }
    }
    set_nonblocking_input(0);
    return NULL;
}

void *stress_client_thread(void *arg) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return NULL;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return NULL;
    }

    pthread_mutex_lock(&stats_lock);
    successful_connections++;
    pthread_mutex_unlock(&stats_lock);

    // Login
    if (send_packet(sock, OP_LOGIN_REQ, NULL, 0) < 0) {
        close(sock);
        return NULL;
    }
    
    uint16_t opcode;
    void *payload = NULL;
    uint32_t len;

    if (recv_packet(sock, &opcode, &payload, &len) < 0) {
        if (payload) free(payload);
        close(sock);
        return NULL;
    }

    if (opcode == OP_LOGIN_RESP) {
        if (payload) free(payload);
    } else {
        if (payload) free(payload);
        close(sock);
        return NULL;
    }

    char dirs[] = {DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT};
    struct timeval start, end;

    for (int i = 0; i < 50; i++) { // Run for a bit
        char dir = dirs[rand() % 4];
        
        gettimeofday(&start, NULL);
        if (send_packet(sock, OP_MOVE, &dir, 1) < 0) {
            break;
        }
        
        payload = NULL;
        if (recv_packet(sock, &opcode, &payload, &len) < 0) {
            if (payload) free(payload);
            break;
        }
        
        gettimeofday(&end, NULL);
        long rtt = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        
        pthread_mutex_lock(&stats_lock);
        total_rtt += rtt;
        total_requests++;
        pthread_mutex_unlock(&stats_lock);

        if (payload) free(payload);
        
        usleep(100000); // 100ms
    }

    close(sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "-stress") == 0) {
        stress_mode = 1;
        int num_threads = 100;
        if (argc > 2) {
            num_threads = atoi(argv[2]);
            if (num_threads < 1) num_threads = 100;
            if (num_threads > 500) num_threads = 500;
        }
        
        printf("========================================\n");
        printf("  Stress Test - %d Concurrent Clients\n", num_threads);
        printf("========================================\n");
        
        gettimeofday(&stress_start_time, NULL);
        
        pthread_t *threads = malloc(sizeof(pthread_t) * num_threads);
        for (int i = 0; i < num_threads; i++) {
            pthread_create(&threads[i], NULL, stress_client_thread, NULL);
            usleep(20000); // 20ms - stagger thread creation
        }
        for (int i = 0; i < num_threads; i++) {
            pthread_join(threads[i], NULL);
        }
        free(threads);
        
        gettimeofday(&stress_end_time, NULL);
        
        double elapsed_sec = (stress_end_time.tv_sec - stress_start_time.tv_sec) + 
                            (stress_end_time.tv_usec - stress_start_time.tv_usec) / 1000000.0;
        
        printf("\n========================================\n");
        printf("  Stress Test Results\n");
        printf("========================================\n");
        printf("  Concurrent Clients:    %d\n", num_threads);
        printf("  Successful Connections: %ld\n", successful_connections);
        printf("  Total Requests:        %ld\n", total_requests);
        printf("  Total Time:            %.2f seconds\n", elapsed_sec);
        
        if (total_requests > 0) {
            printf("  Avg Latency:           %ld us (%.2f ms)\n", 
                   total_rtt / total_requests, 
                   (double)(total_rtt / total_requests) / 1000.0);
            printf("  Throughput:            %.2f requests/sec\n", 
                   (double)total_requests / elapsed_sec);
        }
        printf("========================================\n");
        return 0;
    }

    // Normal Client
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        return 1;
    }

    // Login
    send_packet(sockfd, OP_LOGIN_REQ, NULL, 0);
    
    uint16_t opcode;
    void *payload = NULL;
    uint32_t len;

    if (recv_packet(sockfd, &opcode, &payload, &len) < 0) {
        printf("Failed to login.\n");
        if (payload) free(payload);
        close(sockfd);
        return 1;
    }

    if (opcode == OP_LOGIN_RESP) {
        my_id = *((int*)payload);
        printf("Logged in as Player %d\n", my_id);
        free(payload);
    } else {
        printf("Login failed: %d\n", opcode);
        if(payload) free(payload);
        close(sockfd);
        return 1;
    }

    pthread_t t1, t2, t3;
    pthread_create(&t1, NULL, input_thread_func, NULL);
    pthread_create(&t2, NULL, recv_thread_func, NULL);
    pthread_create(&t3, NULL, heartbeat_thread_func, NULL);

    pthread_join(t1, NULL);
    running = 0;  // Signal other threads to stop
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    close(sockfd);
    return 0;
}
