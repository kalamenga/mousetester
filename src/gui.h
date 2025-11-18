#ifndef GUI_H
#define GUI_H

#include "types.h"
#include "plot.h"
#include <windows.h>

typedef struct {
    HWND hwnd;
    HWND desc_edit;
    HWND cpi_edit;
    HWND status_text;
    HWND measure_btn;
    HWND collect_btn;
    HWND log_btn;
    HWND plot_btn;
    HWND save_btn;
    HWND load_btn;
    HWND stats_text;
} MainWindow;

typedef struct {
    HWND hwnd;
    HWND plot_combo;
    HWND start_edit;
    HWND end_edit;
    HWND lines_check;
    HWND stem_check;
    HWND stats_box;
    MouseLog *log;
    PlotType type;
    size_t start_idx;
    size_t end_idx;
    double start_time;
    double end_time;
    bool show_lines;
    bool show_stem;
} PlotWindow;

bool create_main_window(HINSTANCE hInstance, MainWindow *wnd, MouseLog *log, AppState *state, LARGE_INTEGER *freq);
void create_plot_window(HINSTANCE hInstance, MouseLog *log);
void update_status(MainWindow *wnd, const char *text);
void update_stats(MainWindow *wnd, const Statistics *stats);

#endif