#ifndef MOUSE_LOG_H
#define MOUSE_LOG_H

#include "types.h"

void mouse_log_init(MouseLog *log);
void mouse_log_free(MouseLog *log);
void mouse_log_add(MouseLog *log, MouseEvent event);
void mouse_log_clear(MouseLog *log);
bool mouse_log_load(MouseLog *log, const char *filename);
bool mouse_log_save(const MouseLog *log, const char *filename);
int32_t mouse_log_delta_x(const MouseLog *log);
int32_t mouse_log_delta_y(const MouseLog *log);
double mouse_log_path(const MouseLog *log);

#endif