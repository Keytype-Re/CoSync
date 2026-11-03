/**
 * @author 2023016057 조신근
 */

#ifndef COMMON_H
#define COMMON_H

#include <netinet/in.h>
#include <pthread.h>
#include <stdatomic.h>
#include "../../protocol.h"

#define PORT 8080
#define USER_FILE "user.txt"
#define SHARED_NAME "shared_file.txt"
#define SHARED_FILE "./watch/" SHARED_NAME

extern atomic_int keep_running;
extern pthread_mutex_t send_mutex;
extern pthread_mutex_t file_mutex;
extern int client_socket;
extern char username[50];
extern int stop_pipe[2];

void stop_client(void);
int send_packet(Packet *packet);
int wait_for_input(int fd);
#endif
