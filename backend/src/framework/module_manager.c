/**
 * 模块管理器 - 管理所有加载的插件
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "framework/plugin_interface.h"

#define MAX_MODULES 256
#define MODULE_CACHE_DIR "/tmp/pentk_cache"

typedef struct {
    char name[64];
    char path[256];
    time_t last_modified;
    void *handle;
    PluginInfo info;
    PluginFunctions funcs;
    int enabled;
} ModuleEntry;

static ModuleEntry modules[MAX_MODULES];
static int module_count = 0;
static pthread_mutex_t module_mutex = PTHREAD_MUTEX_INITIALIZER;

// 初始化模块系统
int module_system_init() {
    // 创建缓存目录
    mkdir(MODULE_CACHE_DIR, 0755);

    // 扫描模块目录
    scan_module_directories();

    return 0;
}

// 扫描模块目录
void scan_module_directories() {
    const char *dirs[] = {
        "./modules",
        "/usr/local/lib/pentk/modules",
        "/opt/pentk/modules",
        NULL
    };

    pthread_mutex_lock(&module_mutex);

    for (int i = 0; dirs[i] != NULL; i++) {
        scan_directory(dirs[i]);
    }

    pthread_mutex_unlock(&module_mutex);
}

// 加载单个模块
int load_module(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }

    // 检查是否已加载
    for (int i = 0; i < module_count; i++) {
        if (strcmp(modules[i].path, path) == 0) {
            // 检查是否需要重新加载
            if (modules[i].last_modified != st.st_mtime) {
                unload_module(i);
                break;
            }
            return i;
        }
    }

    if (module_count >= MAX_MODULES) {
        fprintf(stderr, "达到模块数量上限\n");
        return -1;
    }

    // 加载动态库
    void *handle = dlopen(path, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "无法加载模块 %s: %s\n", path, dlerror());
        return -1;
    }

    // 获取插件信息
    GetPluginInfoFunc get_info = (GetPluginInfoFunc)dlsym(handle, "get_plugin_info");
    if (!get_info) {
        dlclose(handle);
        return -1;
    }

    ModuleEntry *entry = &modules[module_count];
    memset(entry, 0, sizeof(ModuleEntry));

    get_info(&entry->info);
    strncpy(entry->path, path, sizeof(entry->path) - 1);
    entry->handle = handle;
    entry->last_modified = st.st_mtime;
    entry->enabled = 1;

    // 获取插件函数
    GetPluginFunctionsFunc get_funcs = (GetPluginFunctionsFunc)dlsym(
        handle, "get_plugin_functions");

    if (get_funcs) {
        get_funcs(&entry->funcs);

        // 初始化模块
        if (entry->funcs.init) {
            entry->funcs.init();
        }
    }

    printf("成功加载模块: %s v%s\n", entry->info.name, entry->info.version);

    return module_count++;
}
