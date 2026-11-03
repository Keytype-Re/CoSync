/**
 * @author 2023015097 도현민
 */

#ifndef COMMON_H
#define COMMON_H

#include <netinet/in.h>
#include <pthread.h>
#include "../../protocol.h"

#define FILE_PATH_SIZE 512
#define MAX_CLIENTS 10
#define PORT 8080
#define CHAT_LOG "chat_log.txt"
#define USER_FILE "user.txt"
#define SHARED_FILE "shared_file.txt"
#define VERSION_DIR "version_logs/"

typedef struct {
    int sockfd;
    char username[50];
    int ready;
} Client;

extern Client clients[MAX_CLIENTS];
extern int client_count;
extern pthread_mutex_t clients_mutex;
#endif
