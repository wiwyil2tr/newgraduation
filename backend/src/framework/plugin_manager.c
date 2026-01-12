/**
 * 插件管理器实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "framework/plugin_manager.h"

// 从目录加载插件
void load_plugins_from_directory(LoadedPlugin *plugins, int *plugin_count, const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "警告: 无法打开模块目录: %s\n", dir_path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // 跳过特殊目录
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char plugin_path[512];
        snprintf(plugin_path, sizeof(plugin_path), "%s/%s", dir_path, entry->d_name);

        // 检查是否是.so文件
        if (strstr(entry->d_name, ".so")) {
            void *handle = dlopen(plugin_path, RTLD_LAZY);
            if (!handle) {
                fprintf(stderr, "警告: 无法加载插件 %s: %s\n", plugin_path, dlerror());
                continue;
            }

            // 获取插件信息函数
            void (*get_info)(PluginInfo*) = dlsym(handle, "get_plugin_info");
            void (*get_funcs)(PluginFunctions*) = dlsym(handle, "get_plugin_functions");

            if (!get_info || !get_funcs) {
                fprintf(stderr, "警告: 插件 %s 缺少必要的导出函数\n", plugin_path);
                dlclose(handle);
                continue;
            }

            // 检查插件数量限制
            if (*plugin_count >= MAX_PLUGINS) {
                fprintf(stderr, "错误: 达到插件数量上限 %d\n", MAX_PLUGINS);
                dlclose(handle);
                break;
            }

            // 存储插件信息
            LoadedPlugin *plugin = &plugins[*plugin_count];
            plugin->handle = handle;
            get_info(&plugin->info);
            get_funcs(&plugin->funcs);
            plugin->initialized = 0;
            strncpy(plugin->path, plugin_path, sizeof(plugin->path) - 1);

            printf("加载插件: %s v%s [%s]\n",
                   plugin->info.name,
                   plugin->info.version,
                   plugin->info.category);

            (*plugin_count)++;
        }
    }

    closedir(dir);
}

// 卸载所有插件
void unload_plugins(LoadedPlugin *plugins, int plugin_count) {
    for (int i = 0; i < plugin_count; i++) {
        if (plugins[i].initialized && plugins[i].funcs.cleanup) {
            plugins[i].funcs.cleanup();
        }
        if (plugins[i].handle) {
            dlclose(plugins[i].handle);
        }
    }
}

// 执行命令
int execute_command(LoadedPlugin *plugins, int plugin_count, int argc, char **argv) {
    if (argc < 1) {
        fprintf(stderr, "错误: 需要指定模块名\n");
        return 1;
    }

    char *module_name = argv[0];

    // 查找模块
    for (int i = 0; i < plugin_count; i++) {
        if (strcmp(plugins[i].info.name, module_name) == 0) {
            // 初始化插件
            if (!plugins[i].initialized && plugins[i].funcs.init) {
                int result = plugins[i].funcs.init();
                if (result != 0) {
                    fprintf(stderr, "错误: 插件 %s 初始化失败\n", module_name);
                    return 1;
                }
                plugins[i].initialized = 1;
            }

            // 执行命令
            if (plugins[i].funcs.execute) {
                return plugins[i].funcs.execute(argc, argv);
            } else {
                fprintf(stderr, "错误: 插件 %s 没有实现 execute 函数\n", module_name);
                return 1;
            }
        }
    }

    fprintf(stderr, "错误: 未找到模块 '%s'\n", module_name);
    fprintf(stderr, "使用 'pentk --list' 查看可用模块\n");
    return 1;
}
