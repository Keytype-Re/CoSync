/**
 * @author 2023015097 도현민
 */

#ifndef FILE_MONITOR_H
#define FILE_MONITOR_H
#include "common.h"

int init_inotify(void);
void *watch_file(void *arg);
int apply_to_file(const char *data);
int send_current_file(int commit);
#endif
