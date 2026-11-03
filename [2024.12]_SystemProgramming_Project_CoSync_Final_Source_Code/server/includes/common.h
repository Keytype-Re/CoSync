/**
 * @author 2023015097 도현민
 */

#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>
#include <netinet/in.h> // sockaddr_in 사용을 위해 필요함

#define FILE_NAME_SIZE 64
#define FILE_PATH_SIZE 512
#define MAX_CLIENTS 10
#define BUFFER_SIZE 4096
#define PORT 8080
#define CHAT_LOG "chat_log.txt"
#define USER_FILE "user.txt"
#define SHARED_FILE "shared_file.txt"
#define VERSION_DIR "version_logs/"

// 데이터 패킷 구조체
typedef struct
{
    int flag;                    // 데이터 유형 플래그
    char username[50];           // 사용자 이름
    char message[BUFFER_SIZE];   // 메시지 또는 파일 이름
    char file_data[BUFFER_SIZE]; // 파일 데이터
} Packet;

// 클라이언트 구조체
typedef struct
{
    int sockfd;
    char username[50];
    pthread_t thread;
} Client;

// 작업 큐 정의
#define QUEUE_SIZE 100
extern pthread_mutex_t queue_mutex;
extern Packet packet_queue[QUEUE_SIZE];
extern int front;
extern int rear;

// 전역 변수 선언
extern int keep_running;
extern Client clients[MAX_CLIENTS];   // 클라이언트 정보 배열
extern int client_count;              // 연결된 클라이언트 수
extern pthread_mutex_t clients_mutex; // 클라이언트 배열 보호 뮤텍스
extern pthread_mutex_t file_mutex;    // 파일 작업 보호 뮤텍스
extern pthread_mutex_t send_mutex;    // send()시 소켓 보호 뮤텍스
extern int server_fd;
extern struct sockaddr_in server_addr;

/**
 * @brief 모든 데이터를 소켓으로 전송하는 함수.
 *
 * 이 함수는 주어진 버퍼의 모든 데이터를 소켓으로 전송함.
 *
 * @param sockfd 소켓 파일 디스크립터.
 * @param buffer 전송할 데이터 버퍼.
 * @param length 전송할 데이터의 길이.
 * @param a 사용되지 않는 매개변수.
 * @return 성공적으로 전송된 바이트 수를 반환하고, 오류 발생 시 -1을 반환함.
 */
ssize_t send_all(int sockfd, const void *buffer, size_t length, int a);

/**
 * @brief 모든 데이터를 소켓으로부터 수신하는 함수.
 *
 * 이 함수는 주어진 버퍼에 소켓으로부터 모든 데이터를 수신함.
 *
 * @param sockfd 소켓 파일 디스크립터.
 * @param buffer 수신할 데이터 버퍼.
 * @param length 수신할 데이터의 길이.
 * @param a 사용되지 않는 매개변수.
 * @return 성공적으로 수신된 바이트 수를 반환하고, 오류 발생 시 -1을 반환함.
 */
ssize_t recv_all(int sockfd, void *buffer, size_t length, int a);

#endif
