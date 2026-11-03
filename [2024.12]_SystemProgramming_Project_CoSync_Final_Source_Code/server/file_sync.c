/**
 * @author 2023015097 도현민
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "includes/file_sync.h"
#include "includes/broadcast.h"
#include "includes/common.h"

int handle_file_packet(Packet *file_packet, int exclude_sock)
{
	pthread_mutex_lock(&file_mutex);
	FILE *fp;
	fp = fopen(SHARED_FILE, "w");
	if (fp == NULL)
	{
		perror(SHARED_FILE);
		// return -1;
	}

	if (fwrite(file_packet->file_data, sizeof(char), strlen(file_packet->file_data), fp) < 0)
	{
		perror("fwrite");
		fclose(fp);
		pthread_mutex_unlock(&file_mutex);
		return -1;
	}

	fclose(fp);
	pthread_mutex_unlock(&file_mutex);

	broadcast_packet(file_packet, clients, client_count, exclude_sock);
	return 0;
}