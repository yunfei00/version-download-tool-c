#ifndef APP_H
#define APP_H
#include <windows.h>
#include <commctrl.h>
#include "downloader.h"
#include "config.h"

#define APP_WIDTH 1080
#define APP_HEIGHT 680
#define WM_APP_DOWNLOAD_PROGRESS (WM_APP + 1)
#define WM_APP_DOWNLOAD_COMPLETE (WM_APP + 2)
#define WM_APP_SCAN_COMPLETE (WM_APP + 3)

typedef enum ControlId {
    IDC_EDIT_URL = 1001,
    IDC_BTN_ADD_URL,
    IDC_BTN_SCAN_PATH,
    IDC_BTN_DOWNLOAD_SELECTED,
    IDC_BTN_DOWNLOAD_ALL,
    IDC_BTN_SELECT_FOLDER,
    IDC_EDIT_FOLDER,
    IDC_LIST_ITEMS,
    IDC_PROGRESS_TOTAL,
    IDC_EDIT_LOG
} ControlId;

typedef struct AppContext {
    HINSTANCE hInstance; HWND hMainWnd;
    HWND hUrlEdit,hAddBtn,hScanBtn,hDownloadSelectedBtn,hDownloadAllBtn,hSelectFolderBtn,hFolderEdit,hListView,hProgress,hLogEdit;
    DownloadItem items[MAX_DOWNLOAD_ITEMS];
    int item_count;
    volatile LONG downloading;
    volatile LONG cancel_download;
    char download_dir[MAX_PATH];
    AppConfig config;
} AppContext;

BOOL app_init(HINSTANCE hInstance, int nCmdShow);

#endif
