/**
 * @author 2023012083 김무성
 */

#ifndef BROADCAST_H
#define BROADCAST_H
#include "common.h"

void broadcast_packet(const Packet *packet, int exclude_sock);
#endif
