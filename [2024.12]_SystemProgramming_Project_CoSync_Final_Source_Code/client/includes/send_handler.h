/**
 * @author 2023012083 김무성
 */

#ifndef SEND_HANDLER_H
#define SEND_HANDLER_H

#include "common.h"

/**
 * @brief 터미널에서 입력을 받아 서버로 패킷을 전송하는 스레드 함수.
 * 
 * 이 함수는 루프를 돌며 터미널에서 입력을 받아 명령어에 따라 적절한 함수를 호출하거나
 * 채팅 메시지를 서버로 전송함. 전역 변수 `keep_running`이 0으로 설정되면 함수는 종료됨.
 * 
 * @param arg 사용되지 않는 매개변수.
 * @return 함수가 종료될 때 NULL을 반환함.
 */
void *send_terminal_packet(void *arg);

// includes/send_handler.h

/**
 * @brief shared_file.txt 파일을 초기화하는 함수.
 * 
 * 이 함수는 shared_file.txt 파일을 쓰기 모드로 열어 내용을 삭제하고 파일을 초기화함.
 */
void command_new();

/**
 * @brief 지정된 파일을 읽어 shared_file.txt에 복사하는 함수.
 * 
 * 이 함수는 입력된 파일 경로에서 파일을 읽어 shared_file.txt에 내용을 복사함.
 * 
 * @param input 파일 경로를 포함한 입력 문자열.
 */
void command_load(const char *input);

/**
 * @brief 서버로 commit 명령을 전송하는 함수.
 * 
 * 이 함수는 서버로 commit 명령을 포함한 패킷을 전송함.
 * 
 * @param input "/commit" 명령어 문자열.
 */
void command_commit(const char *input);

/**
 * @brief 서버로 rebase 명령을 전송하는 함수.
 * 
 * 이 함수는 서버로 rebase 명령을 포함한 패킷을 전송함.
 * 
 * @param input "/rebase (number)" 명령어 문자열.
 */
void command_rebase(const char *input);

/**
 * @brief 서버로 log 명령을 전송하는 함수.
 * 
 * 이 함수는 서버로 log 명령을 포함한 패킷을 전송함.
 * 
 * @param input "/log" 명령어 문자열.
 */
void command_log(const char *input);

/**
 * @brief 서버와의 연결을 종료하는 함수.
 * 
 * 이 함수는 서버와의 연결을 종료하고 프로그램을 종료함.
 */
void command_quit();

/**
 * @brief 서버로 채팅 메시지를 전송하는 함수.
 * 
 * 이 함수는 입력된 채팅 메시지를 서버로 전송함.
 * 
 * @param input 채팅 메시지 문자열.
 */

void send_chat_message(const char *input);

/**
 * @brief 도움말을 제공하는 함수(명령어)
 * 
 * 명령어 등의 도움말을 터미널에 출력.
 * 
 */

void command_help();

void print_centered(const char *text, int width);

#endif // SEND_HANDLER_H