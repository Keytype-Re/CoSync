/**
 * @author 2021115459 전호준
*/

#ifndef VERSION_CONTROL_H
#define VERSION_CONTROL_H

#include <pthread.h>
#include "common.h"

/**
 * @brief 버전 저장 경로를 초기화하는 함수.
 * 
 * 이 함수는 버전 저장 디렉토리가 존재하지 않으면 생성함.
 */
void initialize_version_directory();

/**
 * @brief 파일을 새로운 버전으로 커밋하는 함수.
 * 
 * 이 함수는 현재 shared_file.txt 파일을 새로운 버전으로 저장함.
 */
void commit_version();

/**
 * @brief 버전 로그를 클라이언트에게 전송하는 함수.
 * 
 * 이 함수는 버전 디렉토리에 저장된 모든 버전 파일의 로그를 클라이언트에게 전송함.
 * 
 * @param current_packet 현재 처리 중인 패킷.
 * @param client_sock 클라이언트 소켓 파일 디스크립터.
 */
void log_versions(Packet *current_packet, int client_sock);

/**
 * @brief 특정 버전으로 파일을 복원하는 함수.
 * 
 * 이 함수는 주어진 버전 번호에 해당하는 파일로 shared_file.txt를 복원함.
 * 
 * @param version_number 복원할 버전 번호.
 * @param current_packet 현재 처리 중인 패킷.
 * @param client_sock 클라이언트 소켓 파일 디스크립터.
 */
void rebase_version(int version_number, Packet *current_packet, int client_sock);

#endif // VERSION_CONTROL_H
