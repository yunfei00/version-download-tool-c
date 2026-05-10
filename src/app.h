#ifndef APP_H
#define APP_H

#include <windows.h>
#include <commctrl.h>
#include "downloader.h"
#include "config.h"
#include "version.h"

#define APP_WIDTH 1080
#define APP_HEIGHT 640

#define WM_APP_DOWNLOAD_PROGRESS (WM_APP + 1)
#define WM_APP_DOWNLOAD_COMPLETE (WM_APP + 2)
#define WM_APP_REFRESH_COMPLETE (WM_APP + 3)
#define WM_APP_SELF_UPDATE_COMPLETE (WM_APP + 4)

typedef enum ControlId {
    IDC_COMBO_REPO = 1001,
    IDC_BTN_REFRESH,
    IDC_BTN_DOWNLOAD,
    IDC_BTN_SELECT_FOLDER,
    IDC_BTN_CANCEL_DOWNLOAD,
    IDC_EDIT_FOLDER,
    IDC_LIST_RELEASES,
    IDC_PROGRESS_DOWNLOAD,
    IDC_EDIT_LOG,
    IDC_BTN_SELF_UPDATE,
    IDC_MENU_ABOUT = 3001
} ControlId;

typedef struct AppContext {
    HINSTANCE hInstance;
    HWND hMainWnd;
    HWND hRepoCombo;
    HWND hRefreshBtn;
    HWND hDownloadBtn;
    HWND hSelectFolderBtn;
    HWND hCancelBtn;
    HWND hSelfUpdateBtn;
    HWND hFolderEdit;
    HWND hListView;
    HWND hProgress;
    HWND hLogEdit;
    volatile LONG downloading;
    volatile LONG refreshing;
    volatile LONG checking_update;
    volatile LONG cancel_download;
    char download_dir[MAX_PATH];
    ReleaseAsset assets[MAX_RELEASE_ASSETS];
    int asset_count;
    AppConfig config;
} AppContext;

BOOL app_init(HINSTANCE hInstance, int nCmdShow);
void app_log(AppContext* ctx, const char* fmt, ...);
void app_clear_releases(AppContext* ctx);
void app_add_release_row(AppContext* ctx, int no, const ReleaseAsset* asset);

#endif
