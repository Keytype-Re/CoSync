/**
 * @author 2023012083 김무성
 */

#include "includes/receive_handler.h"
#include "includes/file_monitor.h"
#include <stdio.h>
#include <string.h>

int display_server_packet(const Packet *packet)
{
    if (packet->flag == CHAT && valid_payload(packet) &&
        memchr(packet->username, 0, sizeof(packet->username))) {
        print_right_aligned(packet->username, packet->message);
    } else if (packet->flag == FILE_UPDATE && valid_payload(packet)) {
        return apply_to_file(packet->file_data);
    } else if (packet->flag == COMMAND &&
               memchr(packet->file_data, 0, sizeof(packet->file_data))) {
        puts("[Version_Log]");
        size_t length = strlen(packet->file_data);
        if (length == 0) puts("(no saved versions)");
        else {
            fputs(packet->file_data, stdout);
            if (packet->file_data[length - 1] != '\n') putchar('\n');
        }
    } else return -1;
    return 0;
}
