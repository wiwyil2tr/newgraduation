/**
 * 模块列表面板
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "main_window.h"

// 模块树形视图列定义
enum {
    COLUMN_NAME,
    COLUMN_CATEGORY,
    COLUMN_DESCRIPTION,
    COLUMN_VERSION,
    COLUMN_AUTHOR,
    N_COLUMNS
};

// 创建模块面板
void create_module_panel(GtkWidget *parent) {
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
    
    // 创建树形视图和存储
    module_store = gtk_list_store_new(N_COLUMNS,
                                     G_TYPE_STRING,  // 名称
                                     G_TYPE_STRING,  // 分类
                                     G_TYPE_STRING,  // 描述
                                     G_TYPE_STRING,  // 版本
                                     G_TYPE_STRING); // 作者
    
    module_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(module_store));
    
    // 设置列
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    
    // 名称列
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("模块", renderer,
                                                     "text", COLUMN_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(module_tree), column);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_NAME);
    
    // 分类列
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("分类", renderer,
                                                     "text", COLUMN_CATEGORY, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(module_tree), column);
    gtk_tree_view_column_set_sort_column_id(column, COLUMN_CATEGORY);
    
    // 描述列
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("描述", renderer,
                                                     "text", COLUMN_DESCRIPTION, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(module_tree), column);
    gtk_tree_view_column_set_expand(column, TRUE);
    
    // 版本列
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("版本", renderer,
                                                     "text", COLUMN_VERSION, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(module_tree), column);
    
    // 设置选择模式
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(module_tree));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    
    // 连接双击事件
    g_signal_connect(module_tree, "row-activated",
                    G_CALLBACK(on_module_activated), NULL);
    
    gtk_container_add(GTK_CONTAINER(scrolled_window), module_tree);
    
    // 创建框架
    GtkWidget *frame = gtk_frame_new("模块列表");
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_ETCHED_IN);
    gtk_container_add(GTK_CONTAINER(frame), scrolled_window);
    
    gtk_paned_add1(GTK_PANED(parent), frame);
    gtk_paned_set_position(GTK_PANED(parent), 300);
}

// 加载模块列表
void load_module_list(void) {
    // 执行后端命令获取模块列表
    char command[256];
    snprintf(command, sizeof(command), "%s --list", backend_path);
    
    FILE *fp = popen(command, "r");
    if (!fp) {
        fprintf(stderr, "错误: 无法执行后端命令\n");
        return;
    }
    
    char line[512];
    int in_module_list = 0;
    int in_category = 0;
    char current_category[64] = "";
    
    while (fgets(line, sizeof(line), fp)) {
        // 去除换行符
        line[strcspn(line, "\n")] = '\0';
        
        // 跳过空行和分隔线
        if (strlen(line) == 0 || line[0] == '=' || line[0] == '-') {
            continue;
        }
        
        // 检查是否进入模块列表
        if (strstr(line, "已加载的插件") || strstr(line, "可用模块")) {
            in_module_list = 1;
            continue;
        }
        
        // 检查是否是类别标题
        if (line[0] == '[' && strchr(line, ']')) {
            in_category = 1;
            strncpy(current_category, line, sizeof(current_category) - 1);
            continue;
        }
        
        // 解析模块行
        if (in_module_list && strlen(line) > 0) {
            // 尝试解析模块信息
            // 格式可能是: 名称 版本 分类 作者 描述
            char name[64] = "", version[16] = "", category[32] = "", 
                 author[64] = "", description[256] = "";
            
            // 简化解析，只处理端口扫描器模块
            if (strstr(line, "port-scanner")) {
                strcpy(name, "port-scanner");
                strcpy(version, "1.0.0");
                strcpy(category, "scanner");
                strcpy(author, "PenTest Team");
                strcpy(description, "高级端口扫描器");
                
                add_module_to_list(name, category, description, version, author);
            }
        }
    }
    
    pclose(fp);
    
    // 如果没有加载到模块，添加默认的端口扫描器模块
    if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(module_store), NULL) == 0) {
        add_module_to_list("port-scanner", "scanner", 
                          "高级端口扫描器，支持TCP/UDP扫描", 
                          "1.0.0", "PenTest Team");
    }
}

// 添加模块到列表
void add_module_to_list(const char *name, const char *category, 
                       const char *description, const char *version,
                       const char *author) {
    GtkTreeIter iter;
    gtk_list_store_append(module_store, &iter);
    gtk_list_store_set(module_store, &iter,
                      COLUMN_NAME, name,
                      COLUMN_CATEGORY, category,
                      COLUMN_DESCRIPTION, description,
                      COLUMN_VERSION, version,
                      COLUMN_AUTHOR, author,
                      -1);
}

// 模块双击事件
void on_module_activated(GtkTreeView *tree_view, 
                        GtkTreePath *path,
                        GtkTreeViewColumn *column,
                        gpointer user_data) {
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        char *name, *category;
        gtk_tree_model_get(model, &iter, 
                          COLUMN_NAME, &name,
                          COLUMN_CATEGORY, &category,
                          -1);
        
        if (name) {
            // 根据模块名称打开相应的界面
            if (strcmp(name, "port-scanner") == 0) {
                create_port_scanner_tab();
            } else {
                // 默认创建通用模块选项卡
                create_generic_module_tab(name);
            }
            
            g_free(name);
            g_free(category);
        }
    }
}

// 创建通用模块选项卡
void create_generic_module_tab(const char *module_name) {
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    
    // 标题
    char title[256];
    snprintf(title, sizeof(title), "%s 模块", module_name);
    GtkWidget *label = gtk_label_new(title);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(label), attrs);
    pango_attr_list_unref(attrs);
    
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    
    // 分隔线
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), 
                      FALSE, FALSE, 5);
    
    // 命令输入框
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *cmd_label = gtk_label_new("命令:");
    GtkWidget *cmd_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(cmd_entry), "输入命令...");
    gtk_widget_set_hexpand(cmd_entry, TRUE);
    
    gtk_box_pack_start(GTK_BOX(hbox), cmd_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), cmd_entry, TRUE, TRUE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
    
    // 按钮栏
    GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_START);
    
    GtkWidget *run_button = gtk_button_new_with_label("运行");
    GtkWidget *help_button = gtk_button_new_with_label("帮助");
    
    g_signal_connect(run_button, "clicked",
                    G_CALLBACK(on_generic_run), cmd_entry);
    g_signal_connect(help_button, "clicked",
                    G_CALLBACK(on_generic_help), (gpointer)module_name);
    
    gtk_container_add(GTK_CONTAINER(button_box), run_button);
    gtk_container_add(GTK_CONTAINER(button_box), help_button);
    
    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 5);
    
    // 输出区域
    GtkWidget *output_frame = gtk_frame_new("输出");
    GtkWidget *output_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(output_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(output_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(output_view), GTK_WRAP_WORD_CHAR);
    
    gtk_container_add(GTK_CONTAINER(output_frame), output_view);
    gtk_widget_set_vexpand(output_frame, TRUE);
    
    gtk_box_pack_start(GTK_BOX(vbox), output_frame, TRUE, TRUE, 0);
    
    gtk_container_add(GTK_CONTAINER(scrolled_window), vbox);
    
    // 创建选项卡标签
    char tab_label[64];
    snprintf(tab_label, sizeof(tab_label), "%s", module_name);
    GtkWidget *label_widget = gtk_label_new(tab_label);
    
    // 添加到笔记本
    int page_num = gtk_notebook_append_page(GTK_NOTEBOOK(notebook), 
                                           scrolled_window, 
                                           label_widget);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(notebook), scrolled_window, TRUE);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), page_num);
}

// 通用运行按钮回调
void on_generic_run(GtkButton *button, gpointer data) {
    GtkWidget *entry = GTK_WIDGET(data);
    const char *command = gtk_entry_get_text(GTK_ENTRY(entry));
    
    if (!command || strlen(command) == 0) {
        // 显示错误对话框
        GtkWidget *dialog = gtk_message_dialog_new(NULL,
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "请输入命令");
        gtk_window_set_title(GTK_WINDOW(dialog), "错误");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
    
    // 获取当前选项卡的输出视图
    int current_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
    GtkWidget *page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), current_page);
    
    if (page) {
        // 查找输出视图
        GtkWidget *output_view = find_child_by_type(page, GTK_TYPE_TEXT_VIEW);
        if (output_view) {
            execute_command_and_display(command, output_view);
        }
    }
}

// 通用帮助按钮回调
void on_generic_help(GtkButton *button, gpointer data) {
    const char *module_name = (const char *)data;
    
    char command[256];
    snprintf(command, sizeof(command), "%s help %s", backend_path, module_name);
    
    FILE *fp = popen(command, "r");
    if (!fp) {
        return;
    }
    
    // 创建帮助对话框
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "帮助",
        GTK_WINDOW(main_window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "关闭",
        GTK_RESPONSE_OK,
        NULL);
    
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *text_view = gtk_text_view_new();
    
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(text_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD_CHAR);
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    
    // 读取帮助内容
    char line[1024];
    GString *help_text = g_string_new("");
    
    while (fgets(line, sizeof(line), fp)) {
        g_string_append(help_text, line);
    }
    
    pclose(fp);
    
    gtk_text_buffer_set_text(buffer, help_text->str, -1);
    g_string_free(help_text, TRUE);
    
    gtk_container_add(GTK_CONTAINER(scrolled_window), text_view);
    gtk_container_add(GTK_CONTAINER(content_area), scrolled_window);
    
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// 执行命令并显示输出
void execute_command_and_display(const char *command, GtkWidget *text_view) {
    if (!command || !text_view) {
        return;
    }
    
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    
    // 添加命令到输出
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(buffer, &end);
    
    char cmd_line[1024];
    snprintf(cmd_line, sizeof(cmd_line), "$ %s\n", command);
    gtk_text_buffer_insert(buffer, &end, cmd_line, -1);
    
    // 执行命令
    FILE *fp = popen(command, "r");
    if (!fp) {
        gtk_text_buffer_insert(buffer, &end, "错误: 无法执行命令\n", -1);
        return;
    }
    
    // 读取输出
    char line[4096];
    while (fgets(line, sizeof(line), fp)) {
        gtk_text_buffer_get_end_iter(buffer, &end);
        gtk_text_buffer_insert(buffer, &end, line, -1);
    }
    
    pclose(fp);
    
    // 添加分隔线
    gtk_text_buffer_get_end_iter(buffer, &end);
    gtk_text_buffer_insert(buffer, &end, "\n----------------------------------------\n", -1);
    
    // 滚动到最后
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(text_view), &end, 0.0, FALSE, 0.0, 0.0);
}

// 查找指定类型的子部件
GtkWidget* find_child_by_type(GtkWidget *parent, GType type) {
    if (GTK_IS_CONTAINER(parent)) {
        GList *children = gtk_container_get_children(GTK_CONTAINER(parent));
        GList *iter = children;
        
        while (iter) {
            GtkWidget *child = GTK_WIDGET(iter->data);
            
            if (G_TYPE_FROM_INSTANCE(child) == type) {
                g_list_free(children);
                return child;
            }
            
            // 递归查找
            GtkWidget *found = find_child_by_type(child, type);
            if (found) {
                g_list_free(children);
                return found;
            }
            
            iter = iter->next;
        }
        
        g_list_free(children);
    }
    
    return NULL;
}
