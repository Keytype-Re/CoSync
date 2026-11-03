/**
 * @author 2023015097 도현민
 */

#include "includes/file_monitor.h"
#include "includes/common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>



#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))
#define WATCH_DIRECTORY "./watch/"
#define FILE_NAME_SIZE 64

void *watch_file(void *arg){
    pthread_mutex_lock(&global_mutex);
    int sockfd = client_socket;
    char name[50];
    snprintf(name, sizeof(name), "%s", username);
    pthread_mutex_unlock(&global_mutex);

    int inotify_fd = init_inotify();
    char buffer[EVENT_BUF_LEN];
    int length;

    while(1){
        if(!keep_running){
            close(inotify_fd); //스레드 종료 시 inotify도 자원해제
            return NULL;
        }
        length = read(inotify_fd, buffer, EVENT_BUF_LEN);

        if(length == 0){
            continue;
        }
        else if(length < 0){
            // perror("fail to read event");
            exit(EXIT_FAILURE);
        }

        int i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *)&buffer[i];
            if (event->len) {
                char file_path[FILE_PATH_SIZE];
                snprintf(file_path, sizeof(file_path), "%s%s", WATCH_DIRECTORY, event->name);

                if (event->mask & IN_CREATE) {
                    // printf("[파일 생성 감지] : %s\n", event->name);
                    send_file_to_server(file_path, sockfd, name);
                } else if (event->mask & IN_MODIFY) {
                    // printf("[파일 수정 감지] : %s\n", event->name);
                    sleep(1); // 잠시 기다리기
                    send_file_to_server(file_path, sockfd ,name);
                }
            }
            i += EVENT_SIZE + event->len;
        }
        usleep(10000);
    }
    
}

int init_inotify() {
    int inotify_fd = inotify_init();
    if (inotify_fd < 0) {
        // perror("inotify_init");
        return -1;
    }
    
    int watch_descriptor = inotify_add_watch(inotify_fd, WATCH_DIRECTORY, IN_MODIFY | IN_CREATE);
    if (watch_descriptor < 0) {
        // perror("inotify_add_watch");
        return -1;
    }

    return inotify_fd;
}

int send_file_to_server(const char* file_path, int socket_fd, char *uname) {
    size_t bytes_read;
	Packet new_packet = {0};
	new_packet.flag = 2;
	snprintf(new_packet.username, sizeof(new_packet.username), "%s", uname);

    pthread_mutex_lock(&file_mutex);
    FILE *file = fopen(file_path, "r");
    if (!file) {
        // perror("fopen");
        pthread_mutex_unlock(&file_mutex);
        return -1;
    }
    
    if((bytes_read = fread(new_packet.file_data, 1, sizeof(new_packet.file_data), file)) < 0){
		// perror("fread");
		fclose(file);
        pthread_mutex_unlock(&file_mutex);
		return -1;
	}
    fclose(file);
    pthread_mutex_unlock(&file_mutex);

 
    pthread_mutex_lock(&send_mutex);
    if (send_all(socket_fd, &new_packet, sizeof(Packet), 0) < 0) {
            // perror("send_all");
            pthread_mutex_unlock(&send_mutex);
            return -1;
    }
    pthread_mutex_unlock(&send_mutex);

    return 0;
}

int apply_to_file(char* save_path, Packet *recieved_packet) {
    pthread_mutex_lock(&file_mutex);
    FILE *file = fopen(save_path, "w");
    if (!file) {
        // perror("fopen");
        pthread_mutex_unlock(&file_mutex); // 뮤텍스 해제
        return -1;
    }

	if (fwrite(&recieved_packet->file_data, sizeof(char) ,strlen(recieved_packet->file_data) , file) < 0) {
		// perror("fwrite");
		fclose(file);
        pthread_mutex_unlock(&file_mutex); // 뮤텍스 해제
		return -1;
	}

    fclose(file);
    pthread_mutex_unlock(&file_mutex);
    return 0;
}

int is_file_data_same(const char *filename, const char *data) {
    pthread_mutex_lock(&file_mutex);
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        // 파일이 없거나 읽을 수 없는 경우에는 다르다고 처리(파일은 무조건 있음. 메인 초기호 과정에 포함되어 있다)
        pthread_mutex_unlock(&file_mutex);
        return 0;
    }
    
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    rewind(fp);
    
    size_t data_len = strlen(data);
    if (file_size != (long)data_len) {
        fclose(fp);
        pthread_mutex_unlock(&file_mutex);
        return 0;
    }
    
    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        fclose(fp);
        pthread_mutex_unlock(&file_mutex);
        return 0;
    }
    
    size_t read_size = fread(buffer, 1, file_size, fp);

    fclose(fp);
    pthread_mutex_unlock(&file_mutex);

    if (read_size != (size_t)file_size) {
        free(buffer);
        return 0;
    }
    buffer[file_size] = '\0';
    
    int result = (strcmp(buffer, data) == 0) ? 1 : 0;
    free(buffer);

    return result;
}