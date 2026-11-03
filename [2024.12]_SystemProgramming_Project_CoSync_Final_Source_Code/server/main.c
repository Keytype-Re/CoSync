/**
 * @author 2023015097 도현민
 */

#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include "includes/common.h"
#include "includes/login_control.h"
#include "includes/file_sync.h"
#include "includes/chat_handler.h"
#include "includes/version_control.h"

/* One file/chat operation at a time, including the initial snapshot. A worker
 * keeps its own socket until its last operation completes. */
static pthread_mutex_t dispatch_mutex = PTHREAD_MUTEX_INITIALIZER;
typedef struct Worker {
    int fd;
    struct Worker *next;
} Worker;
static Worker *workers;
static unsigned worker_count;
static pthread_mutex_t workers_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t workers_done = PTHREAD_COND_INITIALIZER;
static int listener;
static atomic_int stopping;
static int accept_failed;

static int send_initial_data(int fd)
{
    Packet history = {.flag = CHAT}, shared = {.flag = FILE_UPDATE};
    FILE *log = fopen(CHAT_LOG, "r");
    if (log) {
        /* Keep the most recent whole records that fit the legacy history packet. */
        long length = -1;
        if (fseek(log, 0, SEEK_END) == 0) length = ftell(log);
        long start = length >= (long)sizeof(history.message) ?
            length - (long)sizeof(history.message) : 0;
        if (length < 0 || fseek(log, start, SEEK_SET) < 0) {
            fclose(log);
            return -1;
        }
        if (length >= (long)sizeof(history.message)) {
            int c;
            do { c = fgetc(log); } while (c != '\n' && c != EOF);
        }
        fread(history.message, 1, sizeof(history.message) - 1, log);
        int failed = ferror(log);
        fclose(log);
        if (failed) return -1;
    } else if (errno != ENOENT) return -1;
    if (read_text_file(SHARED_FILE, shared.file_data, 0) < 0 && errno != ENOENT)
        return -1;
    if (send_all(fd, &history, sizeof(history), 0) < 0) return -1;
    return send_all(fd, &shared, sizeof(shared), 0) < 0 ? -1 : 0;
}

static int login_client(int fd, Client *client)
{
    Packet request;
    ssize_t received;
    while ((received = recv_all(fd, &request, sizeof(request), 0)) > 0) {
        if ((request.flag != LOGIN_NEW && request.flag != CHAT) ||
            !valid_username(request.username, sizeof(request.username))) {
            fprintf(stderr, "Login rejected: invalid login packet or username.\n");
            return -1;
        }
        int signup = request.flag == LOGIN_NEW;
        strcpy(client->username, request.username);
        pthread_mutex_lock(&clients_mutex);
        if (client_count >= MAX_CLIENTS) {
            pthread_mutex_unlock(&clients_mutex);
            send_all(fd, "F", 1, 0);
            return -1;
        }
        int known = is_user_registered(client->username);
        if (known == 1 && signup) {
            pthread_mutex_unlock(&clients_mutex);
            if (send_all(fd, "DUPLICATE", 9, 0) < 0) return -1;
            continue;
        }
        if (!known && !signup) {
            pthread_mutex_unlock(&clients_mutex);
            send_all(fd, "NOT_REGISTERED", 14, 0);
            return -1;
        }
        if (known < 0 || (signup && register_user(client->username) < 0)) {
            pthread_mutex_unlock(&clients_mutex);
            perror(USER_FILE);
            return -1;
        }
        clients[client_count++] = *client;
        pthread_mutex_unlock(&clients_mutex);

        if (signup) {
            char init[sizeof("INIT_REQUEST") - 1];
            if (send_all(fd, "REGISTERED", 10, 0) < 0) {
                perror("Sending registration response");
                remove_client(fd);
                return -1;
            }
            received = recv_all(fd, init, sizeof(init), 0);
            if (received <= 0 || memcmp(init, "INIT_REQUEST", sizeof(init)) != 0) {
                if (received < 0) perror("Receiving INIT_REQUEST");
                else fprintf(stderr, "Login ended without a valid INIT_REQUEST.\n");
                remove_client(fd);
                return -1;
            }
        }
        pthread_mutex_lock(&dispatch_mutex);
        int result = send_initial_data(fd);
        if (result < 0) perror("Preparing or sending initial data");
        else {
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < client_count; i++)
                if (clients[i].sockfd == fd) clients[i].ready = 1;
            pthread_mutex_unlock(&clients_mutex);
        }
        pthread_mutex_unlock(&dispatch_mutex);
        if (result < 0) remove_client(fd);
        return result;
    }
    if (received < 0) perror("Receiving login packet");
    else fprintf(stderr, "Peer disconnected before sending a login packet.\n");
    return -1;
}

static void notify_client(int fd, const char *message)
{
    Packet reply = {.flag = CHAT, .username = "Server"};
    snprintf(reply.message, sizeof(reply.message), "%s", message);
    if (send_all(fd, &reply, sizeof(reply), 0) < 0) shutdown(fd, SHUT_RDWR);
}

static void dispatch(Packet *packet, int fd)
{
    int version;
    if (packet->flag == CHAT) {
        if (strpbrk(packet->message, "\r\n\033")) {
            notify_client(fd, "Chat must be a single line of text.");
        } else if (handle_chat_message(packet, fd) < 0) {
            perror(CHAT_LOG);
            notify_client(fd, "Could not save chat message.");
        }
    } else if (packet->flag == FILE_UPDATE) {
        if (handle_file_packet(packet, fd) < 0) notify_client(fd, "File update failed.");
    } else if (!strcmp(packet->message, "/commit") || !strcmp(packet->message, "/commit snapshot")) {
        const char *snapshot = NULL;
        if (!strcmp(packet->message, "/commit snapshot")) {
            if (!memchr(packet->file_data, 0, sizeof(packet->file_data))) {
                notify_client(fd, "Invalid snapshot.");
                return;
            }
            snapshot = packet->file_data;
        }
        version = commit_version(snapshot);
        if (version < 0) notify_client(fd, "Commit failed.");
        else {
            char reply[64];
            snprintf(reply, sizeof(reply), "Committed version %d.", version);
            notify_client(fd, reply);
        }
    } else if (!strcmp(packet->message, "/log")) {
        if (log_versions(packet, fd) < 0) notify_client(fd, "Could not list versions.");
    } else if (!strncmp(packet->message, "/rebase ", 8) &&
               parse_version(packet->message + 8, &version) == 0) {
        if (rebase_version(version, packet, -1) < 0)
            notify_client(fd, "Could not restore that version.");
    } else {
        notify_client(fd, "Unknown command or invalid version.");
    }
}

static void *receive_packet(void *arg)
{
    Worker *worker = arg;
    Client client = {.sockfd = worker->fd};
    if (login_client(worker->fd, &client) == 0) {
        struct timeval no_timeout = {0};
        if (setsockopt(worker->fd, SOL_SOCKET, SO_RCVTIMEO, &no_timeout, sizeof(no_timeout)) < 0)
            shutdown(worker->fd, SHUT_RDWR);
        Packet packet;
        while (recv_all(worker->fd, &packet, sizeof(packet), 0) > 0) {
            if (!valid_payload(&packet)) break;
            memset(packet.username, 0, sizeof(packet.username));
            strcpy(packet.username, client.username);
            pthread_mutex_lock(&dispatch_mutex);
            dispatch(&packet, worker->fd);
            pthread_mutex_unlock(&dispatch_mutex);
        }
        remove_client(worker->fd);
    }
    pthread_mutex_lock(&workers_mutex);
    Worker **link = &workers;
    while (*link != worker) link = &(*link)->next;
    *link = worker->next;
    worker_count--;
    close(worker->fd);
    free(worker);
    pthread_cond_signal(&workers_done);
    pthread_mutex_unlock(&workers_mutex);
    return NULL;
}

static void *accept_clients(void *arg)
{
    (void)arg;
    for (;;) {
        int fd = accept(listener, NULL, NULL);
        if (fd < 0) {
            if (errno == EINTR || errno == ECONNABORTED) continue;
            if (!stopping) {
                perror("accept");
                accept_failed = 1;
                kill(getpid(), SIGTERM);
            }
            break;
        }
        struct timeval timeout = {.tv_sec = 5};
        if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
            close(fd);
            continue;
        }
        struct timeval login_timeout = {.tv_sec = 30};
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &login_timeout, sizeof(login_timeout)) < 0) {
            close(fd);
            continue;
        }
        Worker *worker = malloc(sizeof(*worker));
        if (!worker) { close(fd); continue; }
        worker->fd = fd;
        pthread_mutex_lock(&workers_mutex);
        /* Bound pending handshakes separately from the logged-in user limit. */
        if (worker_count >= 2 * MAX_CLIENTS) {
            pthread_mutex_unlock(&workers_mutex);
            free(worker);
            close(fd);
            continue;
        }
        worker_count++;
        worker->next = workers;
        workers = worker;
        pthread_t thread;
        if (pthread_create(&thread, NULL, receive_packet, worker) != 0) {
            workers = worker->next;
            worker_count--;
            close(fd);
            free(worker);
        } else pthread_detach(thread);
        pthread_mutex_unlock(&workers_mutex);
    }
    return NULL;
}

int main(void)
{
    sigset_t signals;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    if (pthread_sigmask(SIG_BLOCK, &signals, NULL) != 0) return EXIT_FAILURE;
    initialize_version_directory();
    struct sockaddr_in server_addr = {.sin_family = AF_INET, .sin_port = htons(PORT)};
    server_addr.sin_addr.s_addr = INADDR_ANY;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    int reuse = 1;
    if (listener < 0 || setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0 ||
        bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 ||
        listen(listener, MAX_CLIENTS) < 0) {
        perror("server startup");
        return EXIT_FAILURE;
    }
    pthread_t accept_thread;
    if (pthread_create(&accept_thread, NULL, accept_clients, NULL) != 0) return EXIT_FAILURE;
    printf("[Server] Listening on port %d...\n", PORT);
    int signal_number;
    sigwait(&signals, &signal_number);
    stopping = 1;
    shutdown(listener, SHUT_RDWR);
    pthread_join(accept_thread, NULL);
    close(listener);
    pthread_mutex_lock(&workers_mutex);
    for (Worker *w = workers; w; w = w->next) shutdown(w->fd, SHUT_RDWR);
    while (workers) pthread_cond_wait(&workers_done, &workers_mutex);
    pthread_mutex_unlock(&workers_mutex);
    return accept_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
