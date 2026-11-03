/**
 * @author 2023012083 김무성
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "includes/send_handler.h"

/////////////////////////////////////////////////////////// 스레드 함수 ////////////////////////////////////////////////////////////////
void *send_terminal_packet(void *arg)
{
	while (1)
	{
		char input[BUFFER_SIZE];
		printf("[%s] : ", username);
		fflush(stdout);
		fgets(input, sizeof(input), stdin); // 터미널 입력 받기
		input[strcspn(input, "\n")] = '\0'; // 개행 문자 제거

		// 입력 내용에 따라 분기
		if (strcmp(input, "/new") == 0)
		{
			command_new(); // shared_file.txt 초기화
		}
		else if (strstr(input, "/load") != NULL)
		{
			command_load(input); // 파일 내용 복사 및 shared_file.txt에 붙여넣기
		}
		else if (strcmp(input, "/commit") == 0)
		{
			command_commit(input);
		}
		else if (strstr(input, "/rebase") != NULL)
		{
			command_rebase(input);
		}
		else if (strcmp(input, "/log") == 0)
		{
			command_log(input);
		}
		else if (strcmp(input, "/quit") == 0)
		{
			command_quit(); // 서버와의 연결 종료
		}
		else if (strcmp(input, "/help") == 0)
		{
			command_help(); // help!!!!!!
		}
		else
		{
			send_chat_message(input); // 채팅 메시지 전송
		}

		if (!keep_running)
		{
			return NULL;
		}
	}

	return NULL;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////// 터미널 입력에 따른 분기별 함수 /////////////////////////////////////////////////////////////////////
// /new
void command_new()
{
	// 파일을 쓰기 모드로 열기
	FILE *file = fopen(SHARED_FILE, "w"); // 쓰기 모드는 자동으로 파일 내용을 삭제
	if (file == NULL)
	{
		// perror("[Error] Failed to /new");
		return;
	}

	fclose(file);
	printf("[Client] File '%s' newly created successfully.\n", SHARED_FILE);
}

// /load
void command_load(const char *input)
{
	// 입력에서 파일 경로 추출
	char filepath[BUFFER_SIZE];
	sscanf(input + 6, "%s", filepath); // "/load " 이후 파일 이름 읽기

	printf("[Client] Loading file from path: %s\n", filepath);

	// loaded_file.txt 열기 (읽기 모드)
	FILE *loaded_file = fopen(filepath, "r");
	if (loaded_file == NULL)
	{
		// perror("[Client] Failed to open file");
		return;
	}

	// shared_file.txt 열기 (쓰기 모드)
	FILE *shared_file = fopen(SHARED_FILE, "w");
	if (shared_file == NULL)
	{
		// perror("[Client] Failed to open shared_file.txt");
		fclose(loaded_file);
		return;
	}

	// 파일 내용 복사
	char buffer[BUFFER_SIZE];
	size_t bytes_read;
	pthread_mutex_lock(&file_mutex); // file_mutex lock
	while ((bytes_read = fread(buffer, 1, sizeof(buffer), loaded_file)) > 0)
	{
		fwrite(buffer, 1, bytes_read, shared_file);
		// printf("쓰는 중\n");
	}
	pthread_mutex_unlock(&file_mutex); // file_mutex unlock

	// 파일 스트림 닫기
	fclose(loaded_file);
	fclose(shared_file);

	printf("[Client] File '%s' loaded to 'shared_file.txt' successfully.\n", filepath);
}

// commit
void command_commit(const char *input)
{
	Packet packet;

	memset(&packet, 0, sizeof(Packet));							 // 패킷 메모리 초기화
	packet.flag = 3;											 // 패킷 플래그 설정 (명령어)
	strncpy(packet.username, username, sizeof(packet.username)); // 패킷에 유저 이름 설정
	strncpy(packet.message, input, sizeof(packet.message));		 // "/commit"

	pthread_mutex_lock(&send_mutex);
	if (send_all(client_socket, &packet, sizeof(Packet), 0) < 0)
	{ // 전송
	  // perror("[Client] Failed to /commit");
	}
	pthread_mutex_unlock(&send_mutex); // unlock
}

// rebase
void command_rebase(const char *input)
{
	Packet packet;

	memset(&packet, 0, sizeof(Packet));							 // 패킷 메모리 초기화
	packet.flag = 3;											 // 패킷 플래그 설정 (명령어)
	strncpy(packet.username, username, sizeof(packet.username)); // 패킷에 유저 이름 설정
	strncpy(packet.message, input, sizeof(packet.message));		 // "/rebase (number)"

	pthread_mutex_lock(&send_mutex);
	if (send_all(client_socket, &packet, sizeof(Packet), 0) < 0)
	{ // 전송
	  // perror("[Client] Failed to /rebase");
	}
	pthread_mutex_unlock(&send_mutex); // unlock
}

// log
void command_log(const char *input)
{
	Packet packet;

	memset(&packet, 0, sizeof(Packet));							 // 패킷 메모리 초기화
	packet.flag = 3;											 // 패킷 플래그 설정 (명령어)
	strncpy(packet.username, username, sizeof(packet.username)); // 패킷에 유저 이름 설정
	strncpy(packet.message, input, sizeof(packet.message));		 // "/log"

	pthread_mutex_lock(&send_mutex);
	if (send_all(client_socket, &packet, sizeof(Packet), 0) < 0)
	{ // 전송
	  // perror("[Client] Failed to /log");
	}
	pthread_mutex_unlock(&send_mutex); // unlock
}

// /quit
void command_quit()
{
	keep_running = 0;
	printf("[Client] Exiting...\n");
	close(client_socket);
}

// 채팅 메시지 입력
void send_chat_message(const char *input)
{
	Packet packet;

	memset(&packet, 0, sizeof(Packet)); // 패킷 메모리 초기화
	packet.flag = 1;					// 패킷 플래그 설정 (채팅)
	strncpy(packet.username, username, sizeof(packet.username));
	strncpy(packet.message, input, sizeof(packet.message));

	pthread_mutex_lock(&send_mutex);
	if (send_all(client_socket, &packet, sizeof(Packet), 0) < 0)
	{ // 전송
	  // perror("[Client] Failed to send_all message");
	}
	pthread_mutex_unlock(&send_mutex); // unlock
}

void print_centered(const char *text, int width) {
    int len = strlen(text);
    int padding = (width - len) / 2;
    if (padding < 0) padding = 0; // Prevent negative padding
    for (int i = 0; i < padding; i++) {
        printf(" ");
    }
    printf("%s\n", text);
}

void command_help() {
	struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int term_width = w.ws_col; // Get terminal width

    // 메뉴 상단
    printf("%.*s\n", 102, "==========================================================================================================================================\n");
    print_centered("COMMAND HELP MENU", term_width);
    printf("%.*s\n", 102, "==========================================================================================================================================\n");

    // 테이블 헤더
    printf("| %-19s | %-76s |\n", "Command", "Description");
    printf("%.*s\n", 102, "--------------------------------------------------------------------------------------------------------------------------------------------\n");

    // 명령어 목록
    printf("| %-19s | %-76s |\n", "/new", "Create a new shared file session");
    printf("| %-19s | %-76s |\n", "/load (filepath)", "Load an existing file to shared file session");
    printf("| %-19s | %-76s |\n", "/commit", "Commit shared file to the server");
    printf("| %-19s | %-76s |\n", "/rebase (version)", "Rebase to the file");
    printf("| %-19s | %-76s |\n", "/quit", "Exit the program");
    printf("| %-19s | %-76s |\n", "/help", "Show this help menu");

    // 메뉴 중간
    printf("%.*s\n", 102, "==========================================================================================================================================\n");
    print_centered("NOTICE", term_width);
    printf("%.*s\n", 102, "==========================================================================================================================================\n");

    // 공지 사항
    printf("| %-98s |\n", "1. Use commands with correct arguments.");
    printf("| %-98s |\n", "2. You should save the shared file before using any instructions.");

    // 메뉴 하단
    printf("%.*s\n", 102, "==========================================================================================================================================\n");
    puts("");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////