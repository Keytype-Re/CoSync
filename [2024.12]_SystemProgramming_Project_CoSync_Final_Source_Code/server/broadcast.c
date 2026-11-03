/**
 * @author 2023012083 김무성
 */

#include <pthread.h>
#include "includes/common.h"
#include "includes/broadcast.h"

void broadcast_packet(Packet *packet, Client clients[], int client_count, int exclude_sock)
{
    // 자기 자신을 제외한 연결된 모든 클라이언트에게 데이터 전송
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].sockfd != exclude_sock)
        {
            send_all(clients[i].sockfd, packet, sizeof(Packet), 0);
        }
    }
}