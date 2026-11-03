/**
 * @author 2023012083 김무성
 */

#ifndef SEND_HANDLER_H
#define SEND_HANDLER_H
#include "common.h"

int run_chat_ui(void);
int terminal_columns(void);
void command_help(void);
void print_centered(const char *text, int width);
#endif
