#ifndef GUI_H
#define GUI_H

#include "plot.h"
#include "types.h"
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
  HWND lines_check;
  // Stem removed

  // Config Controls
  HWND start_time_edit;
  HWND end_time_edit;
  HWND update_btn;

  MouseLog *log;
  PlotType type;

  double view_start_ms;
  double view_end_ms;

  bool show_lines;
  bool show_stats;
} PlotWindow;

bool create_main_window(HINSTANCE hInstance, MainWindow *wnd, MouseLog *log,
                        AppState *state, LARGE_INTEGER *freq);
void create_plot_window(HINSTANCE hInstance, MouseLog *log);
void update_status(MainWindow *wnd, const char *text);
void update_stats(MainWindow *wnd, const Statistics *stats);

#endif