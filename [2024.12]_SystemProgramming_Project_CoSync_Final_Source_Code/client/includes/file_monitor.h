/**
 * @author 2023015097 도현민
 */

#ifndef FILE_MONITOR_H
#define FILE_MONITOR_H

#include <stdio.h>          // FILE 타입을 사용하기 위해 필요
#include <sys/inotify.h>    // struct inotify_event를 사용하기 위해 필요
#include <sys/socket.h>     // 소켓 관련 함수 사용
#include <string.h>
#include "common.h"

/**
 * @brief 파일 변경을 감지하고 서버로 파일을 전송하는 스레드 함수.
 * 
 * 이 함수는 inotify를 사용하여 지정된 디렉토리 내의 파일 변경을 감지하고, 변경이 발생하면
 * 해당 파일을 서버로 전송함. 전역 변수 `keep_running`이 0으로 설정되면 함수는 종료됨.
 * 
 * @param arg 사용되지 않는 매개변수.
 * @return 함수가 종료될 때 NULL을 반환함.
 */
void *watch_file(void *arg); //(스레드 핸들러)

/**
 * @brief inotify 초기화 함수.
 * 
 * 이 함수는 inotify를 초기화하고 지정된 디렉토리를 감시하도록 설정함.
 * 
 * @return 성공 시 inotify 파일 디스크립터를 반환하고, 실패 시 -1을 반환함.
 */
int init_inotify();

/**
 * @brief 파일을 서버로 전송하는 함수.
 * 
 * 이 함수는 지정된 파일을 읽어 서버로 전송함.
 * 
 * @param file_path 전송할 파일의 경로.
 * @param socket_fd 서버와의 소켓 파일 디스크립터.
 * @param uname 사용자 이름.
 * @return 성공 시 0을 반환하고, 실패 시 -1을 반환함.
 */
int send_file_to_server(const char* file_path, int socket_fd, char *uname);


// 파일 적용
/**
 * @brief 수신한 패킷의 파일 데이터를 지정된 경로에 저장하는 함수.
 * 
 * 이 함수는 수신한 패킷의 파일 데이터를 지정된 경로에 저장함.
 * 
 * @param save_path 저장할 파일의 경로.
 * @param recieved_packet 수신한 패킷.
 * @return 성공 시 0을 반환하고, 실패 시 -1을 반환함.
 */
int apply_to_file(char* save_path, Packet* recieved_packet);

/**
 * @brief 파일의 데이터가 지정된 데이터와 동일한지 확인하는 함수.
 * 
 * 이 함수는 파일의 데이터가 지정된 데이터와 동일한지 확인함.
 * 
 * @param filename 비교할 파일의 이름.
 * @param data 비교할 데이터.
 * @return 동일하면 1을 반환하고, 다르면 0을 반환함.
 */
int is_file_data_same(const char *filename, const char *data); //현재 파일과 현재 패킷의 내용이 같은지 비교


#endif