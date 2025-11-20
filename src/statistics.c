#include "statistics.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int compare_doubles(const void *a, const void *b) {
  double da = *(const double *)a;
  double db = *(const double *)b;
  return (da > db) - (da < db);
}

Statistics calculate_interval_statistics(const MouseLog *log,
                                         bool is_frequency) {
  Statistics stats = {0};

  if (log->event_count < 2)
    return stats;

  size_t count = log->event_count - 1;
  double *intervals = malloc(count * sizeof(double));

  if (!intervals)
    return stats;

  double sum = 0.0;

  for (size_t i = 1; i < log->event_count; i++) {
    double dt = log->events[i].ts - log->events[i - 1].ts;
    double val;

    if (is_frequency) {

      val = (dt > 1e-7) ? (1000.0 / dt) : 0.0;
    } else {
      val = dt;
    }

    intervals[i - 1] = val;
    sum += val;

    if (i == 1) {
      stats.min = val;
      stats.max = val;
    } else {
      if (val < stats.min)
        stats.min = val;
      if (val > stats.max)
        stats.max = val;
    }
  }

  stats.avg = sum / (double)count;
  stats.range = stats.max - stats.min;

  double sq_diff_sum = 0.0;
  for (size_t i = 0; i < count; i++) {
    double diff = intervals[i] - stats.avg;
    sq_diff_sum += diff * diff;
  }

  if (count > 1) {
    stats.stdev = sqrt(sq_diff_sum / (double)(count - 1));
  } else {
    stats.stdev = 0.0;
  }

  qsort(intervals, count, sizeof(double), compare_doubles);

  if (count % 2 == 0) {
    stats.median = (intervals[count / 2 - 1] + intervals[count / 2]) / 2.0;
  } else {
    stats.median = intervals[count / 2];
  }

  size_t i01 = (size_t)((double)count * 0.001);
  size_t i1 = (size_t)((double)count * 0.01);
  size_t i99 = (size_t)((double)count * 0.99);
  size_t i999 = (size_t)((double)count * 0.999);

  if (i01 >= count)
    i01 = count - 1;
  if (i1 >= count)
    i1 = count - 1;
  if (i99 >= count)
    i99 = count - 1;
  if (i999 >= count)
    i999 = count - 1;

  stats.p01 = intervals[i01];
  stats.p1 = intervals[i1];
  stats.p99 = intervals[i99];
  stats.p99_9 = intervals[i999];

  free(intervals);
  return stats;
}

void calculate_timestamps(MouseLog *log, int64_t freq) {
  if (log->event_count == 0)
    return;

  int64_t min_counter = log->events[0].pcounter;

  double inv_freq_ms = 0.0;
  if (freq > 0) {
    inv_freq_ms = 1000.0 / (double)freq;
  } else {
    inv_freq_ms = 1.0;
  }

  for (size_t i = 0; i < log->event_count; i++) {
    log->events[i].ts =
        (double)(log->events[i].pcounter - min_counter) * inv_freq_ms;
  }
}