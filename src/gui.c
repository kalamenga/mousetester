#include "gui.h"
#include "mouse_log.h"
#include "statistics.h"
#include <stdio.h>
#include <math.h>

#define ID_DESC_EDIT 101
#define ID_CPI_EDIT 102
#define ID_MEASURE_BTN 103
#define ID_COLLECT_BTN 104
#define ID_LOG_BTN 105
#define ID_PLOT_BTN 106
#define ID_SAVE_BTN 107
#define ID_LOAD_BTN 108
#define ID_PLOT_COMBO 201
#define ID_START_EDIT 202
#define ID_END_EDIT 203
#define ID_LINES_CHECK 204
#define ID_STEM_CHECK 205

static MainWindow *g_main_wnd = NULL;
static MouseLog *g_main_log = NULL;
static AppState *g_main_state = NULL;
static LARGE_INTEGER *g_main_freq = NULL;

static void ts_calc(MouseLog *log, LARGE_INTEGER *freq);
static void handle_measure_click(void);
static void handle_collect_click(void);
static void handle_log_click(void);
static void handle_plot_click(void);
static void handle_save_click(void);
static void handle_load_click(void);
static LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK PlotWndProc(HWND, UINT, WPARAM, LPARAM);

bool create_main_window(HINSTANCE hInstance, MainWindow *wnd, MouseLog *log, 
                        AppState *state, LARGE_INTEGER *freq) {
    g_main_wnd = wnd;
    g_main_log = log;
    g_main_state = state;
    g_main_freq = freq;

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "MouseTesterMainClass";
    RegisterClassEx(&wc);

    wnd->hwnd = CreateWindowEx(0, "MouseTesterMainClass", "MouseTester v" VERSION,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 450, NULL, NULL, hInstance, NULL);

    if (!wnd->hwnd) return false;

    CreateWindow("STATIC", "Description:", WS_CHILD | WS_VISIBLE,
        10, 10, 80, 20, wnd->hwnd, NULL, hInstance, NULL);
    wnd->desc_edit = CreateWindow("EDIT", log->desc,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        100, 10, 370, 22, wnd->hwnd, (HMENU)ID_DESC_EDIT, hInstance, NULL);

    CreateWindow("STATIC", "CPI:", WS_CHILD | WS_VISIBLE,
        10, 45, 80, 20, wnd->hwnd, NULL, hInstance, NULL);
    wnd->cpi_edit = CreateWindow("EDIT", "400",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        100, 45, 100, 22, wnd->hwnd, (HMENU)ID_CPI_EDIT, hInstance, NULL);
    wnd->measure_btn = CreateWindow("BUTTON", "Measure",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        210, 45, 80, 25, wnd->hwnd, (HMENU)ID_MEASURE_BTN, hInstance, NULL);

    CreateWindow("BUTTON", "Mouse Data", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 80, 470, 60, wnd->hwnd, NULL, hInstance, NULL);
    wnd->collect_btn = CreateWindow("BUTTON", "Collect",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 100, 80, 25, wnd->hwnd, (HMENU)ID_COLLECT_BTN, hInstance, NULL);
    wnd->log_btn = CreateWindow("BUTTON", "Start Log (F1)",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        110, 100, 120, 25, wnd->hwnd, (HMENU)ID_LOG_BTN, hInstance, NULL);
    wnd->plot_btn = CreateWindow("BUTTON", "Plot (F3)",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        240, 100, 100, 25, wnd->hwnd, (HMENU)ID_PLOT_BTN, hInstance, NULL);

    CreateWindow("BUTTON", "File", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        10, 150, 470, 50, wnd->hwnd, NULL, hInstance, NULL);
    wnd->load_btn = CreateWindow("BUTTON", "Load",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 170, 80, 25, wnd->hwnd, (HMENU)ID_LOAD_BTN, hInstance, NULL);
    wnd->save_btn = CreateWindow("BUTTON", "Save",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        110, 170, 80, 25, wnd->hwnd, (HMENU)ID_SAVE_BTN, hInstance, NULL);

    wnd->status_text = CreateWindow("EDIT", "Enter CPI or press Measure",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        10, 210, 470, 150, wnd->hwnd, NULL, hInstance, NULL);

    wnd->stats_text = CreateWindow("STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        10, 370, 470, 20, wnd->hwnd, NULL, hInstance, NULL);

    ShowWindow(wnd->hwnd, SW_SHOW);
    UpdateWindow(wnd->hwnd);
    return true;
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_MEASURE_BTN: handle_measure_click(); break;
        case ID_COLLECT_BTN: handle_collect_click(); break;
        case ID_LOG_BTN: handle_log_click(); break;
        case ID_PLOT_BTN: handle_plot_click(); break;
        case ID_SAVE_BTN: handle_save_click(); break;
        case ID_LOAD_BTN: handle_load_click(); break;
        case ID_CPI_EDIT:
            if (HIWORD(wParam) == EN_CHANGE) {
                char buf[32];
                GetWindowText(g_main_wnd->cpi_edit, buf, sizeof(buf));
                g_main_log->cpi = atof(buf);
            }
            break;
        }
        break;
    case WM_KEYDOWN:
        if (wParam == VK_F1) handle_log_click();
        else if (wParam == VK_F3) handle_plot_click();
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

void update_status(MainWindow *wnd, const char *text) {
    SetWindowText(wnd->status_text, text);
}

void update_stats(MainWindow *wnd, const Statistics *stats) {
    char buf[256];
    snprintf(buf, sizeof(buf), 
        "Avg: %.2fms | StdDev: %.2fms | Range: %.2fms | 99%%: %.2fms",
        stats->avg, stats->stdev, stats->range, stats->percentile_99);
    SetWindowText(wnd->stats_text, buf);
}

static void ts_calc(MouseLog *log, LARGE_INTEGER *freq) {
    if (log->event_count == 0) return;
    int64_t min = log->events[0].pcounter;
    for (size_t i = 0; i < log->event_count; i++) {
        log->events[i].ts = (double)(log->events[i].pcounter - min) * 1000.0 / freq->QuadPart;
    }
}

static void handle_measure_click(void) {
    update_status(g_main_wnd, "1. Press and hold left button\r\n2. Move 10cm straight\r\n3. Release");
    mouse_log_clear(g_main_log);
    *g_main_state = STATE_MEASURE_WAIT;
}

static void handle_collect_click(void) {
    update_status(g_main_wnd, "1. Press and hold left button\r\n2. Move mouse\r\n3. Release");
    mouse_log_clear(g_main_log);
    *g_main_state = STATE_COLLECT_WAIT;
}

static void handle_log_click(void) {
    if (*g_main_state == STATE_LOG) {
        ts_calc(g_main_log, g_main_freq);
        *g_main_state = STATE_IDLE;
        SetWindowText(g_main_wnd->log_btn, "Start Log (F1)");
        char buf[512];
        snprintf(buf, sizeof(buf), "Logging stopped\r\nEvents: %zu", g_main_log->event_count);
        update_status(g_main_wnd, buf);
        Statistics stats = calculate_interval_statistics(g_main_log);
        update_stats(g_main_wnd, &stats);
    } else {
        update_status(g_main_wnd, "Logging... Press Stop to end");
        mouse_log_clear(g_main_log);
        *g_main_state = STATE_LOG;
        SetWindowText(g_main_wnd->log_btn, "Stop (F2)");
    }
}

static void handle_plot_click(void) {
    if (g_main_log->event_count > 0) {
        GetWindowText(g_main_wnd->desc_edit, g_main_log->desc, MAX_DESC_LEN);
        create_plot_window(GetModuleHandle(NULL), g_main_log);
    }
}

static void handle_save_click(void) {
    OPENFILENAME ofn = {0};
    char filename[MAX_PATH] = "mouse_log.csv";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_main_wnd->hwnd;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "CSV Files\0*.csv\0All Files\0*.*\0";
    ofn.Flags = OFN_OVERWRITEPROMPT;

    if (GetSaveFileName(&ofn)) {
        GetWindowText(g_main_wnd->desc_edit, g_main_log->desc, MAX_DESC_LEN);
        mouse_log_save(g_main_log, filename);
        update_status(g_main_wnd, "File saved");
    }
}

static void handle_load_click(void) {
    OPENFILENAME ofn = {0};
    char filename[MAX_PATH] = "";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_main_wnd->hwnd;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "CSV Files\0*.csv\0All Files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn)) {
        if (mouse_log_load(g_main_log, filename)) {
            SetWindowText(g_main_wnd->desc_edit, g_main_log->desc);
            char buf[64];
            snprintf(buf, sizeof(buf), "%.1f", g_main_log->cpi);
            SetWindowText(g_main_wnd->cpi_edit, buf);
            snprintf(buf, sizeof(buf), "Loaded: %zu events", g_main_log->event_count);
            update_status(g_main_wnd, buf);
            Statistics stats = calculate_interval_statistics(g_main_log);
            update_stats(g_main_wnd, &stats);
        }
    }
}

static void draw_plot(HDC hdc, RECT *area, PlotWindow *pw) {
    if (!pw->log || pw->log->event_count == 0) return;

    int margin = 70;
    int w = area->right - margin * 2;
    int h = area->bottom - margin * 2;

    double x_min = INFINITY, x_max = -INFINITY;
    double y_min = INFINITY, y_max = -INFINITY;

    for (size_t i = pw->start_idx; i <= pw->end_idx && i < pw->log->event_count; i++) {
        double x = pw->log->events[i].ts;
        double y = 0;
        switch (pw->type) {
        case PLOT_X_VS_TIME: y = pw->log->events[i].last_x; break;
        case PLOT_Y_VS_TIME: y = pw->log->events[i].last_y; break;
        case PLOT_INTERVAL_VS_TIME:
            y = i > 0 ? pw->log->events[i].ts - pw->log->events[i-1].ts : 0;
            break;
        case PLOT_FREQUENCY_VS_TIME: {
            double dt = i > 0 ? pw->log->events[i].ts - pw->log->events[i-1].ts : 1;
            y = dt > 0 ? 1000.0 / dt : 0;
            break;
        }
        case PLOT_X_VELOCITY_VS_TIME:
            if (i > 0 && pw->log->cpi > 0) {
                double dt = pw->log->events[i].ts - pw->log->events[i-1].ts;
                y = dt > 0 ? pw->log->events[i].last_x / dt / pw->log->cpi * 25.4 : 0;
            }
            break;
        case PLOT_Y_VELOCITY_VS_TIME:
            if (i > 0 && pw->log->cpi > 0) {
                double dt = pw->log->events[i].ts - pw->log->events[i-1].ts;
                y = dt > 0 ? pw->log->events[i].last_y / dt / pw->log->cpi * 25.4 : 0;
            }
            break;
        default: break;
        }
        if (x < x_min) x_min = x;
        if (x > x_max) x_max = x;
        if (y < y_min) y_min = y;
        if (y > y_max) y_max = y;
    }

    double x_range = x_max - x_min > 0 ? x_max - x_min : 1;
    double y_range = y_max - y_min > 0 ? y_max - y_min : 1;

    HPEN grid_pen = CreatePen(PS_DOT, 1, RGB(220, 220, 220));
    SelectObject(hdc, grid_pen);
    for (int i = 0; i <= 10; i++) {
        int x = margin + (w * i) / 10;
        int y = margin + (h * i) / 10;
        MoveToEx(hdc, x, margin, NULL);
        LineTo(hdc, x, margin + h);
        MoveToEx(hdc, margin, y, NULL);
        LineTo(hdc, margin + w, y);
    }
    DeleteObject(grid_pen);

    HPEN axis_pen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
    SelectObject(hdc, axis_pen);
    MoveToEx(hdc, margin, margin, NULL);
    LineTo(hdc, margin, margin + h);
    LineTo(hdc, margin + w, margin + h);
    DeleteObject(axis_pen);

    HBRUSH point_brush = CreateSolidBrush(RGB(0, 100, 255));
    SelectObject(hdc, point_brush);
    HPEN point_pen = CreatePen(PS_SOLID, 1, RGB(0, 100, 255));
    SelectObject(hdc, point_pen);

    int prev_px = 0, prev_py = 0;
    for (size_t i = pw->start_idx; i <= pw->end_idx && i < pw->log->event_count; i++) {
        double x = pw->log->events[i].ts;
        double y = 0;
        switch (pw->type) {
        case PLOT_X_VS_TIME: y = pw->log->events[i].last_x; break;
        case PLOT_Y_VS_TIME: y = pw->log->events[i].last_y; break;
        case PLOT_INTERVAL_VS_TIME:
            y = i > 0 ? pw->log->events[i].ts - pw->log->events[i-1].ts : 0;
            break;
        case PLOT_FREQUENCY_VS_TIME: {
            double dt = i > 0 ? pw->log->events[i].ts - pw->log->events[i-1].ts : 1;
            y = dt > 0 ? 1000.0 / dt : 0;
            break;
        }
        case PLOT_X_VELOCITY_VS_TIME:
            if (i > 0 && pw->log->cpi > 0) {
                double dt = pw->log->events[i].ts - pw->log->events[i-1].ts;
                y = dt > 0 ? pw->log->events[i].last_x / dt / pw->log->cpi * 25.4 : 0;
            }
            break;
        case PLOT_Y_VELOCITY_VS_TIME:
            if (i > 0 && pw->log->cpi > 0) {
                double dt = pw->log->events[i].ts - pw->log->events[i-1].ts;
                y = dt > 0 ? pw->log->events[i].last_y / dt / pw->log->cpi * 25.4 : 0;
            }
            break;
        default: break;
        }

        int px = margin + (int)((x - x_min) / x_range * w);
        int py = margin + h - (int)((y - y_min) / y_range * h);

        Ellipse(hdc, px - 2, py - 2, px + 2, py + 2);

        if (pw->show_lines && i > pw->start_idx) {
            MoveToEx(hdc, prev_px, prev_py, NULL);
            LineTo(hdc, px, py);
        }
        if (pw->show_stem) {
            MoveToEx(hdc, px, margin + h, NULL);
            LineTo(hdc, px, py);
        }
        prev_px = px;
        prev_py = py;
    }

    DeleteObject(point_brush);
    DeleteObject(point_pen);

    SetBkMode(hdc, TRANSPARENT);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f", x_min);
    TextOut(hdc, margin - 10, margin + h + 5, buf, strlen(buf));
    snprintf(buf, sizeof(buf), "%.1f", x_max);
    TextOut(hdc, margin + w - 30, margin + h + 5, buf, strlen(buf));
    snprintf(buf, sizeof(buf), "%.2f", y_max);
    TextOut(hdc, 5, margin - 5, buf, strlen(buf));
    snprintf(buf, sizeof(buf), "%.2f", y_min);
    TextOut(hdc, 5, margin + h - 10, buf, strlen(buf));

    const char *xlabel = "Time (ms)";
    const char *ylabel = "Value";
    switch (pw->type) {
    case PLOT_INTERVAL_VS_TIME: ylabel = "Interval (ms)"; break;
    case PLOT_FREQUENCY_VS_TIME: ylabel = "Frequency (Hz)"; break;
    case PLOT_X_VELOCITY_VS_TIME: ylabel = "X Velocity (m/s)"; break;
    case PLOT_Y_VELOCITY_VS_TIME: ylabel = "Y Velocity (m/s)"; break;
    default: break;
    }
    TextOut(hdc, margin + w/2 - 30, margin + h + 35, xlabel, strlen(xlabel));
    TextOut(hdc, 10, margin + h/2, ylabel, strlen(ylabel));
}

static LRESULT CALLBACK PlotWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PlotWindow *pw = (PlotWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT *cs = (CREATESTRUCT*)lParam;
        pw = calloc(1, sizeof(PlotWindow));
        pw->log = (MouseLog*)cs->lpCreateParams;
        pw->type = PLOT_INTERVAL_VS_TIME;
        pw->start_idx = 0;
        pw->end_idx = pw->log->event_count > 0 ? pw->log->event_count - 1 : 0;
        pw->start_time = 0;
        pw->end_time = pw->log->event_count > 0 ? pw->log->events[pw->end_idx].ts : 100;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pw);

        pw->plot_combo = CreateWindow("COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            10, 10, 200, 200, hwnd, (HMENU)ID_PLOT_COMBO, GetModuleHandle(NULL), NULL);
        SendMessage(pw->plot_combo, CB_ADDSTRING, 0, (LPARAM)"xCount vs Time");
        SendMessage(pw->plot_combo, CB_ADDSTRING, 0, (LPARAM)"yCount vs Time");
        SendMessage(pw->plot_combo, CB_ADDSTRING, 0, (LPARAM)"Interval vs Time");
        SendMessage(pw->plot_combo, CB_ADDSTRING, 0, (LPARAM)"Frequency vs Time");
        SendMessage(pw->plot_combo, CB_ADDSTRING, 0, (LPARAM)"xVelocity vs Time");
        SendMessage(pw->plot_combo, CB_ADDSTRING, 0, (LPARAM)"yVelocity vs Time");
        SendMessage(pw->plot_combo, CB_SETCURSEL, 2, 0);

        pw->lines_check = CreateWindow("BUTTON", "Lines", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            220, 12, 70, 20, hwnd, (HMENU)ID_LINES_CHECK, GetModuleHandle(NULL), NULL);
        pw->stem_check = CreateWindow("BUTTON", "Stem", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            300, 12, 70, 20, hwnd, (HMENU)ID_STEM_CHECK, GetModuleHandle(NULL), NULL);
        break;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_PLOT_COMBO && HIWORD(wParam) == CBN_SELCHANGE) {
            int sel = SendMessage(pw->plot_combo, CB_GETCURSEL, 0, 0);
            pw->type = (PlotType)(sel < 2 ? sel : sel + 1);
            InvalidateRect(hwnd, NULL, TRUE);
        } else if (LOWORD(wParam) == ID_LINES_CHECK) {
            pw->show_lines = SendMessage(pw->lines_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
            InvalidateRect(hwnd, NULL, TRUE);
        } else if (LOWORD(wParam) == ID_STEM_CHECK) {
            pw->show_stem = SendMessage(pw->stem_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT client;
        GetClientRect(hwnd, &client);
        client.top = 50;
        FillRect(hdc, &client, (HBRUSH)(COLOR_WINDOW + 1));
        draw_plot(hdc, &client, pw);
        EndPaint(hwnd, &ps);
        break;
    }
    case WM_SIZE:
        InvalidateRect(hwnd, NULL, TRUE);
        break;
    case WM_DESTROY:
        free(pw);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void create_plot_window(HINSTANCE hInstance, MouseLog *log) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEX wc = {0};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpfnWndProc = PlotWndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = "MouseTesterPlotClass";
        RegisterClassEx(&wc);
        registered = true;
    }

    char title[256];
    snprintf(title, sizeof(title), "MousePlot - %s (%.1f CPI)", log->desc, log->cpi);
    HWND hwnd = CreateWindowEx(0, "MouseTesterPlotClass", title,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1024, 768,
        NULL, NULL, hInstance, log);
    ShowWindow(hwnd, SW_SHOW);
}