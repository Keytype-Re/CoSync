/**
 * @author 2023016057 조신근
 */

#ifndef LOGIN_CONTROL_H
#define LOGIN_CONTROL_H

#include "common.h"

/**
 * @brief 사용자가 이미 등록된 이름인지 확인하는 함수.
 *
 * 이 함수는 주어진 사용자 이름이 이미 등록된 이름인지 확인함.
 *
 * @param username 확인할 사용자 이름.
 * @return 이미 등록된 이름이면 1을 반환하고, 그렇지 않으면 0을 반환함.
 */
int is_user_registered(const char *username);

/**
 * @brief 사용자를 등록하는 함수.
 *
 * 이 함수는 주어진 사용자 이름으로 사용자를 등록함.
 *
 * @param username 등록할 사용자 이름.
 */
void register_user(const char *username);

/**
 * @brief 클라이언트 연결을 해제하고 배열을 관리하는 함수.
 *
 * 이 함수는 주어진 소켓 파일 디스크립터를 가진 클라이언트의 연결을 해제하고,
 * 클라이언트 배열을 업데이트함.
 *
 * @param sockfd 연결을 해제할 클라이언트의 소켓 파일 디스크립터.
 */
void remove_client(int sockfd);

#endif