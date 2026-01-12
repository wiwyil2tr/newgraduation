/**
 * 插件管理器头文件
 */

#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "plugin_interface.h"

#define MAX_PLUGINS 50
#define MODULE_DIR "./modules"

// 已加载插件结构
typedef struct {
    void *handle;
    PluginInfo info;
    PluginFunctions funcs;
    int initialized;
    char path[256];
} LoadedPlugin;

// 插件管理器函数声明
void list_plugins_internal(LoadedPlugin *plugins, int plugin_count);
void load_plugins_from_directory(LoadedPlugin *plugins, int *plugin_count, const char *dir_path);
void unload_plugins(LoadedPlugin *plugins, int plugin_count);
int execute_command(LoadedPlugin *plugins, int plugin_count, int argc, char **argv);

#endif // PLUGIN_MANAGER_H
