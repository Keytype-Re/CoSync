/**
 * @author 2023015097 도현민
 */

#include "common.h"
#include <pthread.h>
#include <netinet/in.h> // sockaddr_in 사용을 위해 필요함

// 작업 큐 정의
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
Packet packet_queue[QUEUE_SIZE];
int front = 0;
int rear = 0;

// 전역 변수 정의
int keep_running = 1;
Client clients[MAX_CLIENTS];                               // 클라이언트 정보 배열
int client_count = 0;                                      // 연결된 클라이언트 수
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; // 클라이언트 배열 보호 뮤텍스
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;    // 파일 작업 보호 뮤텍스
pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;    // send()시 소켓 보호 뮤텍스
int server_fd;                                             // 서버 소켓 파일 디스크립터
struct sockaddr_in server_addr;                            // 서버 주소 구조체

ssize_t send_all(int sockfd, const void *buffer, size_t length, int a)
{
    size_t total_sent = 0;
    const char *ptr = (const char *)buffer;

    while (total_sent < length)
    {
        ssize_t sent = send(sockfd, ptr + total_sent, length - total_sent, 0);
        if (sent <= 0)
        {
            // 오류 발생 또는 연결 종료
            return -1;
        }
        total_sent += sent;
    }
    return total_sent;
}

ssize_t recv_all(int sockfd, void *buffer, size_t length, int a)
{
    size_t total_received = 0;
    char *ptr = (char *)buffer;

    while (total_received < length)
    {
        ssize_t received = recv(sockfd, ptr + total_received, length - total_received, 0);
        if (received <= 0)
        {
            // 오류 발생 또는 연결 종료
            return -1;
        }
        total_received += received;
    }
    return total_received;
}
