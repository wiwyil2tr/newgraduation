#ifndef PLUGIN_INTERFACE_H
#define PLUGIN_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

// 插件信息结构
typedef struct {
    char name[64];
    char version[16];
    char author[64];
    char description[256];
    char category[32];
} PluginInfo;

// 命令执行结果
typedef struct {
    int exit_code;
    char *output;
    size_t output_size;
    void *data;  // 扩展数据
} CommandResult;

// 插件函数表
typedef struct {
    int (*init)(void);
    int (*execute)(int argc, char **argv);
    void (*cleanup)(void);
    CommandResult* (*run_command)(const char *command, const char **args, int arg_count);
    void (*free_result)(CommandResult *result);
    const char* (*get_help)(void);
} PluginFunctions;

// 插件导出函数类型
typedef void (*GetPluginInfoFunc)(PluginInfo *info);
typedef void (*GetPluginFunctionsFunc)(PluginFunctions *funcs);

// 工具函数声明
CommandResult* execute_system_command(const char *command);
char* read_file(const char *filename);
int write_file(const char *filename, const char *content);
char* json_encode(const char *json_str);
char* format_output(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif // PLUGIN_INTERFACE_H
