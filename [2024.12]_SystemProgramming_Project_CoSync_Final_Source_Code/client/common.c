/**
 * @author 2023016057 조신근
 */

#include "includes/common.h"
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

atomic_int keep_running = 1;
pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
int client_socket = -1;
char username[50];
int stop_pipe[2];

void stop_client(void)
{
    if (atomic_exchange(&keep_running, 0)) {
        shutdown(client_socket, SHUT_RDWR);
        /* Closing the sole writer wakes every poller, including future waiters. */
        close(stop_pipe[1]);
    }
}

int wait_for_input(int fd)
{
    struct pollfd fds[] = {{fd, POLLIN, 0}, {stop_pipe[0], POLLIN, 0}};
    int n;
    do { n = poll(fds, 2, -1); } while (n < 0 && errno == EINTR);
    return n > 0 && !fds[1].revents && (fds[0].revents & (POLLIN | POLLHUP));
}

int send_packet(Packet *packet)
{
    snprintf(packet->username, sizeof(packet->username), "%s", username);
    pthread_mutex_lock(&send_mutex);
    int failed = send_all(client_socket, packet, sizeof(*packet), 0) < 0;
    pthread_mutex_unlock(&send_mutex);
    if (failed) stop_client();
    return failed ? -1 : 0;
}
