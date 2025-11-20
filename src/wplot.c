#include "wplot.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>

typedef void *GpGraphics;
typedef void *GpPen;
typedef void *GpBrush;
typedef void *GpFont;
typedef void *GpFontFamily;
typedef void *GpBitmap;
typedef void *GpStringFormat;
typedef struct {
  float x, y;
} GpPointF;
typedef struct {
  float x, y, w, h;
} GpRectF;
typedef struct {
  unsigned long GdiplusVersion;
  void *DbgEventCallback;
  int SuppressBackgroundThread;
  int SuppressExternalCodecs;
} GdiplusStartupInput;

typedef int(__stdcall *F_Startup)(ULONG_PTR *, const GdiplusStartupInput *,
                                  void *);
typedef int(__stdcall *F_CreateFromHDC)(HDC, GpGraphics *);
typedef int(__stdcall *F_CreateFromImage)(GpBitmap, GpGraphics *);
typedef int(__stdcall *F_DeleteG)(GpGraphics);
typedef int(__stdcall *F_CreateBitmap)(int, int, int, int, void *, GpBitmap *);
typedef int(__stdcall *F_DisposeImage)(GpBitmap);
typedef int(__stdcall *F_DrawImage)(GpGraphics, GpBitmap, int, int);
typedef int(__stdcall *F_CreatePen)(unsigned int, float, int, GpPen *);
typedef int(__stdcall *F_DeletePen)(GpPen);
typedef int(__stdcall *F_CreateBrush)(unsigned int, GpBrush *);
typedef int(__stdcall *F_DeleteBrush)(GpBrush);
typedef int(__stdcall *F_FillEllipse)(GpGraphics, GpBrush, float, float, float,
                                      float);
typedef int(__stdcall *F_FillRect)(GpGraphics, GpBrush, float, float, float,
                                   float);
typedef int(__stdcall *F_DrawLines)(GpGraphics, GpPen, const GpPointF *, int);
typedef int(__stdcall *F_DrawCurve)(GpGraphics, GpPen, const GpPointF *, int,
                                    float);
typedef int(__stdcall *F_DrawLine)(GpGraphics, GpPen, float, float, float,
                                   float);
typedef int(__stdcall *F_SetSmooth)(GpGraphics, int);
typedef int(__stdcall *F_SetTextHint)(GpGraphics, int);
typedef int(__stdcall *F_Clear)(GpGraphics, unsigned int);
typedef int(__stdcall *F_CreateFamily)(const WCHAR *, void *, GpFontFamily *);
typedef int(__stdcall *F_CreateFont)(GpFontFamily, float, int, int, GpFont *);
typedef int(__stdcall *F_DrawString)(GpGraphics, const WCHAR *, int, GpFont,
                                     const GpRectF *, void *, GpBrush);
typedef int(__stdcall *F_DeleteFont)(GpFont);
typedef int(__stdcall *F_DeleteFontFamily)(GpFontFamily);
typedef int(__stdcall *F_SetClip)(GpGraphics, float, float, float, float, int);
typedef int(__stdcall *F_ResetClip)(GpGraphics);
typedef int(__stdcall *F_CreateStringFormat)(int, int, GpStringFormat *);
typedef int(__stdcall *F_DeleteStringFormat)(GpStringFormat);
typedef int(__stdcall *F_SetStringFormatAlign)(GpStringFormat, int);
typedef int(__stdcall *F_SetStringFormatLineAlign)(GpStringFormat, int);
typedef int(__stdcall *F_SetPenWidth)(GpPen, float);
typedef int(__stdcall *F_DrawRect)(GpGraphics, GpPen, float, float, float,
                                   float);

static struct {
  HMODULE dll;
  ULONG_PTR token;
  F_Startup Startup;
  F_CreateFromHDC CreateFromHDC;
  F_CreateFromImage GetImageGraphicsContext;
  F_DeleteG DeleteGraphics;
  F_CreateBitmap CreateBitmapFromScan0;
  F_DisposeImage DisposeImage;
  F_DrawImage DrawImageI;
  F_CreatePen CreatePen1;
  F_DeletePen DeletePen;
  F_CreateBrush CreateSolidFill;
  F_DeleteBrush DeleteBrush;
  F_FillEllipse FillEllipse;
  F_FillRect FillRectangle;
  F_DrawLines DrawLines;
  F_DrawCurve DrawCurve;
  F_DrawLine DrawLine;
  F_SetSmooth SetSmoothingMode;
  F_SetTextHint SetTextRenderingHint;
  F_Clear GraphicsClear;
  F_CreateFamily CreateFontFamilyFromName;
  F_CreateFont CreateFont;
  F_DrawString DrawString;
  F_DeleteFont DeleteFont;
  F_DeleteFontFamily DeleteFontFamily;
  F_SetClip SetClipRect;
  F_ResetClip ResetClip;
  F_CreateStringFormat CreateStringFormat;
  F_DeleteStringFormat DeleteStringFormat;
  F_SetStringFormatAlign SetStringFormatAlign;
  F_SetStringFormatLineAlign SetStringFormatLineAlign;
  F_SetPenWidth SetPenWidth;
  F_DrawRect DrawRectangle;
} gp;

static void load_gdiplus(void) {
  if (gp.token)
    return;
  gp.dll = LoadLibraryA("gdiplus.dll");
  if (!gp.dll)
    return;
  gp.Startup = (F_Startup)(void *)GetProcAddress(gp.dll, "GdiplusStartup");
#define GET(name) gp.name = (void *)GetProcAddress(gp.dll, "Gdip" #name)
  GET(CreateFromHDC);
  GET(GetImageGraphicsContext);
  GET(DeleteGraphics);
  GET(CreateBitmapFromScan0);
  GET(DisposeImage);
  GET(DrawImageI);
  GET(CreatePen1);
  GET(DeletePen);
  GET(CreateSolidFill);
  GET(DeleteBrush);
  GET(FillEllipse);
  GET(FillRectangle);
  GET(DrawLines);
  GET(DrawCurve);
  GET(DrawLine);
  GET(SetSmoothingMode);
  GET(SetTextRenderingHint);
  GET(GraphicsClear);
  GET(CreateFontFamilyFromName);
  GET(CreateFont);
  GET(DrawString);
  GET(DeleteFont);
  GET(DeleteFontFamily);
  GET(SetClipRect);
  GET(ResetClip);
  GET(CreateStringFormat);
  GET(DeleteStringFormat);
  GET(SetStringFormatAlign);
  GET(SetStringFormatLineAlign);
  GET(SetPenWidth);
  GET(DrawRectangle);
  GdiplusStartupInput input = {1, 0, 0, 0};
  if (gp.Startup)
    gp.Startup(&gp.token, &input, 0);
}

static double remove_noise(double x) {
  if (fabs(x) < 1e-14)
    return 0.0;
  return x;
}

static double oxy_calculate_interval(double min, double max,
                                     double available_size,
                                     double max_interval_size) {
  double range = fabs(max - min);
  if (range <= 0)
    return 1.0;
  double target_count = available_size / max_interval_size;
  if (target_count < 2)
    target_count = 2;
  double exponent = floor(log10(range));
  double interval = pow(10.0, exponent);
  double interval_candidate = interval;

  while (1) {
    double m = interval_candidate / pow(10.0, floor(log10(interval_candidate)));
    if (fabs(m - 5.0) < 0.1)
      interval_candidate /= 2.5;
    else if (fabs(m - 2.0) < 0.1 || fabs(m - 1.0) < 0.1 || fabs(m - 10.0) < 0.1)
      interval_candidate /= 2.0;
    else
      interval_candidate /= 2.0;
    interval_candidate = remove_noise(interval_candidate);
    if (range / interval >= target_count * 0.5 &&
        range / interval_candidate > target_count)
      break;
    if (interval_candidate == 0)
      break;
    interval = interval_candidate;
  }
  return interval;
}

typedef struct {
  double *x, *y;
  int count;
  WPlotType type;
  unsigned int color;
  float thickness;
} Series;

typedef struct {
  GpRectF rect;
  const char *label;
  bool hover;
} GraphBtn;

struct wplot_ctx {
  char title[128];
  int width, height;
  Series *series;
  int series_count;
  int series_cap;
  double data_min_x, data_max_x, data_min_y, data_max_y;
  double view_min_x, view_max_x, view_min_y, view_max_y;
  bool is_dragging;
  int last_mouse_x, last_mouse_y;
  GpBitmap backbuffer;
  int bb_width, bb_height;
  bool dirty;
  GraphBtn btn_config;
  GraphBtn btn_range;
  POINT cursor_pos;
};

static double g_dlg_start, g_dlg_end;
static bool g_dlg_ok;

static LRESULT CALLBACK RangeDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
  case WM_CREATE:
    CreateWindow("STATIC", "Start (ms):", WS_CHILD | WS_VISIBLE, 10, 10, 80, 20,
                 hwnd, 0, 0, 0);
    CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                 100, 10, 100, 20, hwnd, (HMENU)1, 0, 0);
    CreateWindow("STATIC", "End (ms):", WS_CHILD | WS_VISIBLE, 10, 40, 80, 20,
                 hwnd, 0, 0, 0);
    CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                 100, 40, 100, 20, hwnd, (HMENU)2, 0, 0);
    CreateWindow("BUTTON", "Apply", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 50,
                 80, 60, 25, hwnd, (HMENU)IDOK, 0, 0);
    CreateWindow("BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 130,
                 80, 60, 25, hwnd, (HMENU)IDCANCEL, 0, 0);
    char buf[32];
    snprintf(buf, 32, "%.2f", g_dlg_start);
    SetDlgItemText(hwnd, 1, buf);
    snprintf(buf, 32, "%.2f", g_dlg_end);
    SetDlgItemText(hwnd, 2, buf);
    return 0;
  case WM_COMMAND:
    if (LOWORD(wp) == IDOK) {
      char b1[32], b2[32];
      GetDlgItemText(hwnd, 1, b1, 32);
      GetDlgItemText(hwnd, 2, b2, 32);
      g_dlg_start = atof(b1);
      g_dlg_end = atof(b2);
      g_dlg_ok = true;
      DestroyWindow(hwnd);
    } else if (LOWORD(wp) == IDCANCEL) {
      g_dlg_ok = false;
      DestroyWindow(hwnd);
    }
    return 0;
  }
  return DefWindowProc(hwnd, msg, wp, lp);
}

static void prompt_range(HWND parent, wplot_ctx *ctx) {
  WNDCLASSA wc = {0};
  wc.lpfnWndProc = RangeDlgProc;
  wc.hInstance = GetModuleHandle(0);
  wc.lpszClassName = "RangeDlg";
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  RegisterClassA(&wc);

  g_dlg_start = ctx->view_min_x;
  g_dlg_end = ctx->view_max_x;
  g_dlg_ok = false;

  HWND hDlg =
      CreateWindowEx(WS_EX_DLGMODALFRAME, "RangeDlg", "Set Time Range",
                     WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 0, 0, 250,
                     150, parent, NULL, GetModuleHandle(0), NULL);

  RECT rcOwner, rcDlg;
  GetWindowRect(parent, &rcOwner);
  GetWindowRect(hDlg, &rcDlg);
  SetWindowPos(hDlg, 0, rcOwner.left + (rcOwner.right - rcOwner.left) / 2 - 125,
               rcOwner.top + (rcOwner.bottom - rcOwner.top) / 2 - 75, 0, 0,
               SWP_NOSIZE | SWP_NOZORDER);

  EnableWindow(parent, FALSE);
  MSG msg;
  while (IsWindow(hDlg) && GetMessage(&msg, 0, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  EnableWindow(parent, TRUE);
  SetForegroundWindow(parent);

  if (g_dlg_ok && g_dlg_end > g_dlg_start) {
    ctx->view_min_x = g_dlg_start;
    ctx->view_max_x = g_dlg_end;
    ctx->dirty = true;
  }
}

static void prompt_color(HWND hwnd, wplot_ctx *ctx, int series_idx) {
  CHOOSECOLOR cc = {0};
  static COLORREF custom_colors[16];
  unsigned int argb = ctx->series[series_idx].color;
  COLORREF color = RGB((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF);

  cc.lStructSize = sizeof(cc);
  cc.hwndOwner = hwnd;
  cc.lpCustColors = custom_colors;
  cc.rgbResult = color;
  cc.Flags = CC_FULLOPEN | CC_RGBINIT;

  if (ChooseColor(&cc)) {
    ctx->series[series_idx].color = 0xFF000000 | GetRValue(cc.rgbResult) << 16 |
                                    GetGValue(cc.rgbResult) << 8 |
                                    GetBValue(cc.rgbResult);
    ctx->dirty = true;
  }
}

wplot_ctx *wplot_create(const char *title, int width, int height) {
  load_gdiplus();
  wplot_ctx *ctx = (wplot_ctx *)calloc(1, sizeof(wplot_ctx));
  strncpy(ctx->title, title, 127);
  ctx->width = width;
  ctx->height = height;
  ctx->data_min_x = DBL_MAX;
  ctx->data_max_x = -DBL_MAX;
  ctx->data_min_y = DBL_MAX;
  ctx->data_max_y = -DBL_MAX;
  ctx->dirty = true;
  return ctx;
}

void wplot_add(wplot_ctx *ctx, double *x, double *y, int count, WPlotType type,
               unsigned int color, float thickness) {
  if (count <= 0)
    return;
  if (ctx->series_count >= ctx->series_cap) {
    ctx->series_cap = (ctx->series_cap == 0) ? 4 : ctx->series_cap * 2;
    ctx->series =
        (Series *)realloc(ctx->series, ctx->series_cap * sizeof(Series));
  }
  Series *s = &ctx->series[ctx->series_count++];

  s->x = (double *)malloc(count * sizeof(double));
  s->y = (double *)malloc(count * sizeof(double));
  memcpy(s->x, x, count * sizeof(double));
  memcpy(s->y, y, count * sizeof(double));
  s->count = count;
  s->type = type;
  s->thickness = thickness;
  s->color = color;

  for (int i = 0; i < count; i++) {
    if (x[i] < ctx->data_min_x)
      ctx->data_min_x = x[i];
    if (x[i] > ctx->data_max_x)
      ctx->data_max_x = x[i];
    if (y[i] < ctx->data_min_y)
      ctx->data_min_y = y[i];
    if (y[i] > ctx->data_max_y)
      ctx->data_max_y = y[i];
  }

  ctx->view_min_x = ctx->data_min_x;
  ctx->view_max_x = ctx->data_max_x;

  double h = ctx->data_max_y - ctx->data_min_y;
  if (h == 0)
    h = 1.0;
  ctx->view_min_y = ctx->data_min_y - (h * 0.05);
  ctx->view_max_y = ctx->data_max_y + (h * 0.05);
}

static void draw_button(GpGraphics g, GraphBtn *btn, GpFont font,
                        GpBrush textBrush) {
  GpPen pen;
  gp.CreatePen1(0xFF808080, 1.0f, 2, &pen);
  GpBrush bg;
  gp.CreateSolidFill(btn->hover ? 0xFFE0E0E0 : 0xFFF0F0F0, &bg);

  gp.FillRectangle(g, bg, btn->rect.x, btn->rect.y, btn->rect.w, btn->rect.h);
  gp.DrawRectangle(g, pen, btn->rect.x, btn->rect.y, btn->rect.w, btn->rect.h);

  WCHAR wlabel[32];
  MultiByteToWideChar(CP_ACP, 0, btn->label, -1, wlabel, 32);

  GpStringFormat fmt;
  gp.CreateStringFormat(0, 0, &fmt);
  gp.SetStringFormatAlign(fmt, 1);
  gp.SetStringFormatLineAlign(fmt, 1);

  gp.DrawString(g, wlabel, -1, font, &btn->rect, fmt, textBrush);

  gp.DeleteStringFormat(fmt);
  gp.DeleteBrush(bg);
  gp.DeletePen(pen);
}

static void render_to_backbuffer(wplot_ctx *ctx) {
  if (ctx->width <= 0 || ctx->height <= 0)
    return;

  if (!ctx->backbuffer || ctx->bb_width != ctx->width ||
      ctx->bb_height != ctx->height) {
    if (ctx->backbuffer)
      gp.DisposeImage(ctx->backbuffer);
    gp.CreateBitmapFromScan0(ctx->width, ctx->height, 0, 0x26200A, NULL,
                             &ctx->backbuffer);
    ctx->bb_width = ctx->width;
    ctx->bb_height = ctx->height;
  }
  if (!ctx->backbuffer)
    return;

  GpGraphics g;
  gp.GetImageGraphicsContext(ctx->backbuffer, &g);
  if (!g)
    return;

  gp.SetSmoothingMode(g, 2);
  if (gp.SetTextRenderingHint)
    gp.SetTextRenderingHint(g, 5);
  gp.GraphicsClear(g, 0xFFFFFFFF);

  const float toolbar_h = 40.0f;
  const float m_left = 60.0f, m_right = 20.0f, m_bottom = 40.0f;

  float graph_x = m_left;
  float graph_y = toolbar_h + 10;
  float graph_w = ctx->width - m_left - m_right;
  float graph_h = ctx->height - graph_y - m_bottom;

  if (graph_w < 10)
    graph_w = 10;
  if (graph_h < 10)
    graph_h = 10;

  GpPen penBorder;
  gp.CreatePen1(0xFFCCCCCC, 1.0f, 2, &penBorder);
  gp.DrawLine(g, penBorder, 0, toolbar_h, (float)ctx->width, toolbar_h);

  GpFontFamily family;
  gp.CreateFontFamilyFromName(L"Segoe UI", 0, &family);
  GpFont fontTitle, fontBtn, fontAxis;
  gp.CreateFont(family, 14.0f, 1, 2, &fontTitle);
  gp.CreateFont(family, 10.0f, 0, 2, &fontBtn);
  gp.CreateFont(family, 9.0f, 0, 2, &fontAxis);
  GpBrush brushText;
  gp.CreateSolidFill(0xFF000000, &brushText);

  WCHAR wtitle[128];
  MultiByteToWideChar(CP_ACP, 0, ctx->title, -1, wtitle, 128);
  GpRectF rectTitle = {10, 8, (float)ctx->width - 200, 24};
  gp.DrawString(g, wtitle, -1, fontTitle, &rectTitle, 0, brushText);

  ctx->btn_config.rect = (GpRectF){(float)ctx->width - 150, 8, 60, 24};
  ctx->btn_config.label = "Config";
  ctx->btn_range.rect = (GpRectF){(float)ctx->width - 80, 8, 70, 24};
  ctx->btn_range.label = "Set Range";

  draw_button(g, &ctx->btn_config, fontBtn, brushText);
  draw_button(g, &ctx->btn_range, fontBtn, brushText);

  GpPen penGridMajor, penGridMinor;
  gp.CreatePen1(0xFFD0D0D0, 1.0f, 2, &penGridMajor);
  gp.CreatePen1(0xFFEAEAEA, 1.0f, 2, &penGridMinor);

  double view_rx = ctx->view_max_x - ctx->view_min_x;
  double view_ry = ctx->view_max_y - ctx->view_min_y;
  double scale_x = graph_w / view_rx;
  double scale_y = graph_h / view_ry;
  double offset_x = graph_x - ctx->view_min_x * scale_x;
  double offset_y = (graph_y + graph_h) + ctx->view_min_y * scale_y;

  gp.DrawRectangle(g, penBorder, graph_x, graph_y, graph_w, graph_h);

  double x_int =
      oxy_calculate_interval(ctx->view_min_x, ctx->view_max_x, graph_w, 80.0);
  double y_int =
      oxy_calculate_interval(ctx->view_min_y, ctx->view_max_y, graph_h, 50.0);

  double x_sub = x_int / 5.0;
  double y_sub = y_int / 5.0;

  char buf[32];
  WCHAR wbuf[32];
  GpStringFormat centerFmt;
  gp.CreateStringFormat(0, 0, &centerFmt);
  gp.SetStringFormatAlign(centerFmt, 1);

  double x_start = floor(ctx->view_min_x / x_sub) * x_sub;
  for (double v = x_start; v <= ctx->view_max_x; v += x_sub) {
    if (v < ctx->view_min_x)
      continue;
    float x = (float)(v * scale_x + offset_x);

    bool is_major = (fabs(v - round(v / x_int) * x_int) < x_sub * 0.1);
    bool is_integer = (fabs(v - round(v)) < 1e-5);

    GpPen p = is_major ? penGridMajor : penGridMinor;
    if (is_integer && is_major)
      gp.SetPenWidth(p, 1.5f);
    else
      gp.SetPenWidth(p, 1.0f);

    gp.DrawLine(g, p, x, graph_y, x, graph_y + graph_h);

    if (is_major) {
      if (fabs(v) < 1e-9)
        v = 0;
      snprintf(buf, 32, "%.2g", v);
      MultiByteToWideChar(CP_ACP, 0, buf, -1, wbuf, 32);
      GpRectF r = {x - 30, graph_y + graph_h + 5, 60, 20};
      gp.DrawString(g, wbuf, -1, fontAxis, &r, centerFmt, brushText);
    }
  }

  double y_start = floor(ctx->view_min_y / y_sub) * y_sub;
  for (double v = y_start; v <= ctx->view_max_y; v += y_sub) {
    if (v < ctx->view_min_y)
      continue;
    float y = (float)(offset_y - v * scale_y);

    bool is_major = (fabs(v - round(v / y_int) * y_int) < y_sub * 0.1);
    bool is_integer = (fabs(v - round(v)) < 1e-5);

    GpPen p = is_major ? penGridMajor : penGridMinor;
    if (is_integer && is_major)
      gp.SetPenWidth(p, 1.5f);
    else
      gp.SetPenWidth(p, 1.0f);

    gp.DrawLine(g, p, graph_x, y, graph_x + graph_w, y);

    if (is_major) {
      if (fabs(v) < 1e-9)
        v = 0;
      snprintf(buf, 32, "%.2g", v);
      MultiByteToWideChar(CP_ACP, 0, buf, -1, wbuf, 32);
      GpRectF r = {graph_x - 45, y - 7, 40, 20};

      GpStringFormat rightFmt;
      gp.CreateStringFormat(0, 0, &rightFmt);
      gp.SetStringFormatAlign(rightFmt, 2);
      gp.DrawString(g, wbuf, -1, fontAxis, &r, rightFmt, brushText);
      gp.DeleteStringFormat(rightFmt);
    }
  }

  gp.SetClipRect(g, graph_x, graph_y, graph_w, graph_h, 0);

  for (int i = 0; i < ctx->series_count; i++) {
    Series *s = &ctx->series[i];
    GpPen penSeries;
    gp.CreatePen1(s->color, s->thickness, 2, &penSeries);
    GpBrush brushSeries;
    gp.CreateSolidFill(s->color, &brushSeries);

    int step = 1;
    if (s->type == WPLOT_LINE || s->type == WPLOT_SPLINE) {
      double points_per_pixel = (double)s->count / graph_w;
      double view_ratio = view_rx / (ctx->data_max_x - ctx->data_min_x);
      step = (int)(points_per_pixel * view_ratio);
      if (step < 1)
        step = 1;
    }

    if (s->type == WPLOT_SCATTER) {
      float r = s->thickness * 1.5f;
      int bin_size = 6;
      int mx = (int)(graph_w / bin_size) + 1;
      int my = (int)(graph_h / bin_size) + 1;
      unsigned char *bins = calloc(mx * my, 1);

      for (int j = 0; j < s->count; j++) {
        if (s->x[j] < ctx->view_min_x || s->x[j] > ctx->view_max_x)
          continue;
        float fx = (float)(s->x[j] * scale_x + offset_x);
        float fy = (float)(offset_y - s->y[j] * scale_y);
        if (fy < graph_y || fy > graph_y + graph_h)
          continue;

        int bx = (int)((fx - graph_x) / bin_size);
        int by = (int)((fy - graph_y) / bin_size);

        if (bx >= 0 && bx < mx && by >= 0 && by < my) {
          if (!bins[by * mx + bx]) {
            gp.FillEllipse(g, brushSeries, fx - r, fy - r, r * 2, r * 2);
            bins[by * mx + bx] = 1;
          }
        }
      }
      free(bins);
    } else {
      int est = (int)graph_w + 200;
      GpPointF *pts = malloc(est * sizeof(GpPointF));
      int pc = 0;
      for (int j = 0; j < s->count; j += step) {
        if (s->x[j] < ctx->view_min_x - view_rx)
          continue;
        if (s->x[j] > ctx->view_max_x + view_rx)
          break;

        pts[pc].x = (float)(s->x[j] * scale_x + offset_x);
        pts[pc].y = (float)(offset_y - s->y[j] * scale_y);
        pc++;
        if (pc >= est) {
          if (s->type == WPLOT_LINE)
            gp.DrawLines(g, penSeries, pts, pc);
          pts[0] = pts[pc - 1];
          pc = 1;
        }
      }
      if (pc > 1) {
        if (s->type == WPLOT_SPLINE)
          gp.DrawCurve(g, penSeries, pts, pc, 0.5f);
        else
          gp.DrawLines(g, penSeries, pts, pc);
      }
      free(pts);
    }
    gp.DeletePen(penSeries);
    gp.DeleteBrush(brushSeries);
  }

  gp.ResetClip(g);

  gp.DeleteStringFormat(centerFmt);
  gp.DeletePen(penGridMajor);
  gp.DeletePen(penGridMinor);
  gp.DeletePen(penBorder);
  gp.DeleteBrush(brushText);
  gp.DeleteFont(fontTitle);
  gp.DeleteFont(fontBtn);
  gp.DeleteFont(fontAxis);
  gp.DeleteFontFamily(family);
  gp.DeleteGraphics(g);
  ctx->dirty = false;
}

static bool hit_test(GraphBtn *btn, int x, int y) {
  return (x >= btn->rect.x && x <= btn->rect.x + btn->rect.w &&
          y >= btn->rect.y && y <= btn->rect.y + btn->rect.h);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  wplot_ctx *ctx = (wplot_ctx *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  if (msg == WM_NCCREATE) {
    CREATESTRUCT *cs = (CREATESTRUCT *)lp;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
    return DefWindowProc(hwnd, msg, wp, lp);
  }
  
  if (msg == WM_MOUSEMOVE && ctx) {
    int x = LOWORD(lp);
    int y = HIWORD(lp);
    ctx->cursor_pos.x = x;
    ctx->cursor_pos.y = y;

    bool old_hv_c = ctx->btn_config.hover;
    bool old_hv_r = ctx->btn_range.hover;
    ctx->btn_config.hover = hit_test(&ctx->btn_config, x, y);
    ctx->btn_range.hover = hit_test(&ctx->btn_range, x, y);
    if (old_hv_c != ctx->btn_config.hover || old_hv_r != ctx->btn_range.hover) {
      ctx->dirty = true;
      InvalidateRect(hwnd, NULL, FALSE);
    }

    if (ctx->is_dragging) {
      int dx = ctx->last_mouse_x - x;
      int dy = y - ctx->last_mouse_y;
      double rx = ctx->view_max_x - ctx->view_min_x;
      double ry = ctx->view_max_y - ctx->view_min_y;

      float graph_w = (float)ctx->width - 80;
      float graph_h = (float)ctx->height - 90;

      ctx->view_min_x += dx * (rx / graph_w);
      ctx->view_max_x += dx * (rx / graph_w);
      ctx->view_min_y += dy * (ry / graph_h);
      ctx->view_max_y += dy * (ry / graph_h);

      ctx->last_mouse_x = x;
      ctx->last_mouse_y = y;
      ctx->dirty = true;
      InvalidateRect(hwnd, NULL, FALSE);
    }
  }

  if (msg == WM_LBUTTONDOWN && ctx) {
    int x = LOWORD(lp);
    int y = HIWORD(lp);
    if (hit_test(&ctx->btn_config, x, y)) {
      HMENU hMenu = CreatePopupMenu();
      for (int i = 0; i < ctx->series_count; i++) {
        char buf[64];
        snprintf(buf, 64, "Color: Series %d", i + 1);
        AppendMenu(hMenu, MF_STRING, 1000 + i, buf);
      }
      POINT pt;
      GetCursorPos(&pt);
      int id = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x,
                              pt.y, 0, hwnd, NULL);
      DestroyMenu(hMenu);
      if (id >= 1000)
        prompt_color(hwnd, ctx, id - 1000);
      else
        ctx->dirty = true;
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }
    if (hit_test(&ctx->btn_range, x, y)) {
      prompt_range(hwnd, ctx);
      InvalidateRect(hwnd, NULL, FALSE);
      return 0;
    }
  }

  if (msg == WM_MOUSEWHEEL && ctx) {
    int delta = GET_WHEEL_DELTA_WPARAM(wp);
    double zoom = (delta > 0) ? 0.8 : 1.25;

    double range = ctx->view_max_x - ctx->view_min_x;
    double new_range = range * zoom;

    double max_data_range = ctx->data_max_x - ctx->data_min_x;
    if (new_range > max_data_range * 1.1)
      new_range = max_data_range * 1.1;

    double center = (ctx->view_min_x + ctx->view_max_x) / 2.0;

    ctx->view_min_x = center - new_range / 2.0;
    ctx->view_max_x = center + new_range / 2.0;

    ctx->dirty = true;
    InvalidateRect(hwnd, NULL, FALSE);
    return 0;
  }

  if (msg == WM_MBUTTONDOWN && ctx) {
    ctx->is_dragging = true;
    ctx->last_mouse_x = LOWORD(lp);
    ctx->last_mouse_y = HIWORD(lp);
    SetCapture(hwnd);
  }
  if (msg == WM_MBUTTONUP && ctx) {
    ctx->is_dragging = false;
    ReleaseCapture();
  }

  if (msg == WM_PAINT && ctx) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    if (ctx->dirty)
      render_to_backbuffer(ctx);
    if (ctx->backbuffer) {
      GpGraphics g;
      gp.CreateFromHDC(hdc, &g);
      gp.DrawImageI(g, ctx->backbuffer, 0, 0);
      gp.DeleteGraphics(g);
    }
    EndPaint(hwnd, &ps);
    return 0;
  }
  if (msg == WM_SIZE && ctx) {
    if (wp == SIZE_MINIMIZED)
      return 0;
    ctx->width = LOWORD(lp);
    ctx->height = HIWORD(lp);
    if (ctx->width < 1)
      ctx->width = 1;
    if (ctx->height < 1)
      ctx->height = 1;
    ctx->dirty = true;
    InvalidateRect(hwnd, 0, 0);
    return 0;
  }
  if (msg == WM_DESTROY)
    PostQuitMessage(0);
  return DefWindowProc(hwnd, msg, wp, lp);
}

void wplot_show(wplot_ctx *ctx) {
  WNDCLASSA wc = {0};
  wc.lpfnWndProc = wnd_proc;
  wc.hInstance = GetModuleHandle(0);
  wc.lpszClassName = "WPlotClass";
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  RegisterClassA(&wc);
  CreateWindowA("WPlotClass", ctx->title, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                CW_USEDEFAULT, CW_USEDEFAULT, ctx->width, ctx->height, 0, 0,
                wc.hInstance, ctx);
  MSG msg;
  while (GetMessage(&msg, 0, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

void wplot_free(wplot_ctx *ctx) {
  if (ctx->backbuffer)
    gp.DisposeImage(ctx->backbuffer);
  for (int i = 0; i < ctx->series_count; i++) {
    free(ctx->series[i].x);
    free(ctx->series[i].y);
  }
  free(ctx->series);
  free(ctx);
}