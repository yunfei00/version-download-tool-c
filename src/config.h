#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>

#define MAX_RECENT_REPOS 10

typedef struct AppConfig {
    char repo[256];
    char download_dir[MAX_PATH];
    int window_width;
    int window_height;
    char recent_repos[MAX_RECENT_REPOS][256];
    int recent_repo_count;
} AppConfig;

void config_set_defaults(AppConfig* cfg);
BOOL config_load(const char* path, AppConfig* cfg);
BOOL config_save(const char* path, const AppConfig* cfg);
void config_add_recent_repo(AppConfig* cfg, const char* repo);

#endif
