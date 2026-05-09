#include "downloader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

static BOOL http_get(const wchar_t* host, INTERNET_PORT port, const wchar_t* path, const wchar_t* headers, BYTE** out_data, DWORD* out_size, DWORD* out_status, char* err, int err_sz) {
    HINTERNET session = NULL, connect = NULL, request = NULL;
    BOOL ok = FALSE;
    BYTE* buffer = NULL;
    DWORD size = 0;

    session = WinHttpOpen(L"version-download-tool-c", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) goto done;
    connect = WinHttpConnect(session, host, port, 0);
    if (!connect) goto done;
    request = WinHttpOpenRequest(connect, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!request) goto done;
    if (!WinHttpSendRequest(request, headers, (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) goto done;
    if (!WinHttpReceiveResponse(request, NULL)) goto done;

    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, out_status, &(DWORD){sizeof(DWORD)}, WINHTTP_NO_HEADER_INDEX);

    while (1) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(request, &avail)) goto done;
        if (avail == 0) break;
        BYTE* newbuf = (BYTE*)realloc(buffer, size + avail + 1);
        if (!newbuf) goto done;
        buffer = newbuf;
        DWORD read = 0;
        if (!WinHttpReadData(request, buffer + size, avail, &read)) goto done;
        size += read;
    }
    if (!buffer) {
        buffer = (BYTE*)malloc(1);
        if (!buffer) goto done;
    }
    buffer[size] = '\0';
    *out_data = buffer;
    *out_size = size;
    ok = TRUE;

done:
    if (!ok) {
        if (err && err_sz > 0) snprintf(err, err_sz, "WinHTTP failed: %lu", GetLastError());
        free(buffer);
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
    return ok;
}

static void json_extract(const char* start, const char* key, char* out, size_t out_sz) {
    out[0] = '\0';
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char* p = strstr(start, pattern);
    if (!p) return;
    p += strlen(pattern);
    const char* e = strchr(p, '"');
    if (!e) return;
    size_t n = (size_t)(e - p);
    if (n >= out_sz) n = out_sz - 1;
    memcpy(out, p, n);
    out[n] = '\0';
}

static DWORD json_extract_number(const char* start, const char* key) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char* p = strstr(start, pattern);
    if (!p) return 0;
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t') p++;
    return (DWORD)strtoul(p, NULL, 10);
}

static void format_size(DWORD bytes, char* out, size_t out_sz) {
    double kb = (double)bytes / 1024.0;
    if (kb >= 1024.0) snprintf(out, out_sz, "%.2f MB", kb / 1024.0);
    else snprintf(out, out_sz, "%.2f KB", kb);
}

BOOL github_fetch_releases(const char* repo, ReleaseAsset* assets, int max_assets, int* out_count, char* error_buf, int error_buf_size) {
    *out_count = 0;
    char owner[128], name[128];
    const char* slash = strchr(repo, '/');
    if (!slash || slash == repo || !*(slash + 1) || strchr(slash + 1, '/')) {
        snprintf(error_buf, error_buf_size, "Invalid repo format. Expected owner/repo.");
        return FALSE;
    }
    size_t owner_len = (size_t)(slash - repo);
    strncpy(owner, repo, owner_len); owner[owner_len] = '\0';
    strncpy(name, slash + 1, sizeof(name) - 1); name[sizeof(name) - 1] = '\0';

    wchar_t path[512];
    swprintf(path, 512, L"/repos/%S/%S/releases", owner, name);
    BYTE* data = NULL; DWORD data_sz = 0; DWORD status = 0;
    if (!http_get(L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, path, L"Accept: application/vnd.github+json\r\nUser-Agent: version-download-tool-c\r\n", &data, &data_sz, &status, error_buf, error_buf_size)) return FALSE;
    if (status != 200) {
        snprintf(error_buf, error_buf_size, "GitHub API HTTP status: %lu", status);
        free(data);
        return FALSE;
    }

    const char* p = (const char*)data;
    while ((p = strstr(p, "\"tag_name\":\"")) && *out_count < max_assets) {
        ReleaseAsset tmp[64]; int tmp_count = 0;
        char tag[128] = {0}, pub[64] = {0};
        json_extract(p, "tag_name", tag, sizeof(tag));
        json_extract(p, "published_at", pub, sizeof(pub));
        const char* release_end = strstr(p + 1, "\"tag_name\":\"");
        const char* a = p;
        while ((a = strstr(a, "\"browser_download_url\":\"")) && (!release_end || a < release_end)) {
            if (tmp_count >= 64) break;
            json_extract(a, "name", tmp[tmp_count].file_name, sizeof(tmp[tmp_count].file_name));
            json_extract(a, "browser_download_url", tmp[tmp_count].download_url, sizeof(tmp[tmp_count].download_url));
            tmp[tmp_count].size_bytes = json_extract_number(a, "size");
            strncpy(tmp[tmp_count].version, tag, sizeof(tmp[tmp_count].version)-1);
            strncpy(tmp[tmp_count].published_at, pub, sizeof(tmp[tmp_count].published_at)-1);
            format_size(tmp[tmp_count].size_bytes, tmp[tmp_count].size_text, sizeof(tmp[tmp_count].size_text));
            strncpy(tmp[tmp_count].status, "Ready", sizeof(tmp[tmp_count].status)-1);
            tmp_count++;
            a += 22;
        }
        for (int i = 0; i < tmp_count && *out_count < max_assets; ++i) assets[(*out_count)++] = tmp[i];
        p += 11;
    }

    free(data);
    if (*out_count == 0) {
        snprintf(error_buf, error_buf_size, "No release assets found.");
        return FALSE;
    }
    return TRUE;
}

BOOL github_download_file(const char* url, const char* file_path, DWORD expected_size, DownloadProgressCallback progress_cb, void* user_data, char* error_buf, int error_buf_size) {
    wchar_t wurl[2048];
    swprintf(wurl, 2048, L"%S", url);
    URL_COMPONENTS uc = {0}; wchar_t host[256], path[1600];
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 1600;
    if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) { snprintf(error_buf, error_buf_size, "Invalid download URL."); return FALSE; }

    HINTERNET s = WinHttpOpen(L"version-download-tool-c", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET c = s ? WinHttpConnect(s, host, uc.nPort, 0) : NULL;
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : NULL;
    if (!r || !WinHttpSendRequest(r, L"User-Agent: version-download-tool-c\r\n", (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(r, NULL)) {
        snprintf(error_buf, error_buf_size, "Download request failed: %lu", GetLastError());
        if (r) WinHttpCloseHandle(r); if (c) WinHttpCloseHandle(c); if (s) WinHttpCloseHandle(s); return FALSE;
    }

    FILE* fp = fopen(file_path, "wb");
    if (!fp) { snprintf(error_buf, error_buf_size, "Failed to open output file."); return FALSE; }
    DWORD total = 0;
    while (1) {
        DWORD avail = 0; if (!WinHttpQueryDataAvailable(r, &avail)) break; if (!avail) break;
        BYTE* buf = (BYTE*)malloc(avail); DWORD rd = 0;
        if (!buf || !WinHttpReadData(r, buf, avail, &rd)) { free(buf); break; }
        fwrite(buf, 1, rd, fp); free(buf); total += rd;
        if (progress_cb && expected_size > 0) progress_cb((int)((total * 100ULL) / expected_size), user_data);
    }
    fclose(fp);
    if (progress_cb) progress_cb(100, user_data);
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    return TRUE;
}
