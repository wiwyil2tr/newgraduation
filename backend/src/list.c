/**
 * 内置列表命令
 */

#include <stdio.h>
#include <string.h>
#include "framework/plugin_manager.h"

void list_plugins_internal(LoadedPlugin *plugins, int plugin_count) {
    if (plugin_count == 0) {
        printf("没有加载任何插件\n");
        return;
    }

    printf("\n已加载的插件 (%d):\n", plugin_count);
    printf("==============================================================================\n");
    printf("%-20s %-10s %-15s %-30s %s\n",
           "名称", "版本", "分类", "作者", "描述");
    printf("%-20s %-10s %-15s %-30s %s\n",
           "----", "----", "----", "----", "----");

    for (int i = 0; i < plugin_count; i++) {
        printf("%-20s %-10s %-15s %-30s %s\n",
               plugins[i].info.name,
               plugins[i].info.version,
               plugins[i].info.category,
               plugins[i].info.author,
               plugins[i].info.description);
    }
}
