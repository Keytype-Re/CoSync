/**
 * @author 2023012083 김무성
 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "includes/chat_handler.h"
#include "includes/broadcast.h"

void handle_chat_message(Packet *packet, int client_socket)
{
    save_chat_log(packet->username, packet->message);               // 채팅 로그 저장
    broadcast_packet(packet, clients, client_count, client_socket); // 다른 클라이언트에게 채팅 보내기
}

void save_chat_log(const char *username, const char *message)
{
    FILE *log = fopen(CHAT_LOG, "a"); // 추가 모드(a)로 파일 열기
    if (log)
    {
        fprintf(log, "%s, %s\n", username, message);
        fclose(log);
    }
}
