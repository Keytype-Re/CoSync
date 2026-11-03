/**
 * @author 2023016057 조신근
 */

#ifndef COMMON_H
#define COMMON_H

#include <pthread.h>
#include <netinet/in.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define USER_FILE "user.txt"
#define SHARED_FILE "./watch/shared_file.txt"
#define FILE_NAME_SIZE 64
#define FILE_PATH_SIZE 128

// 패킷 구조체 정의
typedef struct
{
    int flag;                     // 데이터 유형 플래그: 0 = 로그인, 1 = 채팅, 2 = 파일, 3 = 명령어
    char username[50];            // 사용자 이름
    char message[BUFFER_SIZE];    // 채팅 메시지
    char file_data[BUFFER_SIZE];  // 파일 데이터
} Packet;

// 작업 큐 정의
#define QUEUE_SIZE 100
extern pthread_mutex_t queue_mutex;
extern Packet packet_queue[QUEUE_SIZE];
extern int front;
extern int rear;

/**
 * @brief 패킷을 작업 큐에 추가하는 함수.
 * 
 * 이 함수는 주어진 패킷을 작업 큐에 추가함. 큐가 가득 차면 패킷을 추가하지 않음.
 * 
 * @param packet 작업 큐에 추가할 패킷.
 */
void enqueue(Packet packet);

/**
 * @brief 작업 큐에서 패킷을 제거하는 함수.
 * 
 * 이 함수는 작업 큐에서 패킷을 제거하고, 제거된 패킷을 주어진 포인터에 저장함.
 * 
 * @param packet 제거된 패킷을 저장할 포인터.
 * @return 큐가 비어 있으면 0을 반환하고, 그렇지 않으면 1을 반환함.
 */
int dequeue(Packet *packet);

// 전역 변수 선언
extern int keep_running;
extern pthread_mutex_t global_mutex;
extern pthread_mutex_t send_mutex;
extern pthread_mutex_t file_mutex;

extern int client_socket;
extern struct sockaddr_in server_addr;
extern char username[50];

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