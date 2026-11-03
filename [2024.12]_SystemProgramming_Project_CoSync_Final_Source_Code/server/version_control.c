/**
 * @author 2021115459 전호준
*/

#define _GNU_SOURCE
#include "includes/version_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/syscall.h>
#include <linux/stat.h>
#include <sys/stat.h>
#include "includes/broadcast.h"
#include "includes/common.h"

//pthread_mutex_t version_mutex = PTHREAD_MUTEX_INITIALIZER;

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
int compare_entries(const void *a, const void *b) {
    VersionEntry *entryA = (VersionEntry *)a;
    VersionEntry *entryB = (VersionEntry *)b;
    return (entryA->creation_time - entryB->creation_time);
}

// 버전 저장 경로 초기화
void initialize_version_directory() {
    DIR* dir = opendir(VERSION_DIR);
    if (dir) {
        // 디렉터리가 이미 존재함
        closedir(dir);
    } else {
        // 디렉터리가 없으면 생성
        if (mkdir(VERSION_DIR, 0777) == -1) {
            perror("Failed to create version directory");
            exit(EXIT_FAILURE);
        }
    }
}

// 파일을 새로운 버전으로 커밋
void commit_version() {
    //pthread_mutex_lock(&version_mutex);

    // 버전 번호 확인 (현재 디렉토리에 저장된 파일 개수를 기반으로 결정)
    int version_number = 1;
    struct dirent *entry;
    DIR *dp = opendir(VERSION_DIR);
    if (dp) {
        while ((entry = readdir(dp))) {
            if (entry->d_name[0] != '.') { // 숨김 파일 제외
                version_number++;
            }
        }
        closedir(dp);
    }

    // 버전 파일 이름 생성
    char versioned_file[FILE_PATH_SIZE];
    snprintf(versioned_file, sizeof(versioned_file), "%s%d_shared_file.txt", VERSION_DIR, version_number);

    // 원본 파일 복사
    FILE *src_file = fopen(SHARED_FILE, "r");
    FILE *dest_file = fopen(versioned_file, "w");

    if (!src_file || !dest_file) {
        perror("Failed to open file for commit");
        //pthread_mutex_unlock(&version_mutex);
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
        fwrite(buffer, 1, bytes_read, dest_file);
    }

    fclose(src_file);
    fclose(dest_file);

    printf("[Server] Version %d committed successfully.\n", version_number);
    //pthread_mutex_unlock(&version_mutex);
}

void log_versions(Packet *current_packet, int client_sock) {
    if (current_packet == NULL) {
        fprintf(stderr, "current_packet is NULL\n");
        return;
    }

    DIR *dp = opendir(VERSION_DIR);
    if (!dp) {
        perror("Failed to open version directory");
        return;
    }

    printf("\n[Version Log]\n");
    char filename[FILE_PATH_SIZE];
    struct statx statxbuf;

    VersionEntry entries[100];
    int entry_count = 0;

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] != '.') {
            if (entry_count >= 100) {
                fprintf(stderr, "Too many version files. Maximum supported is %d.\n", 100);
                break;
            }

            snprintf(filename, sizeof(filename), "%s%s", VERSION_DIR, entry->d_name);

            if (syscall(SYS_statx, AT_FDCWD, filename, AT_STATX_SYNC_AS_STAT, STATX_ALL, &statxbuf) != 0) {
                perror("statx");
                continue;
            }

            entries[entry_count].creation_time = statxbuf.stx_btime.tv_sec;
            strncpy(entries[entry_count].filename, entry->d_name, FILE_PATH_SIZE - 1);
            entries[entry_count].filename[FILE_PATH_SIZE - 1] = '\0';
            entry_count++;
        }
    }
    closedir(dp);

    // Sort entries by creation time (newest first)
    qsort(entries, entry_count, sizeof(VersionEntry), compare_entries);

    Packet packet;
    packet.flag = 3;
    strncpy(packet.username, current_packet->username, sizeof(packet.username) - 1);
    packet.username[sizeof(packet.username) - 1] = '\0';

    char log_buffer[BUFFER_SIZE] = "";

    for (int i = 0; i < entry_count; i++) {
        char time_buffer[30];
        struct tm *tm_info = localtime(&entries[i].creation_time);
        if (tm_info == NULL) {
            fprintf(stderr, "Failed to convert time for file: %s\n", entries[i].filename);
            continue;
        }

        if (strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm_info) == 0) {
            fprintf(stderr, "Failed to format time for file: %s\n", entries[i].filename);
            continue;
        }

        char entry_log[FILE_PATH_SIZE + 50];
        snprintf(entry_log, sizeof(entry_log), "Version: %s\tDate: %s\n", entries[i].filename, time_buffer);
        strncat(log_buffer, entry_log, sizeof(log_buffer) - strlen(log_buffer) - 1);
    }

    strncpy(packet.file_data, log_buffer, sizeof(packet.file_data) - 1);
    packet.file_data[sizeof(packet.file_data) - 1] = '\0';

    pthread_mutex_lock(&send_mutex);
    send_all(client_sock, &packet, sizeof(Packet), 0);
    pthread_mutex_unlock(&send_mutex);
}

// 특정 버전으로 복원 (/rebase 명령어)
void rebase_version(int version_number, Packet *current_packet, int client_sock) {
    //pthread_mutex_lock(&version_mutex);

    char versioned_file[FILE_PATH_SIZE];
    snprintf(versioned_file, sizeof(versioned_file), "%s%d_shared_file.txt", VERSION_DIR, version_number);

    // 버전 파일이 있는지 확인
    FILE *src_file = fopen(versioned_file, "r");
    if (!src_file) {
        printf("[Server] Version %d not found.\n", version_number);
        //pthread_mutex_unlock(&version_mutex);
        return;
    }

    // 원본 파일 덮어쓰기
    FILE *dest_file = fopen(SHARED_FILE, "w");
    if (!dest_file) {
        perror("Failed to open shared file for rebase");
        fclose(src_file);
        //pthread_mutex_unlock(&version_mutex);
        return;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
        fwrite(buffer, 1, bytes_read, dest_file);
    }

    fclose(src_file);
    fclose(dest_file);

    printf("[Server] File rebased to version %d successfully.\n", version_number);

    // 버전 복원 후 클라이언트에 전송
    Packet packet;
    packet.flag = 2;
    strncpy(packet.username, current_packet->username, sizeof(packet.username));
    snprintf(packet.message, sizeof(packet.message), "Rebased to version %d", version_number);

    FILE *file_to_send = fopen(SHARED_FILE, "r");
    if (file_to_send) {
        fread(packet.file_data, sizeof(char), BUFFER_SIZE, file_to_send);
        fclose(file_to_send);

        pthread_mutex_lock(&send_mutex);
        broadcast_packet(&packet,clients, client_count, client_sock ); // -1은 모든 클라이언트를 의미함
        pthread_mutex_unlock(&send_mutex);
    }

    //pthread_mutex_unlock(&version_mutex);
}
