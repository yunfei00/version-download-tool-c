#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <windows.h>

typedef struct DownloadTask {
    HWND hTargetWnd;
    int rowIndex;
} DownloadTask;

BOOL downloader_start_mock(HWND hTargetWnd, int rowIndex);

#endif
