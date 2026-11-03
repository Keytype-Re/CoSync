/**
 * @author 2023016057 조신근
 */

#include "common.h"
#include <string.h>
#include <stdio.h>

// 전역 변수 초기화
pthread_mutex_t global_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

int keep_running = 1;

// 작업큐
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
Packet packet_queue[QUEUE_SIZE];
int front = 0;
int rear = 0;
void enqueue(Packet packet) {
    pthread_mutex_lock(&queue_mutex);
    if ((rear + 1) % QUEUE_SIZE == front) {
        printf("Queue is full\n");
    } else {
        packet_queue[rear] = packet;
        rear = (rear + 1) % QUEUE_SIZE;
    }
    pthread_mutex_unlock(&queue_mutex);
}

int dequeue(Packet *packet) {
    pthread_mutex_lock(&queue_mutex);
    if (front == rear) {
        pthread_mutex_unlock(&queue_mutex);
        return 0;
    }
    *packet = packet_queue[front];
    front = (front + 1) % QUEUE_SIZE;
    pthread_mutex_unlock(&queue_mutex);
    return 1;
}

// 전역 변수 정의
int client_socket;
struct sockaddr_in server_addr;
char username[50];


ssize_t send_all(int sockfd, const void *buffer, size_t length, int a) {
    size_t total_sent = 0;
    const char *ptr = (const char*)buffer;

    while (total_sent < length) {
        ssize_t sent = send(sockfd, ptr + total_sent, length - total_sent, 0);
        if (sent <= 0) {
            // 오류 발생 또는 연결 종료
            return -1;
        }
        total_sent += sent;
    }
    return total_sent;
}

ssize_t recv_all(int sockfd, void *buffer, size_t length, int a) {
    size_t total_received = 0;
    char *ptr = (char*)buffer;

    while (total_received < length) {
        ssize_t received = recv(sockfd, ptr + total_received, length - total_received, 0);
        if (received <= 0) {
            // 오류 발생 또는 연결 종료
            return -1;
        }
        total_received += received;
    }
    return total_received;
}