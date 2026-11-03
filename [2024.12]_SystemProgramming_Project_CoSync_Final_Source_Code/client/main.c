/**
 * @author 2023016057 조신근
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "includes/common.h"
#include "includes/receive_handler.h"
#include "includes/send_handler.h"
#include "includes/file_monitor.h"

// #define DEBUG

void print_init_logo();
void print_centered(const char *text, int width);
void print_right_aligned(const char *name, const char *message, int flag);
void get_terminal_size(int *cols);

void enqueue(Packet packet);
int dequeue(Packet *packet);

int main()
{
    int bytes_received;

    ////////////////////////////////// 1번 소켓 연결 //////////////////////////////////////////
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0)
    {
        // perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    // server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_addr.s_addr = inet_addr("0.0.0.0"); // IP

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Connection failed\n");
        exit(EXIT_FAILURE);
    }
    ////////////////////////////////////// 클라이언트에 공유 폴더, 파일 초기화 ///////////////////////////////////////////////

    // watch directory가 없으면 생성
    struct stat st = {0};
    if (stat("./watch", &st) == -1)
    {
        if (mkdir("./watch", 0700) < 0)
        {
            // perror("mkdir");
            return -1;
        }
    }

    FILE *fp;
    fp = fopen(SHARED_FILE, "a");
    if (!fp)
    {
        // printf("초기 공유 파일 생성 실패");
        return -1;
    }
    fclose(fp);

    ///////////////////////////////////// 3번 로그인 /////////////////////////////////////////////
    FILE *file = fopen(USER_FILE, "r");
    Packet login_packet;
    char buffer[BUFFER_SIZE];
    if (file)
    { // user.txt 파일이 이미 있으면 저장된 유저 정보 사용
        fgets(username, sizeof(username), file);
        username[strcspn(username, "\n")] = '\0';
        // 서버에 이름 전송
        login_packet.flag = 1; // 로그인 플래그를 사용하지 않음으로 이미 회원가입이 되있음을 표시
        strncpy(login_packet.username, username, sizeof(login_packet.username));
        pthread_mutex_lock(&send_mutex);
        if (send_all(client_socket, &login_packet, sizeof(Packet), 0) < 0)
        {
            // perror("Failed to send username");
            exit(EXIT_FAILURE);
        }
        print_init_logo();
        printf(">> Login Complete\n");
        puts("");
        pthread_mutex_unlock(&send_mutex);
        fclose(file);
    }
    else
    { // user.txt 파일이 없음 => 회원가입
        // 사용자 이름 입력
        print_init_logo();
        while (1)
        {   
            printf(">> Enter your username: ");
            fgets(username, sizeof(username), stdin);
            username[strcspn(username, "\n")] = '\0';

            // 서버에 이름 전송
            login_packet.flag = 0; // 로그인 플래그
            strncpy(login_packet.username, username, sizeof(login_packet.username));
            pthread_mutex_lock(&send_mutex);
            if (send_all(client_socket, &login_packet, sizeof(Packet), 0) < 0)
            {
                // perror("Failed to send username");
                continue;
            }
            pthread_mutex_unlock(&send_mutex);

            // 서버 응답 대기 (회원가입 시도 중)
            bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received < 0)
            {
                // perror("Failed to receive data");
                exit(EXIT_FAILURE);
            }
            buffer[bytes_received] = '\0';

            if (strcmp(buffer, "REGISTERED") == 0)
            {
                // 이제 초기 데이터를 요청
                pthread_mutex_lock(&send_mutex);
                if (send(client_socket, "INIT_REQUEST", 12, 0) < 0)
                {
                    // perror("failed to send init request");
                };
                pthread_mutex_unlock(&send_mutex);

                file = fopen(USER_FILE, "w");
                if (file)
                {
                    fprintf(file, "%s\n", username);
                    fclose(file);
                }
                printf(">> Registration successful.\n");
                puts("");
                break;
            }
            else if (strcmp(buffer, "DUPLICATE") == 0)
            {
                printf("Username already exists. Try another.\n");
            }
            else
            {
                // printf("Error communicating with server.\n");
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////////// 4번 채팅내역 & 공유파일 불러오기 ///////////////////////////////
    Packet received_packet1 = {0};
    Packet received_packet2 = {0};
    Packet chatLog_packet;    // 불러온 채팅로그 저장할 패킷
    Packet sharedFile_packet; // 불러온 공유파일 저장할 패킷
    // 채팅 내역 수신
    bytes_received = recv_all(client_socket, &received_packet1, sizeof(Packet), 0);
    if (bytes_received <= 0)
    {
        perror("Failed to receive chat history");
        exit(EXIT_FAILURE);
    }

    if (received_packet1.flag == 1)
    { // 채팅 메시지
        chatLog_packet = received_packet1;
    }
    else
    {
        // printf("Unexpected packet type for chat history\n");
    }

    // 공유 파일 수신
    bytes_received = recv_all(client_socket, &received_packet2, sizeof(Packet), 0);
    // printf("packet_flag : %d\n", received_packet2.flag);
    // printf("packet_flag : %02X ", received_packet2.flag);
    // printf("packet_file_data : %s\n", received_packet2.file_data);

    if (bytes_received <= 0)
    {
        perror("Failed to receive shared file");
        exit(EXIT_FAILURE);
    }
    if (received_packet2.flag == 2)
    { // 파일 데이터
        sharedFile_packet = received_packet2;
        // printf("[File Update] Received file data\n");
        // 공유 파일에 파일 데이터를 저장
        pthread_mutex_lock(&file_mutex);
        FILE *shared_file = fopen(SHARED_FILE, "w");
        if (shared_file)
        {
            fwrite(sharedFile_packet.file_data, sizeof(char), strlen(sharedFile_packet.file_data), shared_file);
            fclose(shared_file);
        }
        else
        {
            // perror("Failed to open shared file");
        }
        pthread_mutex_unlock(&file_mutex);
    }
    else
    {
        // printf("Unexpected packet type for shared file\n");
    }
    //////////////////////////////////////////////////////////////////////////////////////////////

    //////////////////////// 5번 불러온 채팅내역 UI로 표시 ////////////////////////////
    //
    // UI 띄우기

    char temp_input[strlen(chatLog_packet.message) + 1];
    strncpy(temp_input, chatLog_packet.message, sizeof(temp_input) - 1);
    temp_input[sizeof(temp_input) - 1] = '\0';

    // Split the input into lines
    char *line = strtok(temp_input, "\n");
    int cols;
    get_terminal_size(&cols);

    for (int i = 0; i < cols / 2 - 5; i++)
        printf("-");
    printf("[ChatLog]");
    for (int i = 0; i < cols / 2 - 4; i++)
        printf("-");
    printf("\n");

    while (line != NULL)
    {
        // Parse input into name and message
        char temp[BUFFER_SIZE];
        strncpy(temp, line, sizeof(temp) - 1);
        temp[sizeof(temp) - 1] = '\0';

        char *comma = strchr(temp, ',');
        if (comma == NULL)
        {
            line = strtok(NULL, "\n");
            continue; // Skip invalid input
        }

        *comma = '\0'; // Split name and message
        char *name = temp;
        char *message = comma + 1;

        // Determine if it's the current user's message
        if (strcmp(name, username) != 0)
        {
            print_right_aligned(name, message, 0);
        }
        else
        {
            printf("[%s] : %s\n", name, message);
        }

        line = strtok(NULL, "\n");
    }
    for (int i = 0; i < cols; i++)
        printf("-");
    printf("\n\n");
    // printf("Temp ChatLog & SharedFile\n");
    //
    //////////////////////////////////////////////////////////////////////////////////////////////

    //////////////////////////// 2번 thread [서버 패킷 수신] 생성 ////////////////////////////////
    pthread_t thread_receive_server_packet; // 서버로부터 패킷을 받는 스레드
    pthread_create(&thread_receive_server_packet, NULL, receive_server_packet, NULL);
    //////////////////////////////////////////////////////////////////////////////////////////////

    ///////////////////////// 6번 thread [터미널 입력 및 패킷 발신] 생성 /////////////////////////
    pthread_t thread_send_terminal_packet; // 서버로 터미널 입력 패킷을 보내는 스레드
    pthread_create(&thread_send_terminal_packet, NULL, send_terminal_packet, NULL);
    //////////////////////////////////////////////////////////////////////////////////////////////

    /////////////////// 7번 thread [클라이언트 파일 변경 감지 및 파일 전송] 생성 /////////////////
    pthread_t thread_inotify_file_and_send_packet; // 클라이언트 파일 변경을 감지하고 변경 시 파일을 전송하는 스레드
    pthread_create(&thread_inotify_file_and_send_packet, NULL, watch_file, NULL);
    //////////////////////////////////////////////////////////////////////////////////////////////

    //////////////////////////////////// 8번 메인 스레드 작업 ////////////////////////////////////
    while (keep_running)
    {
        Packet current_work = {0};
        // 작업 큐에서 패킷을 읽고 채팅인지 파일인지 확인
        if (dequeue(&current_work))
        {
#ifdef DEBUG
            printf("packet dequeue [flag]: %d\n", current_work.flag);
            printf("packet dequeue [username]: %s\n", current_work.username);
            printf("packet dequeue [message]: %s\n", current_work.message);
            printf("packet dequeue [file_data]: %s\n", current_work.file_data);
#endif
            if (current_work.flag == 1)
            { // 채팅 메시지
              //
              // 채팅 UI 띄우기
                printf("\033[2K\r"); // 현재 줄을 지우고 커서를 행의 시작으로 복귀
                print_right_aligned(current_work.username, current_work.message, 1);
                printf("[%s] : ", username);
                fflush(stdout);
                //
            }
            else if (current_work.flag == 2)
            { // 파일 데이터
                // 파일 내용 비교 후 동일하면 스킵
                if (is_file_data_same(SHARED_FILE, current_work.file_data))
                {
                    // printf("[File Update] 파일 내용이 동일하므로 업데이트를 건너뜁니다.\n");
                    continue; // 다시 루프 시작
                }

                // printf("[File Update] Applying file data update\n");
                apply_to_file(SHARED_FILE, &current_work);
            }
            else if (current_work.flag == 3)
            {
                //
                // version log 띄우기
                puts("");

                for (int i = 0; i < cols / 2 - 7; i++)
                    printf("-");
                printf("[Version_Log]");
                for (int i = 0; i < cols / 2 - 6; i++)
                    printf("-");
                printf("\n");
                if (fwrite(&current_work.file_data, sizeof(char), strlen(current_work.file_data), stdout) < 0)
                {
                    // perror("fwrite");
                    fclose(file);
                    return -1;
                }
                for (int i = 0; i < cols; i++)
                    printf("-");
                printf("\n\n");

                printf("[%s] : ", username);
                fflush(stdout);
                //
            }
            else
            {
#ifdef DEBUG
                printf("Unknown packet type in queue [flag]: %d\n", current_work.flag);
                printf("Unknown packet type in queue [username]: %s\n", current_work.username);
                printf("Unknown packet type in queue [message]: %s\n", current_work.message);
                printf("Unknown packet type in queue [file_data]: %s\n", current_work.file_data);
#endif
            }
        }

        if (!keep_running)
        {
            pthread_join(thread_receive_server_packet, NULL);
            pthread_join(thread_send_terminal_packet, NULL);
            pthread_join(thread_inotify_file_and_send_packet, NULL);
            pthread_mutex_destroy(&global_mutex);
            pthread_mutex_destroy(&send_mutex);
            pthread_mutex_destroy(&file_mutex);
            pthread_mutex_destroy(&queue_mutex);
            return 0;
        }
        usleep(10000); // 잠시 대기하여 CPU 사용률 감소 (GPT 추천인데 실제로 써봐야 알 거 같음)
    }
    //////////////////////////////////////////////////////////////////////////////////////////////

    return 0;
}

void print_init_logo()
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int term_width = w.ws_col; // 터미널 너비 가져오기

    // 상단 구분선
    printf("%.*s\n", 102, "==========================================================================================================================================\n");
    
    // 로고 출력 (백슬래시 이스케이프 수정)
    print_centered(" ____               ____                                   ", term_width);
    print_centered("/\\  _`\\            /\\  _`\\                                 ", term_width);
    print_centered("\\ \\ \\\\\\/_\\    ___   \\ \\,\\L\\_\\    __  __      ___      ___   ", term_width);
    print_centered(" \\ \\ \\/_/_  / __`\\  \\/_\\__ \\   /\\ \\\\ /\\ \\   /' _ `\\   /'___\\ ", term_width);
    print_centered("  \\ \\ \\L\\ \\\\ \\ \\L\\ \\   /\\ \\L\\ \\ \\ \\ \\_\\ \\  /\\ \\/\\ \\ /\\ \\__/ ", term_width);
    print_centered("   \\ \\____/\\ \\____/   \\ `\\____\\ \\/`____ \\ \\ \\_\\ \\_\\\\ \\____\\", term_width);
    print_centered("    \\/___/  \\/___/     \\/_____/  `/___/> \\ \\/_/\\/_/ \\/____/", term_width);
    print_centered("                                    /\\___/                 ", term_width);
    print_centered("                                    \\/__/                  ", term_width);
    print_centered("", term_width);
    
    // 하단 구분선
    printf("%.*s\n", 102, "==========================================================================================================================================\n");
    
    // 환영 메시지 및 정보
    print_centered("                 Welcome to the CoSync!                     ", term_width);
    printf("%.*s\n", 102, "==========================================================================================================================================\n");
    print_centered("              Version      : 1.0.0                          ", term_width);
    print_centered("              Author       : Third Team                     ", term_width);
    print_centered("              Description  : File Sync with chat            ", term_width);
    print_centered("              Date Created : 2024-12-13                     ", term_width);
    printf("%.*s\n", 102, "==========================================================================================================================================\n");
    puts("");
    printf(">> Insert \"help\" if you want help\n");
    printf(">> Hello! %s\n", username);
}

void get_terminal_size(int *cols)
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    *cols = w.ws_col;
}

// Function to print right-aligned message
void print_right_aligned(const char *name, const char *message, int flag)
{
    int cols;
    get_terminal_size(&cols);

    char formatted_message[BUFFER_SIZE + 50 + 4];
    snprintf(formatted_message, sizeof(formatted_message), "%s : [%s]", message, name);

    int padding = cols - strlen(formatted_message);

    // if (flag)
    //     padding -= (strlen(username) + 5);

    if (padding > 0)
    {
        for (int i = 0; i < padding; i++)
        {
            printf(" ");
        }
    }
    printf("%s\n", formatted_message);
}