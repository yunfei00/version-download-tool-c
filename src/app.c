#include "app.h"
#include "downloader.h"

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <winhttp.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winhttp.lib")

static const char* MOCK_DATA[][5] = {
    {"v0.1.0", "version-download-tool-v0.1.0.zip", "3.2 MB", "2026-05-01 10:22", "Ready"},
    {"v0.0.9", "version-download-tool-v0.0.9.zip", "2.9 MB", "2026-04-20 08:10", "Ready"},
    {"v0.0.8", "version-download-tool-v0.0.8.zip", "2.8 MB", "2026-04-02 18:36", "Ready"}
};

static AppContext g_app;

static void app_create_controls(AppContext* ctx);
static void app_layout_controls(AppContext* ctx, int width, int height);
static void app_init_list_columns(AppContext* ctx);
static LRESULT CALLBACK app_wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

BOOL app_init(HINSTANCE hInstance, int nCmdShow) {
    INITCOMMONCONTROLSEX icex = {0};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    WNDCLASS wc = {0};
    wc.lpfnWndProc = app_wnd_proc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "VersionDownloadToolWnd";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClass(&wc)) {
        return FALSE;
    }

    ZeroMemory(&g_app, sizeof(g_app));
    g_app.hInstance = hInstance;

    HWND hWnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        APP_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        APP_WIDTH, APP_HEIGHT,
        NULL, NULL, hInstance, &g_app);

    if (!hWnd) {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

void app_log(AppContext* ctx, const char* fmt, ...) {
    if (!ctx || !ctx->hLogEdit) return;

    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    SYSTEMTIME st;
    GetLocalTime(&st);

    char line[1200];
    snprintf(line, sizeof(line), "[%02d:%02d:%02d] %s\r\n", st.wHour, st.wMinute, st.wSecond, message);

    int len = GetWindowTextLength(ctx->hLogEdit);
    SendMessage(ctx->hLogEdit, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessage(ctx->hLogEdit, EM_REPLACESEL, 0, (LPARAM)line);
}

void app_clear_releases(AppContext* ctx) {
    ListView_DeleteAllItems(ctx->hListView);
}

void app_add_release_row(AppContext* ctx, int no, const char* version, const char* fileName,
                         const char* size, const char* publishedAt, const char* status) {
    LVITEM item = {0};
    char noStr[16];
    snprintf(noStr, sizeof(noStr), "%d", no);

    item.mask = LVIF_TEXT;
    item.iItem = ListView_GetItemCount(ctx->hListView);
    item.pszText = noStr;
    int row = ListView_InsertItem(ctx->hListView, &item);

    ListView_SetItemText(ctx->hListView, row, 1, (LPSTR)version);
    ListView_SetItemText(ctx->hListView, row, 2, (LPSTR)fileName);
    ListView_SetItemText(ctx->hListView, row, 3, (LPSTR)size);
    ListView_SetItemText(ctx->hListView, row, 4, (LPSTR)publishedAt);
    ListView_SetItemText(ctx->hListView, row, 5, (LPSTR)status);
}

void app_load_mock_releases(AppContext* ctx) {
    app_clear_releases(ctx);
    for (int i = 0; i < 3; ++i) {
        app_add_release_row(ctx, i + 1, MOCK_DATA[i][0], MOCK_DATA[i][1], MOCK_DATA[i][2], MOCK_DATA[i][3], MOCK_DATA[i][4]);
    }
    app_log(ctx, "Loaded %d mock releases.", 3);
}

void app_set_selected_status(AppContext* ctx, const char* status) {
    int sel = ListView_GetNextItem(ctx->hListView, -1, LVNI_SELECTED);
    if (sel >= 0) {
        ListView_SetItemText(ctx->hListView, sel, 5, (LPSTR)status);
    }
}

static void app_create_controls(AppContext* ctx) {
    HWND hWnd = ctx->hMainWnd;
    ctx->hRepoEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "yunfei00/version-download-tool-c",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0,
        hWnd, (HMENU)IDC_EDIT_REPO, ctx->hInstance, NULL);

    ctx->hRefreshBtn = CreateWindow("BUTTON", "Refresh Releases",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_REFRESH, ctx->hInstance, NULL);

    ctx->hDownloadBtn = CreateWindow("BUTTON", "Download Selected",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 0, 0, hWnd, (HMENU)IDC_BTN_DOWNLOAD, ctx->hInstance, NULL);

    ctx->hListView = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        0, 0, 0, 0, hWnd, (HMENU)IDC_LIST_RELEASES, ctx->hInstance, NULL);

    ListView_SetExtendedListViewStyle(ctx->hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    ctx->hProgress = CreateWindowEx(0, PROGRESS_CLASS, "",
        WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hWnd, (HMENU)IDC_PROGRESS_DOWNLOAD, ctx->hInstance, NULL);

    ctx->hLogEdit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
        0, 0, 0, 0, hWnd, (HMENU)IDC_EDIT_LOG, ctx->hInstance, NULL);

    app_init_list_columns(ctx);
}

static void app_init_list_columns(AppContext* ctx) {
    LVCOLUMN col = {0};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    const char* titles[] = {"No", "Version", "File Name", "Size", "Published At", "Status"};
    int widths[] = {55, 100, 300, 90, 180, 120};

    for (int i = 0; i < 6; ++i) {
        col.iSubItem = i;
        col.pszText = (LPSTR)titles[i];
        col.cx = widths[i];
        ListView_InsertColumn(ctx->hListView, i, &col);
    }
}

static void app_layout_controls(AppContext* ctx, int width, int height) {
    const int margin = 12;
    const int topH = 30;
    const int btnW = 150;
    const int gap = 8;
    const int progressH = 22;
    const int logH = 145;

    int repoW = width - margin * 2 - btnW * 2 - gap * 2 - 8;
    MoveWindow(ctx->hRepoEdit, margin, margin, repoW, topH, TRUE);
    MoveWindow(ctx->hRefreshBtn, margin + repoW + gap, margin, btnW, topH, TRUE);
    MoveWindow(ctx->hDownloadBtn, margin + repoW + gap + btnW + gap, margin, btnW, topH, TRUE);

    int listTop = margin + topH + 10;
    int listH = height - listTop - margin - progressH - 8 - logH - 8;
    MoveWindow(ctx->hListView, margin, listTop, width - margin * 2, listH, TRUE);

    int progressTop = listTop + listH + 8;
    MoveWindow(ctx->hProgress, margin, progressTop, width - margin * 2, progressH, TRUE);

    MoveWindow(ctx->hLogEdit, margin, progressTop + progressH + 8, width - margin * 2, logH, TRUE);
}

static LRESULT CALLBACK app_wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AppContext* ctx = (AppContext*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
        ctx = (AppContext*)cs->lpCreateParams;
        ctx->hMainWnd = hWnd;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)ctx);

        app_create_controls(ctx);
        SendMessage(ctx->hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessage(ctx->hProgress, PBM_SETPOS, 0, 0);
        app_load_mock_releases(ctx);
        app_log(ctx, "Application initialized.");
        return 0;
    }
    case WM_SIZE:
        if (ctx) {
            app_layout_controls(ctx, LOWORD(lParam), HIWORD(lParam));
        }
        return 0;

    case WM_COMMAND:
        if (!ctx) return 0;
        switch (LOWORD(wParam)) {
        case IDC_BTN_REFRESH:
            app_log(ctx, "Refreshing releases (mock)...");
            app_load_mock_releases(ctx);
            break;
        case IDC_BTN_DOWNLOAD: {
            if (ctx->downloading) {
                app_log(ctx, "Download already in progress.");
                break;
            }
            int sel = ListView_GetNextItem(ctx->hListView, -1, LVNI_SELECTED);
            if (sel < 0) {
                app_log(ctx, "Please select a release row first.");
                break;
            }
            ctx->downloading = TRUE;
            SendMessage(ctx->hProgress, PBM_SETPOS, 0, 0);
            app_set_selected_status(ctx, "Downloading");
            app_log(ctx, "Start mock download for row #%d.", sel + 1);
            if (!downloader_start_mock(ctx->hMainWnd, sel)) {
                ctx->downloading = FALSE;
                app_set_selected_status(ctx, "Failed");
                app_log(ctx, "Failed to create download worker thread.");
            }
            break;
        }
        }
        return 0;

    case WM_APP_DOWNLOAD_PROGRESS:
        if (ctx) {
            int progress = (int)wParam;
            SendMessage(ctx->hProgress, PBM_SETPOS, (WPARAM)progress, 0);
            if (progress % 20 == 0) {
                app_log(ctx, "Downloading... %d%%", progress);
            }
        }
        return 0;

    case WM_APP_DOWNLOAD_COMPLETE:
        if (ctx) {
            ctx->downloading = FALSE;
            SendMessage(ctx->hProgress, PBM_SETPOS, 100, 0);
            app_set_selected_status(ctx, "Completed");
            app_log(ctx, "Download completed successfully.");
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}
