/**
 * @author 2023016057 조신근
 */

#include <stdio.h>
#include "includes/common.h"
#include <string.h>
#include <unistd.h>

int is_user_registered(const char *username)
{ // 이미 등록된 이름인지 확인
    FILE *file = fopen(USER_FILE, "r");
    if (!file)
        return 0;

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), file))
    {
        line[strcspn(line, "\n")] = '\0';
        if (strcmp(line, username) == 0)
        {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

void register_user(const char *username)
{ // 입력한 이름으로 회원가입
    FILE *file = fopen(USER_FILE, "a");
    if (file)
    {
        fprintf(file, "%s\n", username);
        fclose(file);
    }
}

void remove_client(int sockfd)
{ // 클라이언트 연결 해제 및 배열 관리
    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].sockfd == sockfd)
        {
            printf("[Server] Client %s disconnected.\n", clients[i].username);

            close(sockfd);

            // 연결 끊긴 클라이언트 이후의 모든 클라이언트를 앞으로 이동
            for (int j = i; j < client_count - 1; j++)
            {
                clients[j] = clients[j + 1];
            }

            client_count--;
            break;
        }
    }

    printf("[Server] Remaining clients: %d\n", client_count);
    pthread_mutex_unlock(&clients_mutex);
}