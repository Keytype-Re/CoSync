/**
 * @author 2023012083 김무성
 */

#ifndef RECEIVE_HANDLER_H
#define RECEIVE_HANDLER_H
#include "common.h"

/* Apply incoming packets until the server disconnects or the session stops. */
int receive_server_packets(void);
void print_right_aligned(const char *name, const char *message);
#endif
