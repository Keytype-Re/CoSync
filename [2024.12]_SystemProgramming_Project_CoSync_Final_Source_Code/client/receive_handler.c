/**
 * @author 2023012083 김무성
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "includes/receive_handler.h"


void *receive_server_packet(void *arg)
{
	Packet packet;

	while (1)
	{
		if (!keep_running)
		{
			return NULL;
		}

		int bytes_received = recv_all(client_socket, &packet, sizeof(Packet), 0);

		if (bytes_received <= 0)
		{
			printf("[Client] Disconnected from server.\n");
			return NULL;
		}

		// 패킷 정보를 메인스레드의 작업큐로 보내는 코드
		enqueue(packet);
	}

	return NULL;
}