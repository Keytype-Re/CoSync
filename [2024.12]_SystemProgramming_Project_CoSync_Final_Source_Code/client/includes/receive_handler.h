/**
 * @author 2023012083 김무성
 */

#ifndef RECEIVE_HANDLER_H
#define RECEIVE_HANDLER_H

#include "common.h"

/**
 * @author 2023012083 김무성
 * @brief 서버로부터 패킷을 수신하는 스레드 함수.
 * 
 * 이 함수는 루프를 돌며 서버로부터 패킷을 지속적으로 수신하고, 이를 메인 스레드의 작업 큐에 넣음.
 * 전역 변수 `keep_running`이 0으로 설정되면 함수는 종료됨.
 * 
 * @param arg 사용되지 않는 매개변수.
 * @return 함수가 종료될 때 NULL을 반환함.
 */
void *receive_server_packet(void *arg);

#endif // RECEIVE_HANDLER_H