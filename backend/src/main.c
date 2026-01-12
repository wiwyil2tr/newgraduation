/**
 * PenTest ToolKit 后端主程序
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <getopt.h>
#include "framework/plugin_manager.h"

// 函数声明
void show_help(void);
void show_version(void);
int load_config(const char *config_file);

int main(int argc, char **argv) {
    int opt;
    int list_flag = 0;
    int help_flag = 0;
    int version_flag = 0;
    char *config_file = "./config/config.json";

    // 全局插件数组
    static LoadedPlugin plugins[MAX_PLUGINS];
    static int plugin_count = 0;

    // 解析命令行选项
    struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"list", no_argument, 0, 'l'},
        {"config", required_argument, 0, 'c'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "hvlc:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h':
                help_flag = 1;
                break;
            case 'v':
                version_flag = 1;
                break;
            case 'l':
                list_flag = 1;
                break;
            case 'c':
                config_file = optarg;
                break;
            default:
                fprintf(stderr, "未知选项\n");
                return 1;
        }
    }

    // 显示帮助
    if (help_flag) {
        show_help();
        return 0;
    }

    // 显示版本
    if (version_flag) {
        show_version();
        return 0;
    }

    // 加载配置
    if (load_config(config_file) != 0) {
        fprintf(stderr, "警告: 配置文件加载失败，使用默认配置\n");
    }

    printf("PenTest ToolKit 后端 v1.0\n");
    printf("作者: PenTest Team\n");
    printf("===========================\n");

    // 加载插件
    load_plugins_from_directory(plugins, &plugin_count, MODULE_DIR);

    // 列出插件
    if (list_flag) {
        list_plugins_internal(plugins, plugin_count);
        unload_plugins(plugins, plugin_count);
        return 0;
    }

    // 如果没有参数，显示帮助
    if (argc == 1) {
        printf("用法: pentk <模块名> <命令> [参数]\n");
        printf("使用 'pentk --help' 获取更多信息\n");
        printf("使用 'pentk --list' 列出所有可用模块\n\n");
        list_plugins_internal(plugins, plugin_count);
        unload_plugins(plugins, plugin_count);
        return 0;
    }

    // 执行命令
    int result = execute_command(plugins, plugin_count, argc - optind, argv + optind);

    // 清理资源
    unload_plugins(plugins, plugin_count);

    return result;
}

// 显示帮助
void show_help(void) {
    printf("PenTest ToolKit - 自动化渗透测试框架\n");
    printf("用法: pentk [选项] <模块名> <命令> [参数]\n\n");
    printf("选项:\n");
    printf("  -h, --help     显示此帮助信息\n");
    printf("  -v, --version  显示版本信息\n");
    printf("  -l, --list     列出所有可用模块\n");
    printf("  -c, --config   指定配置文件\n\n");
    printf("示例:\n");
    printf("  pentk --list                   列出所有模块\n");
    printf("  pentk port-scanner scan        运行端口扫描\n");
    printf("  pentk --config myconfig.json   使用自定义配置\n");
}

// 显示版本
void show_version(void) {
    printf("PenTest ToolKit v1.0.0\n");
    printf("构建日期: %s %s\n", __DATE__, __TIME__);
}

// 加载配置
int load_config(const char *config_file) {
    FILE *fp = fopen(config_file, "r");
    if (!fp) {
        // 如果文件不存在，创建默认配置
        printf("创建默认配置文件: %s\n", config_file);

        // 创建config目录
        system("mkdir -p config");

        fp = fopen(config_file, "w");
        if (fp) {
            fprintf(fp, "{\n");
            fprintf(fp, "  \"backend\": {\n");
            fprintf(fp, "    \"plugin_directories\": [\"./modules\"]\n");
            fprintf(fp, "  }\n");
            fprintf(fp, "}\n");
            fclose(fp);
            return 0;
        }
        return -1;
    }

    fclose(fp);
    printf("加载配置文件: %s\n", config_file);
    return 0;
}
