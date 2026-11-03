#include "protocol.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

ssize_t send_all(int fd, const void *buffer, size_t length, int flags)
{
    size_t offset = 0;
    while (offset < length) {
        ssize_t n = send(fd, (const char *)buffer + offset, length - offset,
                         flags | MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        offset += n;
    }
    return offset;
}

ssize_t recv_all(int fd, void *buffer, size_t length, int flags)
{
    size_t offset = 0;
    while (offset < length) {
        ssize_t n = recv(fd, (char *)buffer + offset, length - offset, flags);
        if (n < 0 && errno == EINTR) continue;
        if (n == 0 && offset != 0) {
            errno = EPROTO;
            return -1;
        }
        if (n <= 0) return n;
        offset += n;
    }
    return offset;
}

int valid_username(const char *name, size_t capacity)
{
    if (!memchr(name, 0, capacity) || !*name) return 0;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        if (*p < ' ' || *p == 127 || *p == ',') return 0;
    }
    return 1;
}

int valid_payload(const Packet *packet)
{
    if (packet->flag == CHAT || packet->flag == COMMAND)
        return memchr(packet->message, 0, sizeof(packet->message)) != NULL;
    return packet->flag == FILE_UPDATE &&
           memchr(packet->file_data, 0, sizeof(packet->file_data)) != NULL;
}

int parse_version(const char *text, int *number)
{
    char *end;
    if (*text < '0' || *text > '9') return -1;
    errno = 0;
    long n = strtol(text, &end, 10);
    if (errno || *end || n < 1 || n > INT_MAX) return -1;
    *number = (int)n;
    return 0;
}

/* A wire field holds at most BUFFER_SIZE - 1 bytes of text plus its NUL. */
int read_text_file(const char *path, char text[BUFFER_SIZE], int allow_symlink)
{
    int fd = open(path, O_RDONLY | O_NONBLOCK | (allow_symlink ? 0 : O_NOFOLLOW));
    if (fd < 0) return -1;
    struct stat st;
    int error = fstat(fd, &st) < 0 ? errno : 0;
    if (!error && !S_ISREG(st.st_mode)) error = EINVAL;
    if (error) {
        close(fd);
        errno = error;
        return -1;
    }
    FILE *fp = fdopen(fd, "rb");
    if (!fp) {
        int saved = errno;
        close(fd);
        errno = saved;
        return -1;
    }
    size_t n = fread(text, 1, BUFFER_SIZE, fp);
    error = ferror(fp) ? EIO : 0;
    if (n == BUFFER_SIZE) error = EFBIG;
    else if (memchr(text, 0, n)) error = EILSEQ;
    if (fclose(fp) != 0 && !error) error = errno;
    if (error) {
        errno = error;
        return -1;
    }
    text[n] = 0;
    return 0;
}

/* Write beside the destination so publication cannot cross filesystems. */
int store_text_file(const char *path, const char *text, int exclusive)
{
    char temp[PATH_MAX];
    if (snprintf(temp, sizeof(temp), "%s.tmp.XXXXXX", path) >= (int)sizeof(temp)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    struct stat previous;
    int preserve_mode = 0;
    if (!exclusive) {
        if (lstat(path, &previous) == 0) {
            if (!S_ISREG(previous.st_mode)) { errno = EINVAL; return -1; }
            if (access(path, W_OK) < 0) return -1;
            preserve_mode = 1;
        } else if (errno != ENOENT) return -1;
    }
    int fd = mkstemp(temp);
    if (fd < 0) return -1;
    size_t left = strlen(text);
    int error = 0;
    while (left) {
        ssize_t n = write(fd, text, left);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            error = n < 0 ? errno : EIO;
            break;
        }
        text += n;
        left -= n;
    }
    if (!error && preserve_mode && fchmod(fd, previous.st_mode & 0777) < 0)
        error = errno;
    if (close(fd) < 0 && !error) error = errno;
    if (!error && (exclusive ? link(temp, path) : rename(temp, path)) < 0)
        error = errno;
    if (error || exclusive) unlink(temp);
    if (error) {
        errno = error;
        return -1;
    }
    return 0;
}
