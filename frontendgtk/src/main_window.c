/**
 * 主窗口实现
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "main_window.h"
#include "module_panel.h"
#include "port_scanner_ui.h"

// 全局变量
GtkWidget *main_window = NULL;
GtkWidget *notebook = NULL;
GtkWidget *status_bar = NULL;
GtkWidget *menu_bar = NULL;
GtkListStore *module_store = NULL;
GtkWidget *module_tree = NULL;

// 后端路径
char *backend_path = "./backend/pentk";

// 创建主窗口
GtkWidget* create_main_window(GtkApplication *app) {
    // 创建主窗口
    main_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(main_window), "PenTest ToolKit");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 1200, 800);
    gtk_window_set_position(GTK_WINDOW(main_window), GTK_WIN_POS_CENTER);
    
    // 创建主垂直容器
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(main_window), vbox);
    
    // 创建菜单栏
    create_menu_bar(vbox);
    
    // 创建工具栏
    create_toolbar(vbox);
    
    // 创建水平分割面板
    GtkWidget *hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);
    
    // 创建模块列表面板
    create_module_panel(hpaned);
    
    // 创建主选项卡区域
    create_main_notebook(hpaned);
    
    // 创建状态栏
    create_status_bar(vbox);
    
    // 加载模块列表
    load_module_list();
    
    return main_window;
}

// 创建菜单栏
void create_menu_bar(GtkWidget *parent) {
    menu_bar = gtk_menu_bar_new();
    
    // 文件菜单
    GtkWidget *file_menu_item = gtk_menu_item_new_with_label("文件");
    GtkWidget *file_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_menu_item), file_menu);
    
    // 新建选项卡
    GtkWidget *new_tab_item = gtk_menu_item_new_with_label("新建选项卡");
    g_signal_connect(new_tab_item, "activate", 
                    G_CALLBACK(on_new_tab), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), new_tab_item);
    
    // 分隔符
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), gtk_separator_menu_item_new());
    
    // 退出
    GtkWidget *quit_item = gtk_menu_item_new_with_label("退出");
    g_signal_connect(quit_item, "activate", 
                    G_CALLBACK(gtk_main_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), quit_item);
    
    // 工具菜单
    GtkWidget *tools_menu_item = gtk_menu_item_new_with_label("工具");
    GtkWidget *tools_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(tools_menu_item), tools_menu);
    
    // 端口扫描器
    GtkWidget *port_scanner_item = gtk_menu_item_new_with_label("端口扫描器");
    g_signal_connect(port_scanner_item, "activate",
                    G_CALLBACK(on_open_port_scanner), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(tools_menu), port_scanner_item);
    
    // 帮助菜单
    GtkWidget *help_menu_item = gtk_menu_item_new_with_label("帮助");
    GtkWidget *help_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_menu_item), help_menu);
    
    // 关于
    GtkWidget *about_item = gtk_menu_item_new_with_label("关于");
    g_signal_connect(about_item, "activate",
                    G_CALLBACK(on_about), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), about_item);
    
    // 添加到菜单栏
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), file_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), tools_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), help_menu_item);
    
    // 添加到主容器
    gtk_box_pack_start(GTK_BOX(parent), menu_bar, FALSE, FALSE, 0);
}

// 创建工具栏
void create_toolbar(GtkWidget *parent) {
    GtkWidget *toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    
    // 新建选项卡按钮
    GtkToolItem *new_tab_item = gtk_tool_button_new(
        gtk_image_new_from_icon_name("tab-new", GTK_ICON_SIZE_LARGE_TOOLBAR),
        "新建选项卡");
    g_signal_connect(new_tab_item, "clicked",
                    G_CALLBACK(on_new_tab), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), new_tab_item, -1);
    
    // 分隔符
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);
    
    // 运行按钮
    GtkToolItem *run_item = gtk_tool_button_new(
        gtk_image_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_LARGE_TOOLBAR),
        "运行");
    g_signal_connect(run_item, "clicked",
                    G_CALLBACK(on_run_command), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), run_item, -1);
    
    // 停止按钮
    GtkToolItem *stop_item = gtk_tool_button_new(
        gtk_image_new_from_icon_name("media-playback-stop", GTK_ICON_SIZE_LARGE_TOOLBAR),
        "停止");
    g_signal_connect(stop_item, "clicked",
                    G_CALLBACK(on_stop_command), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), stop_item, -1);
    
    // 分隔符
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);
    
    // 清除按钮
    GtkToolItem *clear_item = gtk_tool_button_new(
        gtk_image_new_from_icon_name("edit-clear", GTK_ICON_SIZE_LARGE_TOOLBAR),
        "清除输出");
    g_signal_connect(clear_item, "clicked",
                    G_CALLBACK(on_clear_output), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), clear_item, -1);
    
    // 添加到主容器
    gtk_box_pack_start(GTK_BOX(parent), toolbar, FALSE, FALSE, 0);
}

// 创建主选项卡区域
void create_main_notebook(GtkWidget *parent) {
    GtkWidget *frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
    
    // 创建笔记本（选项卡容器）
    notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    gtk_notebook_popup_enable(GTK_NOTEBOOK(notebook));
    
    // 设置选项卡位置
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
    
    // 连接信号
    g_signal_connect(notebook, "switch-page",
                    G_CALLBACK(on_tab_switched), NULL);
    
    gtk_container_add(GTK_CONTAINER(frame), notebook);
    gtk_paned_add2(GTK_PANED(parent), frame);
    
    // 创建初始的欢迎选项卡
    create_welcome_tab();
}

// 创建欢迎选项卡
void create_welcome_tab(void) {
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
    
    GtkWidget *text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    
    // 设置文本
    const char *welcome_text = 
        "=====================================================\n"
        "            欢迎使用 PenTest ToolKit\n"
        "=====================================================\n\n"
        "这是一个模块化的自动化渗透测试工具框架。\n\n"
        "主要特性:\n"
        "  • 模块化设计，易于扩展\n"
        "  • 前后端分离架构\n"
        "  • 支持多种插件类型\n"
        "  • 图形化用户界面\n"
        "  • 命令行接口\n\n"
        "使用方法:\n"
        "  1. 在左侧模块列表中选择一个模块\n"
        "  2. 双击模块名称打开对应的工具界面\n"
        "  3. 配置参数并执行扫描\n"
        "  4. 查看和分析结果\n\n"
        "当前可用模块:\n"
        "  • port-scanner: 高级端口扫描器\n\n"
        "提示: 使用菜单栏或工具栏按钮执行常用操作。\n"
        "=====================================================\n";
    
    gtk_text_buffer_set_text(buffer, welcome_text, -1);
    
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
    
    // 创建选项卡标签
    GtkWidget *label = gtk_label_new("欢迎");
    
    // 添加到笔记本
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scrolled_window, label);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(notebook), scrolled_window, TRUE);
}

// 创建状态栏
void create_status_bar(GtkWidget *parent) {
    status_bar = gtk_statusbar_new();
    gtk_box_pack_end(GTK_BOX(parent), status_bar, FALSE, FALSE, 0);
    
    // 设置初始状态
    update_status("就绪");
}

// 更新状态栏
void update_status(const char *message) {
    guint context_id = gtk_statusbar_get_context_id(GTK_STATUSBAR(status_bar), "status");
    gtk_statusbar_pop(GTK_STATUSBAR(status_bar), context_id);
    gtk_statusbar_push(GTK_STATUSBAR(status_bar), context_id, message);
}

// 新建选项卡回调
void on_new_tab(GtkMenuItem *item, gpointer data) {
    create_welcome_tab();
}

// 运行命令回调
void on_run_command(GtkToolButton *button, gpointer data) {
    // 获取当前选项卡
    int current_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
    if (current_page < 0) {
        return;
    }
    
    GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), current_page);
    if (!page) {
        return;
    }
    
    // 这里需要根据不同的选项卡类型执行不同的命令
    // 暂时显示消息
    update_status("执行命令...");
}

// 停止命令回调
void on_stop_command(GtkToolButton *button, gpointer data) {
    update_status("命令已停止");
}

// 清除输出回调
void on_clean_output(GtkToolButton *button, gpointer data) {
    int current_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
    if (current_page < 0) {
        return;
    }
    
    GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), current_page);
    if (!page) {
        return;
    }
    
    // 这里需要根据不同的选项卡类型清除输出
    update_status("输出已清除");
}

// 选项卡切换回调
void on_tab_switched(GtkNotebook *notebook, GtkWidget *page, guint page_num, gpointer data) {
    GtkWidget *tab_label = gtk_notebook_get_tab_label(notebook, page);
    if (tab_label) {
        const char *label = gtk_label_get_text(GTK_LABEL(tab_label));
        char status[256];
        snprintf(status, sizeof(status), "当前选项卡: %s", label);
        update_status(status);
    }
}

// 打开端口扫描器回调
void on_open_port_scanner(GtkMenuItem *item, gpointer data) {
    // 查找是否已经打开了端口扫描器选项卡
    int page_count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
    for (int i = 0; i < page_count; i++) {
        GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), i);
        GtkWidget *tab_label = gtk_notebook_get_tab_label(GTK_NOTEBOOK(notebook), page);
        
        if (tab_label) {
            const char *label = gtk_label_get_text(GTK_LABEL(tab_label));
            if (strstr(label, "端口扫描")) {
                // 切换到现有选项卡
                gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), i);
                return;
            }
        }
    }
    
    // 创建新的端口扫描器选项卡
    create_port_scanner_tab();
}

// 关于对话框回调
void on_about(GtkMenuItem *item, gpointer data) {
    GtkWidget *dialog = gtk_about_dialog_new();
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "PenTest ToolKit");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "1.0.0");
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), "© 2024 PenTest Team");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), 
        "一个模块化的自动化渗透测试工具框架");
    gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog), "https://github.com/pentest-toolkit");
    gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(dialog), GTK_LICENSE_GPL_3_0);
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}
