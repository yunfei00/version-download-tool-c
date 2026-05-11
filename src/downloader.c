#include "downloader.h"
#include <winhttp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "winhttp.lib")

static BOOL crack_url(const char* url, URL_COMPONENTS* uc, wchar_t* host, DWORD host_len, wchar_t* path, DWORD path_len) {
    wchar_t wurl[2048];
    swprintf(wurl, 2048, L"%S", url);
    ZeroMemory(uc, sizeof(*uc));
    uc->dwStructSize = sizeof(*uc);
    uc->lpszHostName = host; uc->dwHostNameLength = host_len;
    uc->lpszUrlPath = path; uc->dwUrlPathLength = path_len;
    return WinHttpCrackUrl(wurl, 0, 0, uc);
}

void parse_file_name_from_url(const char* url, char* out_name, int out_size) {
    const char* p = strrchr(url, '/');
    p = p ? p + 1 : url;
    if (!*p || strchr(p, '?')) {
        const char* q = p;
        static char tmp[260];
        int i = 0;
        while (q && *q && *q != '?' && i < 259) tmp[i++] = *q++;
        tmp[i] = '\0';
        strncpy(out_name, i ? tmp : "download.bin", out_size - 1);
    } else {
        strncpy(out_name, p, out_size - 1);
    }
    out_name[out_size - 1] = '\0';
    if (!out_name[0]) strncpy(out_name, "download.bin", out_size - 1);
}

BOOL http_get_text(const char* url, char* buffer, int buffer_size) {
    URL_COMPONENTS uc; wchar_t host[256], path[1600];
    if (!crack_url(url, &uc, host, 256, path, 1600)) return FALSE;
    HINTERNET s = WinHttpOpen(L"http-path-downloader", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET c = s ? WinHttpConnect(s, host, uc.nPort, 0) : NULL;
    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags) : NULL;
    if (!r || !WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(r, NULL)) goto fail;
    int total = 0;
    while (1) {
        DWORD avail = 0, rd = 0;
        if (!WinHttpQueryDataAvailable(r, &avail)) goto fail;
        if (!avail) break;
        if (total + (int)avail >= buffer_size - 1) avail = (DWORD)(buffer_size - 1 - total);
        if (!WinHttpReadData(r, buffer + total, avail, &rd)) goto fail;
        total += (int)rd;
        if (total >= buffer_size - 1) break;
    }
    buffer[total] = '\0';
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    return TRUE;
fail:
    if (r) WinHttpCloseHandle(r); if (c) WinHttpCloseHandle(c); if (s) WinHttpCloseHandle(s);
    return FALSE;
}

unsigned long long http_get_file_size(const char* url) {
    URL_COMPONENTS uc; wchar_t host[256], path[1600];
    if (!crack_url(url, &uc, host, 256, path, 1600)) return 0;
    HINTERNET s = WinHttpOpen(L"http-path-downloader", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET c = s ? WinHttpConnect(s, host, uc.nPort, 0) : NULL;
    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET r = c ? WinHttpOpenRequest(c, L"HEAD", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags) : NULL;
    if (!r || !WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(r, NULL)) goto fail;
    wchar_t len_buf[64]; DWORD len = sizeof(len_buf);
    if (WinHttpQueryHeaders(r, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, len_buf, &len, WINHTTP_NO_HEADER_INDEX)) {
        unsigned long long size = _wcstoui64(len_buf, NULL, 10);
        WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s); return size;
    }
fail:
    if (r) WinHttpCloseHandle(r); if (c) WinHttpCloseHandle(c); if (s) WinHttpCloseHandle(s);
    return 0;
}

BOOL http_download_file(const char* url, const char* save_path, HttpDownloadProgressCallback callback, HttpDownloadCancelCallback cancel_cb, void* user_data) {
    URL_COMPONENTS uc; wchar_t host[256], path[1600];
    if (!crack_url(url, &uc, host, 256, path, 1600)) return FALSE;
    unsigned long long total_expected = http_get_file_size(url);
    HINTERNET s = WinHttpOpen(L"http-path-downloader", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET c = s ? WinHttpConnect(s, host, uc.nPort, 0) : NULL;
    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags) : NULL;
    if (!r || !WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(r, NULL)) goto fail;
    FILE* fp = fopen(save_path, "wb"); if (!fp) goto fail;
    unsigned long long total = 0;
    while (1) {
        if (cancel_cb && cancel_cb(user_data)) { fclose(fp); return FALSE; }
        DWORD avail = 0, rd = 0;
        if (!WinHttpQueryDataAvailable(r, &avail)) { fclose(fp); goto fail; }
        if (!avail) break;
        BYTE* buf = (BYTE*)malloc(avail); if (!buf) { fclose(fp); goto fail; }
        if (!WinHttpReadData(r, buf, avail, &rd)) { free(buf); fclose(fp); goto fail; }
        fwrite(buf, 1, rd, fp); free(buf);
        total += rd;
        if (callback) callback(total, total_expected, user_data);
    }
    fclose(fp);
    if (callback) callback(total, total_expected, user_data);
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s); return TRUE;
fail:
    if (r) WinHttpCloseHandle(r); if (c) WinHttpCloseHandle(c); if (s) WinHttpCloseHandle(s);
    return FALSE;
}

int http_scan_directory(const char* url, DownloadItem* items, int max_items) {
    char html[1 << 20];
    if (!http_get_text(url, html, sizeof(html))) return 0;
    int count = 0;
    char* p = html;
    while ((p = strstr(p, "href=\"")) && count < max_items) {
        p += 6;
        char* e = strchr(p, '"'); if (!e) break;
        char href[1024]; int n = (int)(e - p); if (n > 1023) n = 1023; strncpy(href, p, n); href[n] = '\0';
        if (strstr(href, "../") || href[0] == '/' || href[strlen(href)-1]=='/' || href[0]=='?' || strstr(href,"?C=")) { p = e + 1; continue; }
        if (strstr(href, "://")) strncpy(items[count].url, href, sizeof(items[count].url)-1);
        else snprintf(items[count].url, sizeof(items[count].url), "%s%s", url, href);
        parse_file_name_from_url(items[count].url, items[count].file_name, sizeof(items[count].file_name));
        strcpy(items[count].status, "Pending"); items[count].progress = 0; items[count].size_bytes = http_get_file_size(items[count].url);
        count++; p = e + 1;
    }
    return count;
}
