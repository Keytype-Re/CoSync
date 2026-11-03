/**
 * @author 2023012083 김무성
 */

#ifndef BROADCAST_H
#define BROADCAST_H

#include "common.h"

/**
 * @brief 모든 클라이언트에게 패킷을 브로드캐스트하는 함수.
 *
 * 이 함수는 자기 자신을 제외한 연결된 모든 클라이언트에게 주어진 패킷을 전송함.
 *
 * @param packet 전송할 데이터가 담긴 Packet 구조체의 포인터.
 * @param clients 현재 연결된 클라이언트 정보를 담은 배열.
 * @param client_count 현재 연결된 클라이언트의 수.
 * @param exclude_sock 브로드캐스트에서 제외할 클라이언트의 소켓 파일 디스크립터.
 */
void broadcast_packet(Packet *packet, Client clients[], int client_count, int exclude_sock);

#endif
