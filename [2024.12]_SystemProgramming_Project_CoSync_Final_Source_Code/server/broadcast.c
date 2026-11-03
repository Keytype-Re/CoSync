/**
 * @author 2023012083 김무성
 */

#include "includes/broadcast.h"
#include <unistd.h>

/* Dispatch serializes broadcasts. Pin each destination while it is still listed. */
void broadcast_packet(const Packet *packet, int exclude_sock)
{
    int targets[MAX_CLIENTS];
    int count = 0;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (!clients[i].ready || clients[i].sockfd == exclude_sock) continue;
        int fd = dup(clients[i].sockfd);
        if (fd >= 0) targets[count++] = fd;
        else shutdown(clients[i].sockfd, SHUT_RDWR);
    }
    pthread_mutex_unlock(&clients_mutex);
    for (int i = 0; i < count; i++) {
        if (send_all(targets[i], packet, sizeof(*packet), 0) < 0)
            shutdown(targets[i], SHUT_RDWR);
        close(targets[i]);
    }
}
