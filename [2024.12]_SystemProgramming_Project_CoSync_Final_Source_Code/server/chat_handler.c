/**
 * @author 2023012083 김무성
 */

#include <stdio.h>
#include "includes/chat_handler.h"
#include "includes/broadcast.h"

int handle_chat_message(Packet *packet, int client_socket)
{
    FILE *log = fopen(CHAT_LOG, "a");
    if (!log) return -1;
    int failed = fprintf(log, "%s, %s\n", packet->username, packet->message) < 0;
    if (fclose(log) < 0 || failed) return -1;
    broadcast_packet(packet, client_socket);
    return 0;
}
