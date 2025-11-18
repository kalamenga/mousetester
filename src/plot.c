#include "plot.h"
#include <math.h>
#include <string.h>

bool export_plot_csv(const MouseLog *log, PlotType type, const char *filename,
                     size_t start_idx, size_t end_idx) {
    if (start_idx >= log->event_count || end_idx >= log->event_count) return false;
    
    FILE *file = fopen(filename, "w");
    if (!file) return false;
    
    switch (type) {
    case PLOT_X_VS_TIME:
        fprintf(file, "Time(ms),xCount\n");
        for (size_t i = start_idx; i <= end_idx; i++) {
            fprintf(file, "%.6f,%d\n", log->events[i].ts, log->events[i].last_x);
        }
        break;
        
    case PLOT_Y_VS_TIME:
        fprintf(file, "Time(ms),yCount\n");
        for (size_t i = start_idx; i <= end_idx; i++) {
            fprintf(file, "%.6f,%d\n", log->events[i].ts, log->events[i].last_y);
        }
        break;
        
    case PLOT_XY_VS_TIME:
        fprintf(file, "Time(ms),xCount,yCount\n");
        for (size_t i = start_idx; i <= end_idx; i++) {
            fprintf(file, "%.6f,%d,%d\n", log->events[i].ts, 
                   log->events[i].last_x, log->events[i].last_y);
        }
        break;
        
    case PLOT_INTERVAL_VS_TIME:
        fprintf(file, "Time(ms),Interval(ms)\n");
        for (size_t i = start_idx; i <= end_idx; i++) {
            double interval = (i == 0) ? 0.0 : 
                log->events[i].ts - log->events[i-1].ts;
            fprintf(file, "%.6f,%.6f\n", log->events[i].ts, interval);
        }
        break;
        
    case PLOT_FREQUENCY_VS_TIME:
        fprintf(file, "Time(ms),Frequency(Hz)\n");
        for (size_t i = start_idx; i <= end_idx; i++) {
            double interval = (i == 0) ? 0.0 : 
                log->events[i].ts - log->events[i-1].ts;
            double freq = (interval > 0) ? 1000.0 / interval : 0.0;
            fprintf(file, "%.6f,%.6f\n", log->events[i].ts, freq);
        }
        break;
        
    case PLOT_X_VELOCITY_VS_TIME:
        if (log->cpi > 0) {
            fprintf(file, "Time(ms),xVelocity(m/s)\n");
            for (size_t i = start_idx; i <= end_idx; i++) {
                double vel = 0.0;
                if (i > 0) {
                    double dt = log->events[i].ts - log->events[i-1].ts;
                    if (dt > 0) {
                        vel = log->events[i].last_x / dt / log->cpi * 25.4;
                    }
                }
                fprintf(file, "%.6f,%.6f\n", log->events[i].ts, vel);
            }
        }
        break;
        
    case PLOT_X_VS_Y:
        fprintf(file, "xCount,yCount\n");
        {
            double x = 0, y = 0;
            for (size_t i = start_idx; i <= end_idx; i++) {
                x += log->events[i].last_x;
                y += log->events[i].last_y;
                fprintf(file, "%.2f,%.2f\n", x, y);
            }
        }
        break;
        
    default:
        break;
    }
    
    fclose(file);
    return true;
}

void print_plot_text(const MouseLog *log, PlotType type, size_t start, size_t end) {
    const char *titles[] = {
        "X Counts vs Time",
        "Y Counts vs Time",
        "XY Counts vs Time",
        "Interval vs Time",
        "Frequency vs Time",
        "X Velocity vs Time",
        "Y Velocity vs Time",
        "XY Velocity vs Time",
        "X vs Y"
    };
    
    printf("\n=== %s ===\n", titles[type]);
    printf("Events: %zu to %zu (total: %zu)\n", start, end, log->event_count);
    printf("CPI: %.1f\n", log->cpi);
    printf("Description: %s\n\n", log->desc);
}