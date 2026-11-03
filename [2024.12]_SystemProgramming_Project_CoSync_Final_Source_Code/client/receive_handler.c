/**
 * @author 2023012083 김무성
 */

#include "includes/receive_handler.h"
#include "includes/file_monitor.h"
#include <stdio.h>
#include <string.h>


int receive_server_packets(void)
{
    Packet packet;
    while (recv_all(client_socket, &packet, sizeof(packet), 0) > 0) {
        if (packet.flag == CHAT &&
            memchr(packet.username, 0, sizeof(packet.username)) && valid_payload(&packet)) {
            printf("\033[2K\r");
            print_right_aligned(packet.username, packet.message);
        } else if (packet.flag == FILE_UPDATE && valid_payload(&packet)) {
            if (apply_to_file(packet.file_data) < 0) break;
            continue;
        } else if (packet.flag == COMMAND &&
                   memchr(packet.file_data, 0, sizeof(packet.file_data))) {
            printf("\n[Version_Log]\n%s\n", packet.file_data);
        } else break;
        printf("[%s] : ", username);
        fflush(stdout);
    }
    int unexpected = keep_running;
    if (unexpected) fprintf(stderr, "Disconnected from server.\n");
    stop_client();
    return unexpected ? -1 : 0;
}
