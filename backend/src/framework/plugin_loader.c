/**
 * 插件加载器
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "framework/plugin_manager.h"

// 扫描目录中的插件
int scan_directory_for_plugins(const char *dir_path, LoadedPlugin *plugins, int *count) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        return 0;
    }

    struct dirent *entry;
    int loaded = 0;

    while ((entry = readdir(dir)) != NULL) {
        // 跳过特殊目录
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }

        // 如果是.so文件
        if (S_ISREG(st.st_mode) && strstr(entry->d_name, ".so")) {
            // 检查是否已加载
            int already_loaded = 0;
            for (int i = 0; i < *count; i++) {
                if (strcmp(plugins[i].path, full_path) == 0) {
                    already_loaded = 1;
                    break;
                }
            }

            if (!already_loaded) {
                void *handle = dlopen(full_path, RTLD_LAZY);
                if (!handle) {
                    fprintf(stderr, "警告: 无法加载插件 %s: %s\n", full_path, dlerror());
                    continue;
                }

                // 获取插件信息函数
                void (*get_info)(PluginInfo*) = dlsym(handle, "get_plugin_info");
                void (*get_funcs)(PluginFunctions*) = dlsym(handle, "get_plugin_functions");

                if (!get_info || !get_funcs) {
                    fprintf(stderr, "警告: 插件 %s 缺少必要的导出函数\n", full_path);
                    dlclose(handle);
                    continue;
                }

                // 检查插件数量限制
                if (*count >= MAX_PLUGINS) {
                    fprintf(stderr, "错误: 达到插件数量上限 %d\n", MAX_PLUGINS);
                    dlclose(handle);
                    break;
                }

                // 存储插件信息
                LoadedPlugin *plugin = &plugins[*count];
                plugin->handle = handle;
                get_info(&plugin->info);
                get_funcs(&plugin->funcs);
                plugin->initialized = 0;
                strncpy(plugin->path, full_path, sizeof(plugin->path) - 1);

                printf("加载插件: %s v%s [%s]\n",
                       plugin->info.name,
                       plugin->info.version,
                       plugin->info.category);

                (*count)++;
                loaded++;
            }
        }
    }

    closedir(dir);
    return loaded;
}
