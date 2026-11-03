/**
 * @author 2023015097 도현민
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include "includes/common.h"
#include "includes/login_control.h"
#include "includes/broadcast.h"
#include "includes/file_sync.h"
#include "includes/chat_handler.h"
#include "includes/version_control.h"

/**
 * @brief 서버를 초기화하는 함수.
 *
 * 이 함수는 주어진 포트 번호로 서버 소켓을 생성하고 바인딩하며, 클라이언트 연결을 대기함.
 *
 * @param port 서버가 바인딩할 포트 번호.
 * @param server_addr 서버 주소 구조체 포인터.
 * @return 성공 시 서버 소켓 파일 디스크립터를 반환하고, 실패 시 -1을 반환함.
 */
int initialize_server(int port, struct sockaddr_in *server_addr);

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

/**
 * @brief 클라이언트 소켓을 수락하는 함수.
 *
 * 이 함수는 클라이언트 연결을 수락하고, 각 클라이언트 소켓을 처리하기 위한 새로운 스레드를 생성함.
 *
 * @param arg 사용되지 않는 매개변수.
 * @return NULL
 */
void *accept_socket(void *);

/**
 * @brief 클라이언트로부터 패킷을 수신하는 함수.
 *
 * 이 함수는 클라이언트로부터 패킷을 수신하고, 로그인 절차를 처리한 후 패킷을 큐에 추가함.
 *
 * @param arg 클라이언트 소켓 파일 디스크립터를 가리키는 포인터.
 * @return NULL
 */
void *receive_packet(void *arg);

/**
 * @brief 초기 데이터를 클라이언트에게 전송하는 함수.
 *
 * 이 함수는 클라이언트에게 초기 채팅 로그와 공유 파일 데이터를 전송함.
 *
 * @param client_sock 클라이언트 소켓 파일 디스크립터.
 */
void send_initial_data(int client_sock);

/**
 * @brief 현재 패킷의 소켓 파일 디스크립터를 찾는 함수.
 *
 * 이 함수는 주어진 패킷의 사용자 이름을 기반으로 해당 클라이언트의 소켓 파일 디스크립터를 찾음.
 *
 * @param current_packet 현재 패킷.
 * @return 해당 클라이언트의 소켓 파일 디스크립터를 반환하고, 찾지 못하면 -1을 반환함.
 */
int find_current_sockfd(Packet *);

/**
 * @brief SIGINT 시그널을 처리하는 함수.
 *
 * 이 함수는 SIGINT 시그널을 처리하여 서버를 안전하게 종료함.
 *
 * @param sig 시그널 번호.
 */
void handle_sigint(int sig);

int main()
{
    signal(SIGINT, handle_sigint); // ctrl + c 입력이 들어오면 keep_running 값이 0으로 바뀌고 while문이 종료됩니다.

    // 서버 초기화
    server_fd = initialize_server(PORT, &server_addr);
    if (server_fd < 0)
    {
        fprintf(stderr, "[Error] Server initialization failed.\n");
        return EXIT_FAILURE;
    }
    printf("[Server] Listening on port %d...\n", PORT);

    pthread_t accept_ClientSocket;
    pthread_create(&accept_ClientSocket, NULL, accept_socket, NULL);
    initialize_version_directory();

    Packet current_work;
    while (keep_running)
    {
        // 작업 큐에서 패킷을 읽고 채팅인지 파일인지 확인
        if (dequeue(&current_work))
        {
            if (current_work.flag == 1)
            { // 채팅 메시지
                //
                // 채팅 log 저장 + 채팅 broadcast <- 3번
                handle_chat_message(&current_work, find_current_sockfd(&current_work));
                //
            }
            else if (current_work.flag == 2)
            { // 파일 데이터
                //
                // 파일 내역 읽고 shared_file.txt에 반영 + 파일 broadcast <- 2번
                handle_file_packet(&current_work, find_current_sockfd(&current_work));
                //
            }
            else if (current_work.flag == 3)
            {
                //
                // '/commit', '/log', '/rebase' 명령어 처리 <- 4번
                if (strncmp(current_work.message, "/commit", 7) == 0)
                {
                    commit_version();
                }
                else if (strncmp(current_work.message, "/log", 4) == 0)
                {
                    log_versions(&current_work, find_current_sockfd(&current_work));
                }
                else if (strncmp(current_work.message, "/rebase", 7) == 0)
                {
                    int version_number = atoi(&current_work.message[8]);
                    rebase_version(version_number, &current_work, -1);
                }
                else
                {
                    printf("[Server] Unknown version control command received: %s\n", current_work.message);
                }
            }
            usleep(10000); // 잠시 대기하여 CPU 사용률 감소 (GPT 추천인데 실제로 써봐야 알 거 같음)
        }
    }

    // 모든 스레드가 정상 종료 되기를 기다림
    pthread_join(accept_ClientSocket, NULL);
    for (int i = 0; i < client_count; i++)
    {
        pthread_join(clients[i].thread, NULL);
    }

    // 뮤텍스 자원 해제
    pthread_mutex_destroy(&send_mutex);
    pthread_mutex_destroy(&queue_mutex);
    pthread_mutex_destroy(&clients_mutex);
    printf("[Server] Successfully shut down.\n");

    return 0; // 서버 프로그램 종료
}
// 서버 초기화
int initialize_server(int port, struct sockaddr_in *server_addr)
{
    int server_fd;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("[Server] Socket creation failed");
        return -1;
    }

    server_addr->sin_family = AF_INET;
    server_addr->sin_addr.s_addr = INADDR_ANY;
    server_addr->sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
    {
        perror("[Server] Bind failed");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0)
    {
        perror("[Server] Listen failed");
        close(server_fd);
        return -1;
    }

    return server_fd;
}

void *accept_socket(void *arg)
{
    int new_sock;
    struct sockaddr_in client_addr;
    socklen_t addr_size = sizeof(client_addr);

    while (1)
    {
        new_sock = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (new_sock < 0)
        {
            if (!keep_running)
            {
                // printf("accept_socket 스레드 종료\n");
                break;
            }
            // [Server] accept 호출 실패 시 오류 메시지 출력
            perror("[Server] Accept failed");
        }

        int *client_sock_ptr = malloc(sizeof(int));
        if (client_sock_ptr == NULL)
        {
            perror("[Server] malloc failed");
            free(client_sock_ptr);
            close(new_sock);
            continue;
        }
        *client_sock_ptr = new_sock;

        // 클라이언트 소켓을 처리하기 위한 새로운 스레드 생성
        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, receive_packet, client_sock_ptr) != 0)
        {
            perror("[Server] Failed to create client thread");
            close(new_sock);
            continue;
        }
        pthread_detach(client_thread); // 스레드를 분리하여 리소스를 자동으로 정리할 수 있도록 설정
    }

    return NULL;
}

void *receive_packet(void *arg)
{
    int client_sock = *(int *)arg;
    free(arg);
    Packet packet;
    int bytes_received;
    Client temp_client; // 해당 쓰레드 전용 클라이언트 구조체
    temp_client.sockfd = client_sock;
    temp_client.thread = pthread_self();
    int is_logged_in = 0; // 로그인 상태 변수

    // 로그인 절차 루프
    while (!is_logged_in)
    {
        bytes_received = recv_all(client_sock, &packet, sizeof(Packet), 0);
        if (bytes_received <= 0)
        {
            if (bytes_received == 0)
                printf("[Server] Client disconnected before login\n"); // 로그인 전 클라이언트와의 연결 끊김
            else
                perror("[Server] Receive failed (before login)"); // 로그인 전 패킷 리시브 오류
            close(client_sock);
            return NULL;
        }

        strncpy(temp_client.username, packet.username, sizeof(temp_client.username));
        temp_client.username[sizeof(temp_client.username) - 1] = '\0'; // Null-termination 보장

        if (packet.flag == 0)
        {
            // 신규 유저 등록 로직
            pthread_mutex_lock(&clients_mutex);
            int duplicate = is_user_registered(temp_client.username);
            if (duplicate)
            {
                // 실패 응답 전송
                pthread_mutex_lock(&send_mutex);
                send(client_sock, "DUPLICATE", 9, 0);
                pthread_mutex_unlock(&send_mutex);
                pthread_mutex_unlock(&clients_mutex);
                continue; // 다시 로그인 패킷 대기
            }
            else
            {
                // user.txt에 저장
                register_user(temp_client.username);
                // 클라이언트 배열에 추가
                clients[client_count++] = temp_client;
                is_logged_in = 1; // 로그인 성공
                pthread_mutex_unlock(&clients_mutex);

                // 성공 응답 전송
                pthread_mutex_lock(&send_mutex);
                send(client_sock, "REGISTERED", 10, 0); // 등록 성공 응답
                pthread_mutex_unlock(&send_mutex);

                // 이제 클라이언트로부터 INIT_REQUEST를 기다림
                char init_buffer[BUFFER_SIZE];
                int init_bytes;
                init_bytes = recv(client_sock, init_buffer, sizeof(init_buffer) - 1, 0);

                init_buffer[init_bytes] = '\0';

                // 클라이언트가 "INIT_REQUEST"를 보냈는지 확인
                if (strcmp(init_buffer, "INIT_REQUEST") == 0)
                {
                    // 이제 초기 데이터 전송
                    send_initial_data(client_sock);
                }
                else
                {
                    printf("[Server] Unexpected message from client: %s\n", init_buffer);
                    // 적절한 에러 처리
                }
                printf("[Server] New user registered: %s\n", temp_client.username);
            }
        }
        else
        {
            // 기존 유저 로그인 로직 (예: flag=1로 기존 유저 입장이라고 가정)
            pthread_mutex_lock(&clients_mutex);
            // 유저가 등록된 유저인지 확인 (is_user_registered 활용)
            int known_user = is_user_registered(temp_client.username);
            if (!known_user)
            {
                // 등록되지 않은 유저가 기존 유저 플래그로 접속 시도한 경우 에러 처리
                pthread_mutex_unlock(&clients_mutex);
                pthread_mutex_lock(&send_mutex);
                send(client_sock, "NOT_REGISTERED", 14, 0);
                pthread_mutex_unlock(&send_mutex);
                continue; // 다시 로그인 대기
            }

            // 클라이언트 배열에 추가
            clients[client_count++] = temp_client;
            is_logged_in = 1;
            pthread_mutex_unlock(&clients_mutex);

            // 초기 데이터 전송
            send_initial_data(client_sock);
            printf("[Server] Existing user connected: %s\n", temp_client.username);
        }
    }

    // 로그인 완료 후 패킷 처리 루프
    while (1)
    {
        bytes_received = recv_all(client_sock, &packet, sizeof(Packet), 0);
        if (bytes_received <= 0)
        {
            if (bytes_received == 0)
                printf("[Server] Client '%s' disconnected gracefully\n", temp_client.username);
            else
                perror("[Server] Receive failed after login");

            remove_client(client_sock);
            break;
        }

        // 이제는 해당 클라이언트 패킷만 처리 -> enqueue
        enqueue(packet);
        printf("[Server] Enqueued packet from client '%s'\n", temp_client.username);
    }

    return NULL;
}

// 초기 데이터 전송 함수
void send_initial_data(int client_sock)
{
    Packet packet;
    memset(&packet, 0, sizeof(Packet));

    // 채팅 로그 전송
    pthread_mutex_lock(&file_mutex);
    FILE *log_file = fopen(CHAT_LOG, "r");
    if (log_file)
    {
        if (fread(packet.message, 1, sizeof(packet.message), log_file) == 0)
        {
            // 파일은 열렸으나 내용 없음
            packet.flag = 1;
            packet.message[0] = '\0';
        }
        else
        {
            // 내용 있음
            packet.flag = 1;
        }
        fclose(log_file);
    }
    else
    {
        // 파일 자체가 없음
        packet.flag = 1;
        packet.message[0] = '\0';
    }
    pthread_mutex_lock(&send_mutex);
    send_all(client_sock, &packet, sizeof(Packet), 0);
    pthread_mutex_unlock(&send_mutex);
    pthread_mutex_unlock(&file_mutex);

    // 공유 파일 전송
    memset(&packet, 0, sizeof(Packet));
    pthread_mutex_lock(&file_mutex);
    FILE *shared_file = fopen(SHARED_FILE, "r");
    if (shared_file)
    {
        if (fread(packet.file_data, 1, sizeof(packet.file_data), shared_file) == 0)
        {
            // 파일은 열렸으나 내용 없음
            packet.flag = 2;
            packet.file_data[0] = '\0';
        }
        else
        {
            // 내용 있음
            packet.flag = 2;
        }
        fclose(shared_file);
    }
    else
    {
        // 파일 자체가 없음
        packet.flag = 2;
        packet.file_data[0] = '\0';
    }
    pthread_mutex_lock(&send_mutex);
    send_all(client_sock, &packet, sizeof(Packet), 0);
    pthread_mutex_unlock(&send_mutex);
    pthread_mutex_unlock(&file_mutex);

    printf("Success send_all Initial Data\n");
}

void enqueue(Packet packet)
{
    pthread_mutex_lock(&queue_mutex);
    if ((rear + 1) % QUEUE_SIZE == front)
    {
        printf("Queue is full\n");
    }
    else
    {
        packet_queue[rear] = packet;
        rear = (rear + 1) % QUEUE_SIZE;
    }
    pthread_mutex_unlock(&queue_mutex);
}

int dequeue(Packet *packet)
{
    pthread_mutex_lock(&queue_mutex);
    if (front == rear)
    {
        pthread_mutex_unlock(&queue_mutex);
        return 0;
    }
    *packet = packet_queue[front];
    front = (front + 1) % QUEUE_SIZE;
    pthread_mutex_unlock(&queue_mutex);
    return 1;
}

int find_current_sockfd(Packet *current_packet)
{
    int temp_socketfd;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (strcmp(clients[i].username, current_packet->username) == 0)
        {
            temp_socketfd = clients[i].sockfd;
            pthread_mutex_unlock(&clients_mutex);
            return temp_socketfd;
        }
    }

    printf("can't find client_sockfd");
    pthread_mutex_unlock(&clients_mutex);
    return -1;
}

void handle_sigint(int sig)
{
    keep_running = 0;
    shutdown(server_fd, SHUT_RDWR); // 소켓 닫기 시그널 전송
    close(server_fd);
}