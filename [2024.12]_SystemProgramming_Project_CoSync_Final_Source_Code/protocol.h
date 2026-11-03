#ifndef COSYNC_PROTOCOL_H
#define COSYNC_PROTOCOL_H

#include <stddef.h>
#include <sys/types.h>

#define BUFFER_SIZE 4096
enum { LOGIN_NEW, CHAT, FILE_UPDATE, COMMAND };

typedef struct {
    int flag;
    char username[50];
    char message[BUFFER_SIZE];
    char file_data[BUFFER_SIZE];
} Packet;

ssize_t send_all(int fd, const void *buffer, size_t length, int flags);
ssize_t recv_all(int fd, void *buffer, size_t length, int flags);
int read_text_file(const char *path, char text[BUFFER_SIZE], int allow_symlink);
int store_text_file(const char *path, const char *text, int exclusive);
int valid_username(const char *name, size_t capacity);
int valid_payload(const Packet *packet);
int parse_version(const char *text, int *number);

#endif
