#ifndef APP_H
#define APP_H

#include <windows.h>
#include <commctrl.h>

#define APP_TITLE "Version Download Tool - C Edition"
#define APP_WIDTH 900
#define APP_HEIGHT 560

#define WM_APP_DOWNLOAD_PROGRESS (WM_APP + 1)
#define WM_APP_DOWNLOAD_COMPLETE (WM_APP + 2)

typedef enum ControlId {
    IDC_EDIT_REPO = 1001,
    IDC_BTN_REFRESH,
    IDC_BTN_DOWNLOAD,
    IDC_LIST_RELEASES,
    IDC_PROGRESS_DOWNLOAD,
    IDC_EDIT_LOG
} ControlId;

typedef struct AppContext {
    HINSTANCE hInstance;
    HWND hMainWnd;
    HWND hRepoEdit;
    HWND hRefreshBtn;
    HWND hDownloadBtn;
    HWND hListView;
    HWND hProgress;
    HWND hLogEdit;
    BOOL downloading;
} AppContext;

BOOL app_init(HINSTANCE hInstance, int nCmdShow);
void app_log(AppContext* ctx, const char* fmt, ...);
void app_clear_releases(AppContext* ctx);
void app_load_mock_releases(AppContext* ctx);
void app_add_release_row(AppContext* ctx, int no, const char* version, const char* fileName,
                         const char* size, const char* publishedAt, const char* status);
void app_set_selected_status(AppContext* ctx, const char* status);

#endif
