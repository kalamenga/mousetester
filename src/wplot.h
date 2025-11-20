#ifndef WPLOT_H
#define WPLOT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { WPLOT_LINE, WPLOT_SCATTER, WPLOT_SPLINE, WPLOT_STEM } WPlotType;

typedef struct wplot_ctx wplot_ctx;

wplot_ctx *wplot_create(const char *title, int width, int height);

void wplot_add(wplot_ctx *ctx, double *x, double *y, int count, WPlotType type,
               unsigned int color, float thickness);

void wplot_show(wplot_ctx *ctx);

void wplot_free(wplot_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif