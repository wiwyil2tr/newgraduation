/**
 * 工具函数
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "framework/plugin_interface.h"

// 执行系统命令并返回结果
CommandResult* execute_system_command(const char *command) {
    CommandResult *result = malloc(sizeof(CommandResult));
    if (!result) {
        return NULL;
    }

    result->output = NULL;
    result->output_size = 0;
    result->exit_code = -1;
    result->data = NULL;

    // 创建管道
    int stdout_pipe[2];
    if (pipe(stdout_pipe) == -1) {
        free(result);
        return NULL;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        free(result);
        return NULL;
    }

    if (pid == 0) {
        // 子进程
        close(stdout_pipe[0]);  // 关闭读端

        // 重定向标准输出和错误输出到管道
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stdout_pipe[1], STDERR_FILENO);
        close(stdout_pipe[1]);

        // 执行命令
        execl("/bin/sh", "sh", "-c", command, (char *)NULL);
        exit(EXIT_FAILURE);
    } else {
        // 父进程
        close(stdout_pipe[1]);  // 关闭写端

        // 读取命令输出
        char buffer[4096];
        ssize_t bytes_read;
        size_t total_size = 0;
        char *output = NULL;

        while ((bytes_read = read(stdout_pipe[0], buffer, sizeof(buffer))) > 0) {
            char *new_output = realloc(output, total_size + bytes_read + 1);
            if (!new_output) {
                free(output);
                close(stdout_pipe[0]);
                waitpid(pid, NULL, 0);
                free(result);
                return NULL;
            }

            output = new_output;
            memcpy(output + total_size, buffer, bytes_read);
            total_size += bytes_read;
        }

        if (output) {
            output[total_size] = '\0';
        }

        close(stdout_pipe[0]);

        // 等待子进程结束
        int status;
        waitpid(pid, &status, 0);

        result->output = output;
        result->output_size = total_size;
        result->exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

        return result;
    }
}

// 读取文件内容
char* read_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *content = malloc(size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }

    fread(content, 1, size, fp);
    content[size] = '\0';
    fclose(fp);

    return content;
}

// 写入文件
int write_file(const char *filename, const char *content) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        return -1;
    }

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, fp);
    fclose(fp);

    return (written == len) ? 0 : -1;
}
