/**
 * @author 2023015097 도현민
 */

#include "includes/common.h"

Client clients[MAX_CLIENTS];
int client_count;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
