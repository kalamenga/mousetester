#include "mouse_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

void mouse_log_init(MouseLog *log) {
    strcpy(log->desc, "MouseTester");
    log->cpi = 400.0;
    log->event_capacity = 1024;
    log->events = malloc(log->event_capacity * sizeof(MouseEvent));
    log->event_count = 0;
}

void mouse_log_free(MouseLog *log) {
    if (log->events) {
        free(log->events);
        log->events = NULL;
    }
    log->event_count = 0;
    log->event_capacity = 0;
}

void mouse_log_add(MouseLog *log, MouseEvent event) {
    if (log->event_count >= log->event_capacity) {
        log->event_capacity *= 2;
        if (log->event_capacity > MAX_EVENTS) {
            log->event_capacity = MAX_EVENTS;
        }
        log->events = realloc(log->events, log->event_capacity * sizeof(MouseEvent));
    }
    
    if (log->event_count < log->event_capacity) {
        log->events[log->event_count++] = event;
    }
}

void mouse_log_clear(MouseLog *log) {
    log->event_count = 0;
}

bool mouse_log_load(MouseLog *log, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return false;
    
    mouse_log_clear(log);
    
    if (fgets(log->desc, MAX_DESC_LEN, file) == NULL) {
        fclose(file);
        return false;
    }
    log->desc[strcspn(log->desc, "\n")] = 0;
    
    if (fscanf(file, "%lf\n", &log->cpi) != 1) {
        fclose(file);
        return false;
    }
    
    char header[256];
    if (fgets(header, sizeof(header), file) == NULL) {
        fclose(file);
        return false;
    }
    
    int32_t x, y;
    double ts;
    uint16_t flags;
    while (fscanf(file, "%d,%d,%lf,%hu\n", &x, &y, &ts, &flags) == 4) {
        MouseEvent event = {flags, x, y, 0, ts};
        mouse_log_add(log, event);
    }
    
    fclose(file);
    return true;
}

bool mouse_log_save(const MouseLog *log, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) return false;
    
    fprintf(file, "%s\n", log->desc);
    fprintf(file, "%.1f\n", log->cpi);
    fprintf(file, "xCount,yCount,Time (ms),buttonflags\n");
    
    for (size_t i = 0; i < log->event_count; i++) {
        const MouseEvent *e = &log->events[i];
        fprintf(file, "%d,%d,%.6f,%u\n", e->last_x, e->last_y, e->ts, e->button_flags);
    }
    
    fclose(file);
    return true;
}

int32_t mouse_log_delta_x(const MouseLog *log) {
    int32_t sum = 0;
    for (size_t i = 0; i < log->event_count; i++) {
        sum += log->events[i].last_x;
    }
    return sum;
}

int32_t mouse_log_delta_y(const MouseLog *log) {
    int32_t sum = 0;
    for (size_t i = 0; i < log->event_count; i++) {
        sum += log->events[i].last_y;
    }
    return sum;
}

double mouse_log_path(const MouseLog *log) {
    double path = 0.0;
    for (size_t i = 0; i < log->event_count; i++) {
        const MouseEvent *e = &log->events[i];
        path += sqrt((double)(e->last_x * e->last_x) + (double)(e->last_y * e->last_y));
    }
    return path;
}