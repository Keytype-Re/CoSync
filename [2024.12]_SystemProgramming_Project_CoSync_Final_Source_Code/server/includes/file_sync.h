/**
 * @author 2023015097 도현민
 */

#ifndef FILE_SYCN_H
#define FILE_SYCN_H
#include "broadcast.h"

/**
 * @brief 파일 패킷을 처리하는 함수.
 *
 * 이 함수는 주어진 파일 패킷의 데이터를 shared_file.txt에 저장하고, 다른 클라이언트에게 브로드캐스트함.
 *
 * @param file_packet 처리할 파일 패킷.
 * @param exclude_sock 브로드캐스트에서 제외할 클라이언트의 소켓 파일 디스크립터.
 * @return 성공 시 0을 반환하고, 실패 시 -1을 반환함.
 */
int handle_file_packet(Packet *file_packet, int exclude_sock);

#endif
