#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <windows.h>

#include "types.h"
#include "mouse_log.h"
#include "statistics.h"
#include "gui.h"

#define RIDEV_INPUTSINK 0x00000100
#define RID_INPUT 0x10000003
#define WM_INPUT 0x00FF
#define RIM_TYPEMOUSE 0

static MouseLog g_log;
static AppState g_state = STATE_IDLE;
static LARGE_INTEGER g_freq;
static MainWindow g_main_wnd;

static void ts_calc(MouseLog *log) {
    if (log->event_count == 0) return;
    int64_t min = log->events[0].pcounter;
    for (size_t i = 0; i < log->event_count; i++) {
        log->events[i].ts = (double)(log->events[i].pcounter - min) * 1000.0 / g_freq.QuadPart;
    }
}

static void process_mouse_event(MouseEvent event) {
    switch (g_state) {
    case STATE_MEASURE_WAIT:
        if (event.button_flags == 0x0001) {
            mouse_log_add(&g_log, event);
            update_status(&g_main_wnd, "Measuring...");
            g_state = STATE_MEASURE;
        }
        break;

    case STATE_MEASURE:
        mouse_log_add(&g_log, event);
        if (event.button_flags == 0x0002) {
            double x = 0.0, y = 0.0;
            for (size_t i = 0; i < g_log.event_count; i++) {
                x += g_log.events[i].last_x;
                y += g_log.events[i].last_y;
            }
            ts_calc(&g_log);
            g_log.cpi = sqrt(x*x + y*y) / (10.0 / 2.54);

            char buf[128];
            snprintf(buf, sizeof(buf), "Measured CPI: %.1f\r\nReady for collection", g_log.cpi);
            update_status(&g_main_wnd, buf);

            char cpi_buf[32];
            snprintf(cpi_buf, sizeof(cpi_buf), "%.1f", g_log.cpi);
            SetWindowText(g_main_wnd.cpi_edit, cpi_buf);

            g_state = STATE_IDLE;
        }
        break;

    case STATE_COLLECT_WAIT:
        if (event.button_flags == 0x0001) {
            mouse_log_add(&g_log, event);
            update_status(&g_main_wnd, "Collecting...");
            g_state = STATE_COLLECT;
        }
        break;

    case STATE_COLLECT:
        mouse_log_add(&g_log, event);
        if (event.button_flags == 0x0002) {
            ts_calc(&g_log);
            int32_t dx = mouse_log_delta_x(&g_log);
            int32_t dy = mouse_log_delta_y(&g_log);
            double path = mouse_log_path(&g_log);

            char buf[512];
            snprintf(buf, sizeof(buf),
                "Collection complete\r\nEvents: %zu\r\n"
                "Sum X: %d counts (%.1f cm)\r\n"
                "Sum Y: %d counts (%.1f cm)\r\n"
                "Path: %.0f counts (%.1f cm)",
                g_log.event_count, dx, fabs(dx / g_log.cpi * 2.54),
                dy, fabs(dy / g_log.cpi * 2.54),
                path, path / g_log.cpi * 2.54);
            update_status(&g_main_wnd, buf);

            Statistics stats = calculate_interval_statistics(&g_log);
            update_stats(&g_main_wnd, &stats);

            g_state = STATE_IDLE;
        }
        break;

    case STATE_LOG:
        mouse_log_add(&g_log, event);
        break;

    default:
        break;
    }
}

static LRESULT CALLBACK RawInputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INPUT) {
        UINT size;
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
        BYTE *buffer = malloc(size);
        if (buffer && GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER)) == size) {
            RAWINPUT *raw = (RAWINPUT*)buffer;
            if (raw->header.dwType == RIM_TYPEMOUSE) {
                LARGE_INTEGER counter;
                QueryPerformanceCounter(&counter);
                MouseEvent event = {
                    raw->data.mouse.usButtonFlags,
                    raw->data.mouse.lLastX,
                    -raw->data.mouse.lLastY,
                    counter.QuadPart,
                    0.0
                };
                process_mouse_event(event);
            }
        }
        free(buffer);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static bool register_raw_input(HWND hwnd) {
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 1;
    rid.usUsage = 2;
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = hwnd;
    return RegisterRawInputDevices(&rid, 1, sizeof(rid)) != FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    QueryPerformanceFrequency(&g_freq);
    mouse_log_init(&g_log);

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = RawInputWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "MouseTesterRawInputClass";
    RegisterClassEx(&wc);

    HWND raw_hwnd = CreateWindowEx(0, "MouseTesterRawInputClass", "", 0,
                                   0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    if (!raw_hwnd || !register_raw_input(raw_hwnd)) {
        MessageBox(NULL, "Failed to initialize raw input", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!create_main_window(hInstance, &g_main_wnd, &g_log, &g_state, &g_freq)) {
        MessageBox(NULL, "Failed to create window", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    mouse_log_free(&g_log);
    return (int)msg.wParam;
}