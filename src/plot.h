#ifndef PLOT_H
#define PLOT_H

#include "types.h"
#include <stdio.h>

typedef enum {
    PLOT_X_VS_TIME,
    PLOT_Y_VS_TIME,
    PLOT_XY_VS_TIME,
    PLOT_INTERVAL_VS_TIME,
    PLOT_FREQUENCY_VS_TIME,
    PLOT_X_VELOCITY_VS_TIME,
    PLOT_Y_VELOCITY_VS_TIME,
    PLOT_XY_VELOCITY_VS_TIME,
    PLOT_X_VS_Y
} PlotType;

bool export_plot_csv(const MouseLog *log, PlotType type, const char *filename, 
                     size_t start_idx, size_t end_idx);
void print_plot_text(const MouseLog *log, PlotType type, size_t start, size_t end);

#endif