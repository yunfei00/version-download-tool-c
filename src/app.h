#ifndef APP_H
#define APP_H

#include <windows.h>
#include <commctrl.h>
#include "downloader.h"

#define APP_TITLE "Version Download Tool - C Edition"
#define APP_WIDTH 900
#define APP_HEIGHT 560

#define WM_APP_DOWNLOAD_PROGRESS (WM_APP + 1)
#define WM_APP_DOWNLOAD_COMPLETE (WM_APP + 2)
#define WM_APP_REFRESH_COMPLETE (WM_APP + 3)

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
    BOOL refreshing;
    ReleaseAsset assets[MAX_RELEASE_ASSETS];
    int asset_count;
} AppContext;

BOOL app_init(HINSTANCE hInstance, int nCmdShow);
void app_log(AppContext* ctx, const char* fmt, ...);
void app_clear_releases(AppContext* ctx);
void app_add_release_row(AppContext* ctx, int no, const ReleaseAsset* asset);

#endif
