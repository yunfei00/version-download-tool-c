#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <windows.h>

#define MAX_RELEASE_ASSETS 256

typedef struct {
    char version[128];
    char file_name[260];
    char size_text[64];
    char published_at[64];
    char download_url[1024];
    DWORD size_bytes;
    char status[64];
} ReleaseAsset;

typedef void (*DownloadProgressCallback)(int progress, DWORD total, DWORD expected, void* user_data);
typedef BOOL (*DownloadCancelCallback)(void* user_data);

BOOL github_fetch_releases(const char* repo, ReleaseAsset* assets, int max_assets, int* out_count, char* error_buf, int error_buf_size);
BOOL github_download_file(const char* url, const char* file_path, DWORD expected_size, DownloadProgressCallback progress_cb, DownloadCancelCallback cancel_cb, void* user_data, BOOL* was_cancelled, char* error_buf, int error_buf_size);

#endif
