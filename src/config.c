#include "config.h"

#include <stdio.h>
#include <string.h>

static void trim(char* s) {
    char* p = s;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) s[--n] = '\0';
}

void config_set_defaults(AppConfig* cfg) {
    memset(cfg, 0, sizeof(*cfg));
    strncpy(cfg->repo, "yunfei00/version-download-tool-c", sizeof(cfg->repo) - 1);
    strncpy(cfg->download_dir, "downloads", sizeof(cfg->download_dir) - 1);
    cfg->window_width = 900;
    cfg->window_height = 560;
}

void config_add_recent_repo(AppConfig* cfg, const char* repo) {
    if (!repo || !*repo) return;
    for (int i = 0; i < cfg->recent_repo_count; ++i) {
        if (_stricmp(cfg->recent_repos[i], repo) == 0) {
            for (int j = i; j > 0; --j) strcpy(cfg->recent_repos[j], cfg->recent_repos[j - 1]);
            strncpy(cfg->recent_repos[0], repo, sizeof(cfg->recent_repos[0]) - 1);
            return;
        }
    }
    if (cfg->recent_repo_count < MAX_RECENT_REPOS) cfg->recent_repo_count++;
    for (int i = cfg->recent_repo_count - 1; i > 0; --i) strcpy(cfg->recent_repos[i], cfg->recent_repos[i - 1]);
    strncpy(cfg->recent_repos[0], repo, sizeof(cfg->recent_repos[0]) - 1);
}

BOOL config_load(const char* path, AppConfig* cfg) {
    config_set_defaults(cfg);
    FILE* fp = fopen(path, "r");
    if (!fp) return FALSE;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char* k = line;
        char* v = eq + 1;
        trim(k); trim(v);
        if (_stricmp(k, "repo") == 0) strncpy(cfg->repo, v, sizeof(cfg->repo) - 1);
        else if (_stricmp(k, "download_dir") == 0) strncpy(cfg->download_dir, v, sizeof(cfg->download_dir) - 1);
        else if (_stricmp(k, "window_width") == 0) cfg->window_width = atoi(v);
        else if (_stricmp(k, "window_height") == 0) cfg->window_height = atoi(v);
        else if (strncmp(k, "recent_repo_", 12) == 0) {
            int idx = atoi(k + 12);
            if (idx >= 1 && idx <= MAX_RECENT_REPOS && *v) {
                strncpy(cfg->recent_repos[idx - 1], v, sizeof(cfg->recent_repos[idx - 1]) - 1);
                if (cfg->recent_repo_count < idx) cfg->recent_repo_count = idx;
            }
        }
    }
    fclose(fp);
    if (cfg->window_width <= 200) cfg->window_width = 900;
    if (cfg->window_height <= 200) cfg->window_height = 560;
    return TRUE;
}

BOOL config_save(const char* path, const AppConfig* cfg) {
    FILE* fp = fopen(path, "w");
    if (!fp) return FALSE;
    fprintf(fp, "repo=%s\n", cfg->repo);
    fprintf(fp, "download_dir=%s\n", cfg->download_dir);
    fprintf(fp, "window_width=%d\n", cfg->window_width);
    fprintf(fp, "window_height=%d\n", cfg->window_height);
    for (int i = 0; i < cfg->recent_repo_count && i < MAX_RECENT_REPOS; ++i) {
        if (cfg->recent_repos[i][0]) fprintf(fp, "recent_repo_%d=%s\n", i + 1, cfg->recent_repos[i]);
    }
    fclose(fp);
    return TRUE;
}
