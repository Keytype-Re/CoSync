/**
 * @author 2023015097 도현민
 */

#include "includes/file_monitor.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

/* Protected by file_mutex: avoid sending an update just received from the server. */
static char synced[BUFFER_SIZE];

int apply_to_file(const char *data)
{
    pthread_mutex_lock(&file_mutex);
    char current[BUFFER_SIZE];
    int result = read_text_file(SHARED_FILE, current, 0);
    if (result == 0) {
        if (strcmp(current, data)) result = store_text_file(SHARED_FILE, data, 0);
    } else if (errno == ENOENT) {
        result = store_text_file(SHARED_FILE, data, 0);
    }
    if (result == 0) strcpy(synced, data);
    pthread_mutex_unlock(&file_mutex);
    if (result < 0) perror(SHARED_FILE);
    return result;
}

int send_current_file(int commit)
{
    Packet packet = {.flag = FILE_UPDATE};
    pthread_mutex_lock(&file_mutex);
    int result = read_text_file(SHARED_FILE, packet.file_data, 0);
    if (result < 0) perror(SHARED_FILE);
    else if (strcmp(packet.file_data, synced)) {
        result = send_packet(&packet);
        if (result == 0) strcpy(synced, packet.file_data);
    }
    if (result == 0 && commit) {
        packet.flag = COMMAND;
        strcpy(packet.message, "/commit snapshot");
        result = send_packet(&packet);
    }
    pthread_mutex_unlock(&file_mutex);
    return result;
}

int init_inotify(void)
{
    int fd = inotify_init1(IN_CLOEXEC);
    if (fd >= 0 && inotify_add_watch(fd, "./watch", IN_CLOSE_WRITE | IN_MOVED_TO) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

void *watch_file(void *arg)
{
    int fd = *(int *)arg;
    char buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
    while (wait_for_input(fd)) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        for (ssize_t offset = 0; offset < n;) {
            struct inotify_event *event = (void *)(buffer + offset);
            if ((event->mask & IN_Q_OVERFLOW) ||
                (event->len && !(event->mask & IN_ISDIR) &&
                 strcmp(event->name, SHARED_NAME) == 0))
                send_current_file(0);
            if (event->mask & IN_IGNORED) stop_client();
            offset += sizeof(*event) + event->len;
        }
    }
    stop_client();
    return NULL;
}
