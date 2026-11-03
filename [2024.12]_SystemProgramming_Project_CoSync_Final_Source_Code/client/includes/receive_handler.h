/**
 * @author 2023012083 김무성
 */

#ifndef RECEIVE_HANDLER_H
#define RECEIVE_HANDLER_H
#include "common.h"

int display_server_packet(const Packet *packet);
void print_right_aligned(const char *name, const char *message);
#endif
