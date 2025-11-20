#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>
#include <stdint.h>

#define VERSION "1.0.0"
#define MAX_EVENTS 1000000
#define MAX_DESC_LEN 256

typedef struct {
  uint16_t button_flags;
  int32_t last_x;
  int32_t last_y;
  int64_t pcounter;
  double ts;
} MouseEvent;

typedef struct {
  char desc[MAX_DESC_LEN];
  double cpi;
  MouseEvent *events;
  size_t event_count;
  size_t event_capacity;
} MouseLog;

typedef enum {
  STATE_IDLE,
  STATE_MEASURE_WAIT,
  STATE_MEASURE,
  STATE_COLLECT_WAIT,
  STATE_COLLECT,
  STATE_LOG
} AppState;

typedef struct {
  double max;
  double min;
  double avg;
  double stdev;
  double range;
  double median;
  double p1;
  double p01;
  double p99;
  double p99_9;
} Statistics;

#endif