#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "gui.h"
#include "mouse_log.h"
#include "statistics.h"
#include "types.h"

#ifndef RIDEV_INPUTSINK
#define RIDEV_INPUTSINK 0x00000100
#endif

#ifndef RID_INPUT
#define RID_INPUT 0x10000003
#endif

#ifndef WM_INPUT
#define WM_INPUT 0x00FF
#endif

#ifndef RIM_TYPEMOUSE
#define RIM_TYPEMOUSE 0
#endif

#ifndef RI_MOUSE_LEFT_BUTTON_DOWN
#define RI_MOUSE_LEFT_BUTTON_DOWN 0x0001
#endif
#ifndef RI_MOUSE_LEFT_BUTTON_UP
#define RI_MOUSE_LEFT_BUTTON_UP 0x0002
#endif
#ifndef RI_MOUSE_RIGHT_BUTTON_DOWN
#define RI_MOUSE_RIGHT_BUTTON_DOWN 0x0004
#endif
#ifndef RI_MOUSE_RIGHT_BUTTON_UP
#define RI_MOUSE_RIGHT_BUTTON_UP 0x0008
#endif

static MouseLog g_log;
static AppState g_state = STATE_IDLE;
static LARGE_INTEGER g_freq;
static MainWindow g_main_wnd;

static void ts_calc(MouseLog *log) {
  if (log->event_count == 0)
    return;
  int64_t min = log->events[0].pcounter;

  double inv_freq_ms = 1000.0 / (double)g_freq.QuadPart;

  for (size_t i = 0; i < log->event_count; i++) {
    log->events[i].ts = (double)(log->events[i].pcounter - min) * inv_freq_ms;
  }
}

static void process_mouse_event(MouseEvent event) {
  switch (g_state) {
  case STATE_MEASURE_WAIT:
    if (event.button_flags & RI_MOUSE_LEFT_BUTTON_DOWN) {
      mouse_log_add(&g_log, event);
      update_status(&g_main_wnd, "Measuring... Move 10cm");
      g_state = STATE_MEASURE;
    }
    break;

  case STATE_MEASURE:
    mouse_log_add(&g_log, event);
    if (event.button_flags & RI_MOUSE_LEFT_BUTTON_UP) {
      double x = 0.0, y = 0.0;
      for (size_t i = 0; i < g_log.event_count; i++) {
        x += g_log.events[i].last_x;
        y += g_log.events[i].last_y;
      }
      ts_calc(&g_log);

      double distance_cm = 10.0;
      double counts = sqrt(x * x + y * y);

      g_log.cpi = round(counts / (distance_cm / 2.54));

      char buf[128];
      snprintf(buf, sizeof(buf), "Measured: %.1f CPI", g_log.cpi);
      update_status(&g_main_wnd, buf);

      char cpi_buf[32];
      snprintf(cpi_buf, sizeof(cpi_buf), "%.0f", g_log.cpi);
      SetWindowText(g_main_wnd.cpi_edit, cpi_buf);

      g_state = STATE_IDLE;
    }
    break;

  case STATE_COLLECT_WAIT:
    if (event.button_flags & RI_MOUSE_LEFT_BUTTON_DOWN) {
      mouse_log_add(&g_log, event);
      update_status(&g_main_wnd, "Collecting...");
      g_state = STATE_COLLECT;
    }
    break;

  case STATE_COLLECT:
    mouse_log_add(&g_log, event);
    if (event.button_flags & RI_MOUSE_LEFT_BUTTON_UP) {
      ts_calc(&g_log);
      int32_t dx = mouse_log_delta_x(&g_log);
      int32_t dy = mouse_log_delta_y(&g_log);
      double path = mouse_log_path(&g_log);

      double safe_cpi = g_log.cpi > 0 ? g_log.cpi : 400.0;

      char buf[512];
      snprintf(buf, sizeof(buf),
               "Collection complete\r\nEvents: %zu\r\n"
               "X: %d (%.1f cm) Y: %d (%.1f cm)\r\n"
               "Path: %.0f counts (%.1f cm)",
               g_log.event_count, dx, fabs(dx / safe_cpi * 2.54), dy,
               fabs(dy / safe_cpi * 2.54), path, path / safe_cpi * 2.54);
      update_status(&g_main_wnd, buf);

      Statistics stats = calculate_interval_statistics(&g_log, false);
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

    union {
      RAWINPUT raw;
      char padding[sizeof(RAWINPUT) + 16];
    } buffer;

    UINT size = sizeof(buffer);

    UINT bytes_read = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &buffer,
                                      &size, sizeof(RAWINPUTHEADER));

    if (bytes_read > 0 && bytes_read != (UINT)-1) {

      if (buffer.raw.header.dwType == RIM_TYPEMOUSE) {
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);

        MouseEvent event = {
            buffer.raw.data.mouse.usButtonFlags, buffer.raw.data.mouse.lLastX,
            buffer.raw.data.mouse.lLastY, counter.QuadPart, 0.0};
        process_mouse_event(event);
      }
    }
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine,
        int nCmdShow) {
  (void)hPrevInstance;
  (void)lpCmdLine;
  (void)nCmdShow;

  SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

  DWORD_PTR affinityMask = 2;
  DWORD_PTR processAffinityMask, systemAffinityMask;
  if (GetProcessAffinityMask(GetCurrentProcess(), &processAffinityMask,
                             &systemAffinityMask)) {
    if (systemAffinityMask & affinityMask) {
      SetProcessAffinityMask(GetCurrentProcess(), affinityMask);
    }
  }

  if (!QueryPerformanceFrequency(&g_freq)) {
    MessageBox(NULL, "High resolution timer not supported", "Error",
               MB_OK | MB_ICONERROR);
    return 1;
  }

  mouse_log_init(&g_log);

  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.lpfnWndProc = RawInputWndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = "MouseTesterRawInputClass";

  if (!RegisterClassEx(&wc)) {
    MessageBox(NULL, "Failed to register window class", "Error",
               MB_OK | MB_ICONERROR);
    return 1;
  }

  HWND raw_hwnd = CreateWindowEx(0, "MouseTesterRawInputClass", "", 0, 0, 0, 0,
                                 0, HWND_MESSAGE, NULL, hInstance, NULL);

  if (!raw_hwnd) {
    MessageBox(NULL, "Failed to create message window", "Error",
               MB_OK | MB_ICONERROR);
    return 1;
  }

  if (!register_raw_input(raw_hwnd)) {
    MessageBox(NULL, "Failed to init raw input", "Error", MB_OK | MB_ICONERROR);
    return 1;
  }

  if (!create_main_window(hInstance, &g_main_wnd, &g_log, &g_state, &g_freq)) {
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