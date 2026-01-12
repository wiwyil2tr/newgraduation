/**
 * PenTest ToolKit 前端主程序
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main_window.h"

// 全局应用状态
typedef struct {
    GtkApplication *app;
    GtkWidget *main_window;
    int backend_available;
    char *backend_path;
} AppState;

static AppState app_state;

// 启动后端检查
int check_backend(void) {
    char command[256];
    snprintf(command, sizeof(command), "%s --version", 
             app_state.backend_path ? app_state.backend_path : "./backend/pentk");
    
    FILE *fp = popen(command, "r");
    if (!fp) {
        return 0;
    }
    
    char buffer[128];
    if (fgets(buffer, sizeof(buffer), fp)) {
        if (strstr(buffer, "PenTest ToolKit")) {
            app_state.backend_available = 1;
        }
    }
    
    pclose(fp);
    return app_state.backend_available;
}

// 应用程序激活回调
static void app_activate(GtkApplication *app, gpointer user_data) {
    // 检查后端
    if (!check_backend()) {
        GtkWidget *dialog = gtk_message_dialog_new(NULL,
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "后端程序未找到或无法运行\n请确保后端程序已正确编译并位于正确位置");
        gtk_window_set_title(GTK_WINDOW(dialog), "错误");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        
        // 退出应用
        g_application_quit(G_APPLICATION(app));
        return;
    }
    
    // 创建主窗口
    app_state.main_window = create_main_window(app);
    gtk_window_set_application(GTK_WINDOW(app_state.main_window), app);
    gtk_widget_show_all(app_state.main_window);
}

int main(int argc, char **argv) {
    // 初始化应用状态
    memset(&app_state, 0, sizeof(AppState));
    app_state.backend_path = "./backend/pentk";
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
            app_state.backend_path = argv[++i];
        }
    }
    
    // 创建Gtk应用
    app_state.app = gtk_application_new("com.pentest.toolkit", 
                                       G_APPLICATION_FLAGS_NONE);
    
    // 连接信号
    g_signal_connect(app_state.app, "activate", 
                    G_CALLBACK(app_activate), NULL);
    
    // 运行应用
    int status = g_application_run(G_APPLICATION(app_state.app), 
                                  argc, argv);
    
    // 清理
    g_object_unref(app_state.app);
    
    return status;
}
