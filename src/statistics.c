#include "statistics.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int compare_double(const void *a, const void *b) {
    double diff = *(const double *)a - *(const double *)b;
    return (diff > 0) - (diff < 0);
}

Statistics calculate_interval_statistics(const MouseLog *log) {
    Statistics stats = {0};
    
    if (log->event_count < 2) return stats;
    
    double *intervals = malloc((log->event_count - 1) * sizeof(double));
    if (!intervals) return stats;
    
    for (size_t i = 1; i < log->event_count; i++) {
        intervals[i - 1] = log->events[i].ts - log->events[i - 1].ts;
    }
    
    size_t count = log->event_count - 1;
    
    double sum = 0.0;
    stats.min = intervals[0];
    stats.max = intervals[0];
    
    for (size_t i = 0; i < count; i++) {
        sum += intervals[i];
        if (intervals[i] < stats.min) stats.min = intervals[i];
        if (intervals[i] > stats.max) stats.max = intervals[i];
    }
    
    stats.avg = sum / count;
    stats.range = stats.max - stats.min;
    
    double sq_diff_sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        double diff = intervals[i] - stats.avg;
        sq_diff_sum += diff * diff;
    }
    stats.stdev = sqrt(sq_diff_sum / count);
    
    qsort(intervals, count, sizeof(double), compare_double);
    
    if (count % 2 == 1) {
        stats.median = intervals[count / 2];
    } else {
        stats.median = (intervals[count / 2 - 1] + intervals[count / 2]) / 2.0;
    }
    
    size_t idx_99 = (size_t)ceil(0.99 * count) - 1;
    size_t idx_999 = (size_t)ceil(0.999 * count) - 1;
    if (idx_99 < count) stats.percentile_99 = intervals[idx_99];
    if (idx_999 < count) stats.percentile_999 = intervals[idx_999];
    
    free(intervals);
    return stats;
}

void calculate_timestamps(MouseLog *log, int64_t freq) {
    if (log->event_count == 0) return;
    
    int64_t min_counter = log->events[0].pcounter;
    for (size_t i = 0; i < log->event_count; i++) {
        log->events[i].ts = (double)(log->events[i].pcounter - min_counter) * 1000.0 / freq;
    }
}