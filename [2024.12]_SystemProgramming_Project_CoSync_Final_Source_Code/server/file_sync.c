/**
 * @author 2023015097 도현민
 */

#include "includes/file_sync.h"
#include "includes/broadcast.h"
#include <stdio.h>

int handle_file_packet(Packet *packet, int exclude_sock)
{
    if (store_text_file(SHARED_FILE, packet->file_data, 0) < 0) {
        perror(SHARED_FILE);
        return -1;
    }
    broadcast_packet(packet, exclude_sock);
    return 0;
}
