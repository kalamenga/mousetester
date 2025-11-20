#include "gui.h"
#include "mouse_log.h"
#include "statistics.h"
#include "wplot.h"
#include <float.h>
#include <math.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>

enum ControlIDs {
  ID_DESC_EDIT = 101,
  ID_CPI_EDIT,
  ID_MEASURE_BTN,
  ID_COLLECT_BTN,
  ID_LOG_BTN,
  ID_PLOT_BTN,
  ID_SAVE_BTN,
  ID_LOAD_BTN,
  ID_TYPE_COMBO
};

static MainWindow *g_main_wnd = NULL;
static MouseLog *g_main_log = NULL;
static AppState *g_main_state = NULL;
static LARGE_INTEGER *g_main_freq = NULL;

#define COLOR_BLUE 0xFF0000FF
#define COLOR_RED 0xFFFF0000
#define COLOR_DARK_BLUE 0xFF00008B
#define COLOR_DARK_RED 0xFF8B0000

static LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
static void handle_measure_click(void);
static void handle_collect_click(void);
static void handle_log_click(void);
static void handle_plot_click(void);
static void handle_save_click(void);
static void handle_load_click(void);

void update_status(MainWindow *wnd, const char *text) {
  SetWindowText(wnd->status_text, text);
}

void update_stats(MainWindow *wnd, const Statistics *stats) {
  char buf[512];
  snprintf(buf, sizeof(buf),
           "Avg: %.4f   StDev: %.4f   Range: %.4f\r\n"
           "Min: %.4f   Max: %.4f     Median: %.4f\r\n"
           "1%% Low: %.4f   0.1%% Low: %.4f\r\n"
           "99%% High: %.4f  99.9%% High: %.4f",
           stats->avg, stats->stdev, stats->range, 
           stats->min, stats->max, stats->median, 
           stats->p1, stats->p01,
           stats->p99, stats->p99_9);
           
  SetWindowText(wnd->stats_text, buf);
}

static void ts_calc(MouseLog *log, const LARGE_INTEGER *freq) {
  if (log->event_count == 0)
    return;
  int64_t min = log->events[0].pcounter;

  double freq_ms_inv = 1000.0 / (double)freq->QuadPart;
  if (freq->QuadPart == 0)
    freq_ms_inv = 1.0;

  for (size_t i = 0; i < log->event_count; i++) {
    log->events[i].ts = (double)(log->events[i].pcounter - min) * freq_ms_inv;
  }
}

static HWND CreateCtrl(const char *wc, const char *txt, DWORD style, int x,
                       int y, int w, int h, HWND parent, int id) {
  return CreateWindowEx(strcmp(wc, "EDIT") == 0 ? WS_EX_CLIENTEDGE : 0, wc, txt,
                        WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent,
                        (HMENU)(intptr_t)id, GetModuleHandle(NULL), NULL);
}

#define MAX_PLOT_SERIES 8

typedef struct {
  double *x[MAX_PLOT_SERIES];
  double *y[MAX_PLOT_SERIES];
  int count[MAX_PLOT_SERIES];
  int type[MAX_PLOT_SERIES];
  unsigned int color[MAX_PLOT_SERIES];
  float thick[MAX_PLOT_SERIES];
  int num_series;
  char title[128];
  char desc[MAX_DESC_LEN];
} PlotThreadArgs;

static void calculate_time_based_smoothing(const double *src_x,
                                           const double *src_y, int count,
                                           double **out_x, double **out_y,
                                           int *out_count) {
  if (count == 0) {
    *out_count = 0;
    return;
  }

  const double interval_ms = 8.0;
  double max_x = src_x[count - 1];

  int est_buckets = (int)(max_x / interval_ms) + 2;

  *out_x = malloc(est_buckets * sizeof(double));
  *out_y = malloc(est_buckets * sizeof(double));

  int idx = 0;
  int src_idx = 0;
  double current_boundary = interval_ms;

  while (current_boundary <= max_x + interval_ms && src_idx < count) {
    double sum = 0.0;
    int n = 0;

    if (src_x[src_idx] > current_boundary) {
      current_boundary += interval_ms;
      continue;
    }

    while (src_idx < count && src_x[src_idx] <= current_boundary) {
      sum += src_y[src_idx];
      n++;
      src_idx++;
    }

    if (n > 0) {
      (*out_x)[idx] = current_boundary - (interval_ms * 0.5);
      (*out_y)[idx] = sum / (double)n;
      idx++;
    }
    current_boundary += interval_ms;
  }
  *out_count = idx;
}

unsigned __stdcall PlotThreadFunc(void *arg) {
  PlotThreadArgs *args = (PlotThreadArgs *)arg;

  wplot_ctx *ctx = wplot_create(args->title, 1000, 600);
  if (!ctx)
    goto cleanup;

  for (int i = 0; i < args->num_series; i++) {
    if (args->count[i] > 0 && args->x[i] && args->y[i]) {
      wplot_add(ctx, args->x[i], args->y[i], args->count[i],
                (WPlotType)args->type[i], args->color[i], args->thick[i]);
    }
  }

  wplot_show(ctx);
  wplot_free(ctx);

cleanup:
  for (int i = 0; i < args->num_series; i++) {
    if (args->x[i])
      free(args->x[i]);
    if (args->y[i])
      free(args->y[i]);
  }
  free(args);
  return 0;
}

static void add_series_to_args(PlotThreadArgs *args, double *x, double *y,
                               int count, int type, unsigned int color,
                               float thick, bool copy_data) {
  if (args->num_series >= MAX_PLOT_SERIES)
    return;

  int idx = args->num_series++;

  if (copy_data) {
    args->x[idx] = malloc(count * sizeof(double));
    args->y[idx] = malloc(count * sizeof(double));
    memcpy(args->x[idx], x, count * sizeof(double));
    memcpy(args->y[idx], y, count * sizeof(double));
  } else {
    args->x[idx] = x;
    args->y[idx] = y;
  }

  args->count[idx] = count;
  args->type[idx] = type;
  args->color[idx] = color;
  args->thick[idx] = thick;
}

static void process_series_extraction(const MouseLog *log, PlotType type,
                                      bool is_y, double **raw_x, double **raw_y,
                                      int *count) {
  *raw_x = malloc(log->event_count * sizeof(double));
  *raw_y = malloc(log->event_count * sizeof(double));
  int idx = 0;

  double cpi_factor = (log->cpi > 0) ? (0.0254 / log->cpi) : 0;
  double vel_mult = (cpi_factor > 0) ? (1.0 / log->cpi * 25.4) : 0;

  for (size_t i = 0; i < log->event_count; i++) {
    double val = 0;
    double t = log->events[i].ts;
    double dt = (i > 0) ? (t - log->events[i - 1].ts) : 0;
    bool ok = false;

    switch (type) {
    case PLOT_X_VS_TIME:
    case PLOT_Y_VS_TIME:
    case PLOT_XY_VS_TIME:
      val = is_y ? log->events[i].last_y : log->events[i].last_x;
      ok = true;
      break;

    case PLOT_INTERVAL_VS_TIME:
      if (i > 0) {
        val = dt;
        ok = (val <= 500.0 || i >= 10);
      }
      break;

    case PLOT_FREQUENCY_VS_TIME:
      if (i > 0 && dt > 1e-5) {
        val = 1000.0 / dt;
        ok = true;
      }
      break;

    case PLOT_X_VELOCITY_VS_TIME:
    case PLOT_Y_VELOCITY_VS_TIME:
    case PLOT_XY_VELOCITY_VS_TIME:
      if (i > 0 && dt > 1e-5 && vel_mult > 0) {
        double d = is_y ? (double)log->events[i].last_y
                        : (double)log->events[i].last_x;
        val = d / dt * vel_mult;
        ok = true;
      }
      break;
    default:
      break;
    }

    if (ok) {
      (*raw_x)[idx] = t;
      (*raw_y)[idx] = val;
      idx++;
    }
  }
  *count = idx;
}

static void extract_and_plot(const MouseLog *log, PlotType type) {
  if (log->event_count < 2)
    return;

  PlotThreadArgs *args = calloc(1, sizeof(PlotThreadArgs));
  strncpy(args->desc, log->desc, MAX_DESC_LEN - 1);

  bool dual = (type == PLOT_XY_VS_TIME || type == PLOT_XY_VELOCITY_VS_TIME);
  bool x_vs_y = (type == PLOT_X_VS_Y);
  bool use_stem =
      (type == PLOT_INTERVAL_VS_TIME || type == PLOT_FREQUENCY_VS_TIME);

  if (x_vs_y) {
    snprintf(args->title, 128, "X vs Y - %s", log->desc);
    double *rx = malloc(log->event_count * sizeof(double));
    double *ry = malloc(log->event_count * sizeof(double));
    double sum_x = 0, sum_y = 0;
    for (size_t i = 0; i < log->event_count; i++) {
      sum_x += log->events[i].last_x;
      sum_y += log->events[i].last_y;
      rx[i] = sum_x;
      ry[i] = sum_y;
    }

    double *lx = malloc(log->event_count * sizeof(double));
    double *ly = malloc(log->event_count * sizeof(double));
    memcpy(lx, rx, log->event_count * sizeof(double));
    memcpy(ly, ry, log->event_count * sizeof(double));

    add_series_to_args(args, lx, ly, log->event_count, WPLOT_LINE,
                       COLOR_DARK_BLUE, 1.0f, false);
    add_series_to_args(args, rx, ry, log->event_count, WPLOT_SCATTER,
                       COLOR_BLUE, 1.5f, false);

  } else {

    double *rx1, *ry1;
    int c1;
    process_series_extraction(log, type, false, &rx1, &ry1, &c1);

    if (c1 > 0) {
      add_series_to_args(args, rx1, ry1, c1, WPLOT_SCATTER, COLOR_BLUE, 1.5f,
                         true);

      double *sx1, *sy1;
      int sc1;
      calculate_time_based_smoothing(rx1, ry1, c1, &sx1, &sy1, &sc1);

      if (sc1 > 0) {
        add_series_to_args(args, sx1, sy1, sc1, WPLOT_SPLINE, COLOR_DARK_BLUE,
                           2.0f, false);
      }

      if (use_stem) {
        double *stem_x = malloc(c1 * sizeof(double));
        double *stem_y = malloc(c1 * sizeof(double));
        memcpy(stem_x, rx1, c1 * sizeof(double));
        memcpy(stem_y, ry1, c1 * sizeof(double));
        add_series_to_args(args, stem_x, stem_y, c1, WPLOT_STEM, COLOR_BLUE,
                           1.0f, false);
      }

      free(rx1);
      free(ry1);
    }

    if (dual) {
      double *rx2, *ry2;
      int c2;
      process_series_extraction(log, type, true, &rx2, &ry2, &c2);

      if (c2 > 0) {
        add_series_to_args(args, rx2, ry2, c2, WPLOT_SCATTER, COLOR_RED, 1.5f,
                           true);

        double *sx2, *sy2;
        int sc2;
        calculate_time_based_smoothing(rx2, ry2, c2, &sx2, &sy2, &sc2);

        if (sc2 > 0) {
          add_series_to_args(args, sx2, sy2, sc2, WPLOT_SPLINE, COLOR_DARK_RED,
                             2.0f, false);
        }

        if (use_stem) {
          double *stem_x2 = malloc(c2 * sizeof(double));
          double *stem_y2 = malloc(c2 * sizeof(double));
          memcpy(stem_x2, rx2, c2 * sizeof(double));
          memcpy(stem_y2, ry2, c2 * sizeof(double));
          add_series_to_args(args, stem_x2, stem_y2, c2, WPLOT_STEM, COLOR_RED,
                             1.0f, false);
        }

        free(rx2);
        free(ry2);
      }
    }

    const char *t = "";
    switch (type) {
    case PLOT_INTERVAL_VS_TIME:
      t = "Interval vs Time";
      break;
    case PLOT_FREQUENCY_VS_TIME:
      t = "Frequency vs Time";
      break;
    case PLOT_X_VELOCITY_VS_TIME:
      t = "xVelocity vs Time";
      break;
    case PLOT_Y_VELOCITY_VS_TIME:
      t = "yVelocity vs Time";
      break;
    case PLOT_XY_VELOCITY_VS_TIME:
      t = "xyVelocity vs Time";
      break;
    case PLOT_X_VS_TIME:
      t = "xCounts vs Time";
      break;
    case PLOT_Y_VS_TIME:
      t = "yCounts vs Time";
      break;
    case PLOT_XY_VS_TIME:
      t = "xyCounts vs Time";
      break;
    default:
      break;
    }
    snprintf(args->title, 128, "%s - %s", t, log->desc);
  }

  if (args->num_series == 0) {
    free(args);
    MessageBox(g_main_wnd->hwnd, "No valid data points.", "Error", MB_OK);
    return;
  }

  HANDLE hThread =
      (HANDLE)_beginthreadex(NULL, 0, PlotThreadFunc, args, 0, NULL);
  if (hThread)
    CloseHandle(hThread);
}

bool create_main_window(HINSTANCE hInstance, MainWindow *wnd, MouseLog *log,
                        AppState *state, LARGE_INTEGER *freq) {
  g_main_wnd = wnd;
  g_main_log = log;
  g_main_state = state;
  g_main_freq = freq;
  WNDCLASSEX wc = {sizeof(WNDCLASSEX),
                   0,
                   MainWndProc,
                   0,
                   0,
                   hInstance,
                   LoadIcon(hInstance, MAKEINTRESOURCE(1)),
                   LoadCursor(NULL, IDC_ARROW),
                   (HBRUSH)(COLOR_BTNFACE + 1),
                   NULL,
                   "MouseTesterMainClass",
                   NULL};
  RegisterClassEx(&wc);
  wnd->hwnd = CreateWindow(
      "MouseTesterMainClass", "MouseTester",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
      CW_USEDEFAULT, CW_USEDEFAULT, 500, 480, NULL, NULL, hInstance, NULL);
  if (!wnd->hwnd)
    return false;

  HFONT hFont =
      CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                 DEFAULT_PITCH | FF_SWISS, "Segoe UI");

  CreateCtrl("STATIC", "Description:", 0, 10, 12, 80, 20, wnd->hwnd, 0);
  wnd->desc_edit = CreateCtrl("EDIT", log->desc, ES_AUTOHSCROLL, 100, 10, 370,
                              24, wnd->hwnd, ID_DESC_EDIT);
  CreateCtrl("STATIC", "CPI:", 0, 10, 47, 80, 20, wnd->hwnd, 0);
  wnd->cpi_edit = CreateCtrl("EDIT", "400", ES_NUMBER, 100, 45, 100, 24,
                             wnd->hwnd, ID_CPI_EDIT);
  wnd->measure_btn = CreateCtrl("BUTTON", "Measure", BS_PUSHBUTTON, 210, 44, 80,
                                26, wnd->hwnd, ID_MEASURE_BTN);

  CreateCtrl("BUTTON", "Data", BS_GROUPBOX, 10, 80, 470, 65, wnd->hwnd, 0);
  wnd->collect_btn = CreateCtrl("BUTTON", "Collect", BS_PUSHBUTTON, 20, 105, 80,
                                26, wnd->hwnd, ID_COLLECT_BTN);
  wnd->log_btn = CreateCtrl("BUTTON", "Start Log (F1)", BS_PUSHBUTTON, 110, 105,
                            120, 26, wnd->hwnd, ID_LOG_BTN);

  HWND combo = CreateCtrl("COMBOBOX", NULL, CBS_DROPDOWNLIST, 240, 106, 140,
                          300, wnd->hwnd, ID_TYPE_COMBO);
  const char *plots[] = {"Interval vs Time", "Frequency vs Time", "X Velocity",
                         "Y Velocity",       "XY Velocity",       "X Counts",
                         "Y Counts",         "XY Counts",         "X vs Y"};
  for (int i = 0; i < 9; i++)
    SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)plots[i]);
  SendMessage(combo, CB_SETCURSEL, 0, 0);

  wnd->plot_btn = CreateCtrl("BUTTON", "Plot", BS_PUSHBUTTON, 390, 105, 80, 26,
                             wnd->hwnd, ID_PLOT_BTN);

  CreateCtrl("BUTTON", "File", BS_GROUPBOX, 10, 155, 470, 50, wnd->hwnd, 0);
  wnd->load_btn = CreateCtrl("BUTTON", "Load", BS_PUSHBUTTON, 20, 175, 80, 26,
                             wnd->hwnd, ID_LOAD_BTN);
  wnd->save_btn = CreateCtrl("BUTTON", "Save", BS_PUSHBUTTON, 110, 175, 80, 26,
                             wnd->hwnd, ID_SAVE_BTN);

  wnd->status_text = CreateCtrl("EDIT", "Enter CPI or press Measure",
                                ES_MULTILINE | ES_READONLY | WS_VSCROLL, 10,
                                215, 470, 150, wnd->hwnd, 0);
  wnd->stats_text = CreateCtrl("STATIC", "", 0, 10, 375, 470, 60, wnd->hwnd, 0);

  EnumChildWindows(wnd->hwnd, (WNDENUMPROC)(void *)SendMessage, (LPARAM)hFont);
  return true;
}

void create_plot_window(HINSTANCE hInstance, MouseLog *log) {
  (void)hInstance;
  (void)log;
}

static void handle_measure_click(void) {
  update_status(g_main_wnd,
                "1. Press & hold left btn\r\n2. Move 10cm\r\n3. Release");
  mouse_log_clear(g_main_log);
  *g_main_state = STATE_MEASURE_WAIT;
}

static void handle_collect_click(void) {
  update_status(g_main_wnd,
                "1. Press & hold left btn\r\n2. Move mouse\r\n3. Release");
  mouse_log_clear(g_main_log);
  *g_main_state = STATE_COLLECT_WAIT;
}

static void handle_log_click(void) {
  if (*g_main_state == STATE_LOG) {
    ts_calc(g_main_log, g_main_freq);
    *g_main_state = STATE_IDLE;
    SetWindowText(g_main_wnd->log_btn, "Start Log (F1)");
    update_status(g_main_wnd, "Logging stopped");
    Statistics stats = calculate_interval_statistics(g_main_log, false);
    update_stats(g_main_wnd, &stats);
  } else {
    update_status(g_main_wnd, "Logging... Press Stop");
    mouse_log_clear(g_main_log);
    *g_main_state = STATE_LOG;
    SetWindowText(g_main_wnd->log_btn, "Stop (F1)");
  }
}

static void handle_plot_click(void) {
  if (g_main_log->event_count == 0) {
    MessageBox(g_main_wnd->hwnd, "No data.", "Error", MB_OK);
    return;
  }
  GetWindowText(g_main_wnd->desc_edit, g_main_log->desc, MAX_DESC_LEN);
  char buf[32];
  GetWindowText(g_main_wnd->cpi_edit, buf, 32);
  double cpi = atof(buf);
  if (cpi > 0)
    g_main_log->cpi = cpi;

  HWND combo = GetDlgItem(g_main_wnd->hwnd, ID_TYPE_COMBO);
  int sel = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);

  static const PlotType type_map[] = {PLOT_INTERVAL_VS_TIME,
                                      PLOT_FREQUENCY_VS_TIME,
                                      PLOT_X_VELOCITY_VS_TIME,
                                      PLOT_Y_VELOCITY_VS_TIME,
                                      PLOT_XY_VELOCITY_VS_TIME,
                                      PLOT_X_VS_TIME,
                                      PLOT_Y_VS_TIME,
                                      PLOT_XY_VS_TIME,
                                      PLOT_X_VS_Y};

  if (sel >= 0 && sel < 9)
    extract_and_plot(g_main_log, type_map[sel]);
}

static void handle_save_click(void) {
  OPENFILENAME ofn = {0};
  ofn.lStructSize = sizeof(ofn);
  char fn[MAX_PATH] = "mouse_log.csv";
  ofn.hwndOwner = g_main_wnd->hwnd;
  ofn.lpstrFile = fn;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = "CSV\0*.csv\0";
  ofn.Flags = OFN_OVERWRITEPROMPT;
  if (GetSaveFileName(&ofn)) {
    GetWindowText(g_main_wnd->desc_edit, g_main_log->desc, MAX_DESC_LEN);
    mouse_log_save(g_main_log, fn);
    update_status(g_main_wnd, "Saved");
  }
}

static void handle_load_click(void) {
  OPENFILENAME ofn = {0};
  ofn.lStructSize = sizeof(ofn);
  char fn[MAX_PATH] = "";
  ofn.hwndOwner = g_main_wnd->hwnd;
  ofn.lpstrFile = fn;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = "CSV\0*.csv\0";
  ofn.Flags = OFN_FILEMUSTEXIST;
  if (GetOpenFileName(&ofn) && mouse_log_load(g_main_log, fn)) {
    SetWindowText(g_main_wnd->desc_edit, g_main_log->desc);
    char buf[64];
    snprintf(buf, 64, "%.0f", g_main_log->cpi);
    SetWindowText(g_main_wnd->cpi_edit, buf);
    Statistics stats = calculate_interval_statistics(g_main_log, false);
    update_stats(g_main_wnd, &stats);
    update_status(g_main_wnd, "Loaded");
  }
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam) {
  switch (msg) {
  case WM_COMMAND:
    switch (LOWORD(wParam)) {
    case ID_MEASURE_BTN:
      handle_measure_click();
      break;
    case ID_COLLECT_BTN:
      handle_collect_click();
      break;
    case ID_LOG_BTN:
      handle_log_click();
      break;
    case ID_PLOT_BTN:
      handle_plot_click();
      break;
    case ID_SAVE_BTN:
      handle_save_click();
      break;
    case ID_LOAD_BTN:
      handle_load_click();
      break;
    }
    break;
  case WM_KEYDOWN:
    if (wParam == VK_F1)
      handle_log_click();
    if (wParam == VK_F3)
      handle_plot_click();
    break;
  case WM_CLOSE:
    DestroyWindow(hwnd);
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  return 0;
}