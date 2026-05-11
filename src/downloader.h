#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <windows.h>

#define MAX_DOWNLOAD_ITEMS 1000

typedef struct {
    char url[2048];
    char file_name[260];
    char save_path[MAX_PATH];
    unsigned long long size_bytes;
    unsigned long long downloaded_bytes;
    int progress;
    char status[64];
} DownloadItem;

typedef void (*HttpDownloadProgressCallback)(unsigned long long downloaded, unsigned long long total, void* user_data);
typedef BOOL (*HttpDownloadCancelCallback)(void* user_data);

BOOL http_get_text(const char* url, char* buffer, int buffer_size);
unsigned long long http_get_file_size(const char* url);
BOOL http_download_file(const char* url, const char* save_path, HttpDownloadProgressCallback callback, HttpDownloadCancelCallback cancel_cb, void* user_data);
int http_scan_directory(const char* url, DownloadItem* items, int max_items);
void parse_file_name_from_url(const char* url, char* out_name, int out_size);

#endif
