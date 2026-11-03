/**
 * @author 2023012083 김무성
 */

#ifndef CHAT_HANDLER_H
#define CHAT_HANDLER_H

#include "common.h"

/**
 * @brief 채팅 메시지를 처리하는 함수.
 *
 * 이 함수는 채팅 메시지를 저장하고, 다른 클라이언트에게 브로드캐스트함.
 *
 * @param packet 처리할 채팅 메시지가 담긴 Packet 구조체의 포인터.
 * @param client_socket 메시지를 보낸 클라이언트의 소켓 파일 디스크립터.
 */
void handle_chat_message(Packet *packet, int sender_sock);

/**
 * @brief 채팅 로그를 저장하는 함수.
 *
 * 이 함수는 주어진 사용자 이름과 메시지를 채팅 로그 파일에 저장함.
 *
 * @param username 채팅 메시지를 보낸 사용자의 이름.
 * @param message 저장할 채팅 메시지.
 */
void save_chat_log(const char *username, const char *message);

#endif