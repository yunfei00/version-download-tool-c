#include "downloader.h"

#include <stdlib.h>

typedef struct DownloadThreadParam {
    HWND hTargetWnd;
    int rowIndex;
} DownloadThreadParam;

static DWORD WINAPI mock_download_thread(LPVOID lpParam) {
    DownloadThreadParam* param = (DownloadThreadParam*)lpParam;
    if (!param) {
        return 1;
    }

    for (int progress = 0; progress <= 100; ++progress) {
        PostMessage(param->hTargetWnd, WM_APP + 1, (WPARAM)progress, (LPARAM)param->rowIndex);
        Sleep(45);
    }

    PostMessage(param->hTargetWnd, WM_APP + 2, 0, (LPARAM)param->rowIndex);
    free(param);
    return 0;
}

BOOL downloader_start_mock(HWND hTargetWnd, int rowIndex) {
    DownloadThreadParam* param = (DownloadThreadParam*)malloc(sizeof(DownloadThreadParam));
    if (!param) {
        return FALSE;
    }

    param->hTargetWnd = hTargetWnd;
    param->rowIndex = rowIndex;

    HANDLE hThread = CreateThread(NULL, 0, mock_download_thread, param, 0, NULL);
    if (!hThread) {
        free(param);
        return FALSE;
    }

    CloseHandle(hThread);
    return TRUE;
}
