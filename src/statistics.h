#ifndef STATISTICS_H
#define STATISTICS_H

#include "types.h"

Statistics calculate_interval_statistics(const MouseLog *log);
void calculate_timestamps(MouseLog *log, int64_t freq);

#endif