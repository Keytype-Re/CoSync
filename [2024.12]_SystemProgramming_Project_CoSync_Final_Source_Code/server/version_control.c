/**
 * @author 2021115459 전호준
*/

#define _GNU_SOURCE
#include "includes/version_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include "includes/broadcast.h"
#include "includes/common.h"


typedef struct {
    char filename[FILE_PATH_SIZE];
    time_t creation_time;
} VersionEntry;


/**
 * @brief 버전 엔트리를 비교하는 함수.
 * 
 * 이 함수는 두 버전 엔트리의 생성 시간을 비교하여 정렬에 사용됨.
 * 
 * @param a 비교할 첫 번째 엔트리.
 * @param b 비교할 두 번째 엔트리.
 * @return 첫 번째 엔트리가 더 오래된 경우 음수, 더 새로운 경우 양수, 동일한 경우 0을 반환함.
 */
static int compare_entries(const void *a, const void *b) {
    VersionEntry *entryA = (VersionEntry *)a;
    VersionEntry *entryB = (VersionEntry *)b;
    return (entryA->creation_time > entryB->creation_time) -
           (entryA->creation_time < entryB->creation_time);
}

// 버전 저장 경로 초기화
void initialize_version_directory() {
    DIR* dir = opendir(VERSION_DIR);
    if (dir) {
        // 디렉터리가 이미 존재함
        closedir(dir);
    } else {
        // 디렉터리가 없으면 생성
        if (mkdir(VERSION_DIR, 0700) == -1) {
            perror("Failed to create version directory");
            exit(EXIT_FAILURE);
        }
    }
}

static int version_from_name(const char *name)
{
    char *end;
    if (*name < '0' || *name > '9') return 0;
    errno = 0;
    long n = strtol(name, &end, 10);
    return !errno && n > 0 && n <= INT_MAX &&
           strcmp(end, "_shared_file.txt") == 0 ? (int)n : 0;
}

int commit_version(const char *snapshot)
{
    char data[BUFFER_SIZE];
    if (!snapshot) {
        if (read_text_file(SHARED_FILE, data, 0) < 0) return -1;
        snapshot = data;
    }
    DIR *dir = opendir(VERSION_DIR);
    if (!dir) return -1;
    int highest = 0;
    struct dirent *entry;
    while (1) {
        errno = 0;
        entry = readdir(dir);
        if (!entry) break;
        int number = version_from_name(entry->d_name);
        if (number > highest) highest = number;
    }
    int error = errno;
    closedir(dir);
    if (error) {
        errno = error;
        return -1;
    }
    while (highest < INT_MAX) {
        char path[FILE_PATH_SIZE];
        snprintf(path, sizeof(path), "%s%d_shared_file.txt", VERSION_DIR, ++highest);
        if (store_text_file(path, snapshot, 1) == 0) return highest;
        if (errno != EEXIST) return -1;
    }
    errno = EOVERFLOW;
    return -1;
}

static int send_version_page(int fd, Packet *page)
{
    if (send_all(fd, page, sizeof(*page), 0) < 0) {
        shutdown(fd, SHUT_RDWR);
        return -1;
    }
    page->file_data[0] = 0;
    return 0;
}

int log_versions(Packet *request, int fd)
{
    DIR *dir = opendir(VERSION_DIR);
    if (!dir) return -1;
    VersionEntry *entries = NULL;
    size_t count = 0, capacity = 0;
    int result = -1;
    struct dirent *entry;
    for (;;) {
        errno = 0;
        entry = readdir(dir);
        if (!entry) {
            if (errno) goto done;
            break;
        }
        if (!version_from_name(entry->d_name)) continue;
        struct statx st;
        if (statx(dirfd(dir), entry->d_name, AT_SYMLINK_NOFOLLOW,
                  STATX_TYPE | STATX_BTIME | STATX_MTIME, &st) < 0) goto done;
        if (!S_ISREG(st.stx_mode)) continue;
        if (count == capacity) {
            size_t next = capacity ? capacity * 2 : 16;
            VersionEntry *grown = reallocarray(entries, next, sizeof(*entries));
            if (!grown) goto done;
            entries = grown;
            capacity = next;
        }
        entries[count].creation_time = (st.stx_mask & STATX_BTIME) ?
            st.stx_btime.tv_sec : st.stx_mtime.tv_sec;
        snprintf(entries[count].filename, sizeof(entries[count].filename), "%s", entry->d_name);
        count++;
    }
    if (count) qsort(entries, count, sizeof(*entries), compare_entries);
    Packet page = {.flag = COMMAND};
    strcpy(page.username, request->username);
    size_t used = 0;
    for (size_t i = 0; i < count; i++) {
        char timestamp[32], line[FILE_PATH_SIZE + 64];
        struct tm tm;
        if (!localtime_r(&entries[i].creation_time, &tm) ||
            !strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm))
            goto done;
        int n = snprintf(line, sizeof(line), "Version: %s\tDate: %s\n", entries[i].filename, timestamp);
        if (n < 0 || (size_t)n >= sizeof(line)) goto done;
        if (used + (size_t)n >= sizeof(page.file_data)) {
            if (send_version_page(fd, &page) < 0) goto done;
            used = 0;
        }
        memcpy(page.file_data + used, line, (size_t)n + 1);
        used += (size_t)n;
    }
    result = send_version_page(fd, &page);
done:
    free(entries);
    closedir(dir);
    return result;
}

int rebase_version(int number, Packet *request, int exclude_sock)
{
    char path[FILE_PATH_SIZE];
    snprintf(path, sizeof(path), "%s%d_shared_file.txt", VERSION_DIR, number);
    Packet packet = {.flag = FILE_UPDATE};
    snprintf(packet.username, sizeof(packet.username), "%s", request->username);
    if (read_text_file(path, packet.file_data, 0) < 0 ||
        store_text_file(SHARED_FILE, packet.file_data, 0) < 0)
        return -1;
    broadcast_packet(&packet, exclude_sock);
    return 0;
}
