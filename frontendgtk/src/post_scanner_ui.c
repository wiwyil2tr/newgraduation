/**
 * 端口扫描器用户界面 - 适配完全C实现的扫描器
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "main_window.h"

// 扫描进度对话框
typedef struct {
    GtkWidget *dialog;
    GtkWidget *progress_bar;
    GtkWidget *status_label;
    GtkWidget *details_text;
    GtkTextBuffer *details_buffer;
    GtkWidget *stop_button;
    int scanning;
    pid_t scan_pid;
    int output_fd;
    guint watch_id;
} ScanProgressDialog;

// 扫描结果对话框
typedef struct {
    GtkWidget *dialog;
    GtkWidget *result_tree;
    GtkListStore *result_store;
    GtkWidget *export_button;
    ScanResult *results;
    int result_count;
    char *target;
} ScanResultDialog;

// 全局扫描对话框
static ScanProgressDialog *progress_dialog = NULL;

// 创建端口扫描器选项卡
void create_port_scanner_tab(void) {
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    // 标题
    GtkWidget *label = gtk_label_new("端口扫描器 (C实现)");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    pango_attr_list_insert(attrs, pango_attr_scale_new(1.2));
    gtk_label_set_attributes(GTK_LABEL(label), attrs);
    pango_attr_list_unref(attrs);

    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    // 分隔线
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),
                       FALSE, FALSE, 5);

    // 创建笔记本用于不同设置标签页
    GtkWidget *notebook = gtk_notebook_new();

    // 基本设置标签页
    GtkWidget *basic_page = create_basic_settings_page();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), basic_page, gtk_label_new("基本设置"));

    // 高级设置标签页
    GtkWidget *advanced_page = create_advanced_settings_page();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), advanced_page, gtk_label_new("高级设置"));

    // 输出设置标签页
    GtkWidget *output_page = create_output_settings_page();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), output_page, gtk_label_new("输出设置"));

    gtk_box_pack_start(GTK_BOX(vbox), notebook, FALSE, FALSE, 10);

    // 按钮栏
    GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_CENTER);
    gtk_box_set_spacing(GTK_BOX(button_box), 10);

    GtkWidget *scan_button = gtk_button_new_with_label("开始扫描");
    GtkWidget *stop_button = gtk_button_new_with_label("停止扫描");
    GtkWidget *clear_button = gtk_button_new_with_label("清除设置");
    GtkWidget *help_button = gtk_button_new_with_label("帮助");

    // 设置按钮样式
    gtk_widget_set_size_request(scan_button, 100, 40);
    gtk_widget_set_size_request(stop_button, 100, 40);
    gtk_widget_set_size_request(clear_button, 100, 40);
    gtk_widget_set_size_request(help_button, 100, 40);

    // 设置扫描按钮为建议动作
    GtkStyleContext *context = gtk_widget_get_style_context(scan_button);
    gtk_style_context_add_class(context, "suggested-action");

    // 连接信号
    g_signal_connect(scan_button, "clicked",
                     G_CALLBACK(on_scan_clicked), notebook);
    g_signal_connect(stop_button, "clicked",
                     G_CALLBACK(on_stop_scan), NULL);
    g_signal_connect(clear_button, "clicked",
                     G_CALLBACK(on_clear_settings), notebook);
    g_signal_connect(help_button, "clicked",
                     G_CALLBACK(on_port_scanner_help), NULL);

    gtk_container_add(GTK_CONTAINER(button_box), scan_button);
    gtk_container_add(GTK_CONTAINER(button_box), stop_button);
    gtk_container_add(GTK_CONTAINER(button_box), clear_button);
    gtk_container_add(GTK_CONTAINER(button_box), help_button);

    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 10);

    // 历史记录区域
    GtkWidget *history_frame = gtk_frame_new("扫描历史");
    GtkWidget *history_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(history_box), 5);

    GtkWidget *history_scrolled = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *history_text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(history_text), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(history_text), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(history_text), GTK_WRAP_WORD_CHAR);

    gtk_container_add(GTK_CONTAINER(history_scrolled), history_text);
    gtk_box_pack_start(GTK_BOX(history_box), history_scrolled, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(history_frame), history_box);
    gtk_widget_set_vexpand(history_frame, TRUE);

    gtk_box_pack_start(GTK_BOX(vbox), history_frame, TRUE, TRUE, 10);

    gtk_container_add(GTK_CONTAINER(scrolled_window), vbox);

    // 创建选项卡标签
    GtkWidget *tab_label = gtk_label_new("端口扫描器");

    // 添加到主笔记本
    int page_num = gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                                            scrolled_window,
                                            tab_label);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(notebook), scrolled_window, TRUE);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), page_num);

    // 存储历史文本视图引用
    g_object_set_data(G_OBJECT(scrolled_window), "history-text", history_text);
}

// 创建基本设置页面
GtkWidget* create_basic_settings_page(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    // 目标设置
    GtkWidget *target_frame = gtk_frame_new("目标设置");
    GtkWidget *target_grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(target_grid), 5);
    gtk_grid_set_row_spacing(GTK_GRID(target_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(target_grid), 10);

    GtkWidget *target_label = gtk_label_new("目标地址:");
    target_label = gtk_label_new("目标地址:");
    GtkWidget *target_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(target_entry), "例如: 192.168.1.1 或 example.com");

    gtk_grid_attach(GTK_GRID(target_grid), target_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(target_grid), target_entry, 1, 0, 3, 1);

    // 目标列表
    GtkWidget *target_list_label = gtk_label_new("目标列表:");
    GtkWidget *target_list_scrolled = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *target_list_text = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(target_list_text), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(target_list_scrolled), target_list_text);
    gtk_widget_set_size_request(target_list_scrolled, -1, 100);

    gtk_grid_attach(GTK_GRID(target_grid), target_list_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(target_grid), target_list_scrolled, 1, 1, 3, 1);

    GtkWidget *target_list_help = gtk_label_new("每行一个目标，支持IP地址和域名");
    gtk_widget_set_halign(target_list_help, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(target_grid), target_list_help, 1, 2, 3, 1);

    gtk_container_add(GTK_CONTAINER(target_frame), target_grid);
    gtk_box_pack_start(GTK_BOX(vbox), target_frame, FALSE, FALSE, 0);

    // 端口设置
    GtkWidget *port_frame = gtk_frame_new("端口设置");
    GtkWidget *port_grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(port_grid), 5);
    gtk_grid_set_row_spacing(GTK_GRID(port_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(port_grid), 10);

    // 端口范围
    GtkWidget *port_range_label = gtk_label_new("端口范围:");
    GtkWidget *port_start_entry = gtk_entry_new();
    GtkWidget *port_to_label = gtk_label_new("到");
    GtkWidget *port_end_entry = gtk_entry_new();

    gtk_entry_set_text(GTK_ENTRY(port_start_entry), "1");
    gtk_entry_set_text(GTK_ENTRY(port_end_entry), "1024");

    gtk_grid_attach(GTK_GRID(port_grid), port_range_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(port_grid), port_start_entry, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(port_grid), port_to_label, 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(port_grid), port_end_entry, 3, 0, 1, 1);

    // 自定义端口
    GtkWidget *custom_ports_label = gtk_label_new("自定义端口:");
    GtkWidget *custom_ports_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(custom_ports_entry), "例如: 80,443,8080 或 22-25,80,443");

    gtk_grid_attach(GTK_GRID(port_grid), custom_ports_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(port_grid), custom_ports_entry, 1, 1, 3, 1);

    // 预设端口组
    GtkWidget *preset_label = gtk_label_new("预设端口组:");
    GtkWidget *preset_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(preset_combo), "选择预设...");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(preset_combo), "常用端口 (1-1024)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(preset_combo), "Web服务 (80,443,8080,8443)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(preset_combo), "数据库 (1433,1521,3306,5432,27017)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(preset_combo), "Windows服务 (135,139,445,3389)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(preset_combo), "全端口 (1-65535)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(preset_combo), 0);

    g_signal_connect(preset_combo, "changed",
                     G_CALLBACK(on_preset_ports_changed),
                     custom_ports_entry);

    gtk_grid_attach(GTK_GRID(port_grid), preset_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(port_grid), preset_combo, 1, 2, 3, 1);

    gtk_container_add(GTK_CONTAINER(port_frame), port_grid);
    gtk_box_pack_start(GTK_BOX(vbox), port_frame, FALSE, FALSE, 0);

    // 存储控件引用
    g_object_set_data(G_OBJECT(vbox), "target-entry", target_entry);
    g_object_set_data(G_OBJECT(vbox), "target-list-text", target_list_text);
    g_object_set_data(G_OBJECT(vbox), "port-start-entry", port_start_entry);
    g_object_set_data(G_OBJECT(vbox), "port-end-entry", port_end_entry);
    g_object_set_data(G_OBJECT(vbox), "custom-ports-entry", custom_ports_entry);
    g_object_set_data(G_OBJECT(vbox), "preset-combo", preset_combo);

    return vbox;
}

// 创建高级设置页面
GtkWidget* create_advanced_settings_page(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    // 扫描选项
    GtkWidget *scan_frame = gtk_frame_new("扫描选项");
    GtkWidget *scan_grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(scan_grid), 5);
    gtk_grid_set_row_spacing(GTK_GRID(scan_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(scan_grid), 10);

    // 扫描类型
    GtkWidget *scan_type_label = gtk_label_new("扫描类型:");
    GtkWidget *scan_type_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(scan_type_combo), "TCP Connect (推荐)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(scan_type_combo), "TCP SYN (需要root)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(scan_type_combo), "UDP");
    gtk_combo_box_set_active(GTK_COMBO_BOX(scan_type_combo), 0);

    gtk_grid_attach(GTK_GRID(scan_grid), scan_type_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(scan_grid), scan_type_combo, 1, 0, 3, 1);

    // 线程数
    GtkWidget *threads_label = gtk_label_new("线程数:");
    GtkWidget *threads_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1, 200, 1);
    gtk_range_set_value(GTK_RANGE(threads_scale), 50);
    GtkWidget *threads_value = gtk_label_new("50");

    g_signal_connect(threads_scale, "value-changed",
                     G_CALLBACK(on_threads_changed), threads_value);

    gtk_grid_attach(GTK_GRID(scan_grid), threads_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(scan_grid), threads_scale, 1, 1, 2, 1);
    gtk_grid_attach(GTK_GRID(scan_grid), threads_value, 3, 1, 1, 1);

    // 超时时间
    GtkWidget *timeout_label = gtk_label_new("超时(毫秒):");
    GtkWidget *timeout_spin = gtk_spin_button_new_with_range(100, 10000, 100);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(timeout_spin), 2000);

    gtk_grid_attach(GTK_GRID(scan_grid), timeout_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(scan_grid), timeout_spin, 1, 2, 1, 1);

    // 重试次数
    GtkWidget *retry_label = gtk_label_new("重试次数:");
    GtkWidget *retry_spin = gtk_spin_button_new_with_range(0, 5, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(retry_spin), 1);

    gtk_grid_attach(GTK_GRID(scan_grid), retry_label, 2, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(scan_grid), retry_spin, 3, 2, 1, 1);

    // 横幅抓取
    GtkWidget *banner_check = gtk_check_button_new_with_label("启用横幅抓取");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(banner_check), TRUE);
    gtk_grid_attach(GTK_GRID(scan_grid), banner_check, 0, 3, 4, 1);

    // 详细输出
    GtkWidget *verbose_check = gtk_check_button_new_with_label("显示详细输出");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(verbose_check), FALSE);
    gtk_grid_attach(GTK_GRID(scan_grid), verbose_check, 0, 4, 4, 1);

    gtk_container_add(GTK_CONTAINER(scan_frame), scan_grid);
    gtk_box_pack_start(GTK_BOX(vbox), scan_frame, FALSE, FALSE, 0);

    // 性能选项
    GtkWidget *perf_frame = gtk_frame_new("性能选项");
    GtkWidget *perf_grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(perf_grid), 5);
    gtk_grid_set_row_spacing(GTK_GRID(perf_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(perf_grid), 10);

    // 延迟
    GtkWidget *delay_label = gtk_label_new("延迟(毫秒):");
    GtkWidget *delay_spin = gtk_spin_button_new_with_range(0, 1000, 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(delay_spin), 0);

    gtk_grid_attach(GTK_GRID(perf_grid), delay_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(perf_grid), delay_spin, 1, 0, 1, 1);

    // 批量大小
    GtkWidget *batch_label = gtk_label_new("批量大小:");
    GtkWidget *batch_spin = gtk_spin_button_new_with_range(1, 1000, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(batch_spin), 100);

    gtk_grid_attach(GTK_GRID(perf_grid), batch_label, 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(perf_grid), batch_spin, 3, 0, 1, 1);

    // 随机扫描
    GtkWidget *random_check = gtk_check_button_new_with_label("随机扫描端口顺序");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(random_check), FALSE);
    gtk_grid_attach(GTK_GRID(perf_grid), random_check, 0, 1, 4, 1);

    gtk_container_add(GTK_CONTAINER(perf_frame), perf_grid);
    gtk_box_pack_start(GTK_BOX(vbox), perf_frame, FALSE, FALSE, 0);

    // 存储控件引用
    g_object_set_data(G_OBJECT(vbox), "scan-type-combo", scan_type_combo);
    g_object_set_data(G_OBJECT(vbox), "threads-scale", threads_scale);
    g_object_set_data(G_OBJECT(vbox), "threads-value", threads_value);
    g_object_set_data(G_OBJECT(vbox), "timeout-spin", timeout_spin);
    g_object_set_data(G_OBJECT(vbox), "retry-spin", retry_spin);
    g_object_set_data(G_OBJECT(vbox), "banner-check", banner_check);
    g_object_set_data(G_OBJECT(vbox), "verbose-check", verbose_check);
    g_object_set_data(G_OBJECT(vbox), "delay-spin", delay_spin);
    g_object_set_data(G_OBJECT(vbox), "batch-spin", batch_spin);
    g_object_set_data(G_OBJECT(vbox), "random-check", random_check);

    return vbox;
}

// 创建输出设置页面
GtkWidget* create_output_settings_page(void) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    // 输出格式
    GtkWidget *format_frame = gtk_frame_new("输出格式");
    GtkWidget *format_grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(format_grid), 5);
    gtk_grid_set_row_spacing(GTK_GRID(format_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(format_grid), 10);

    GtkWidget *format_label = gtk_label_new("格式:");
    GtkWidget *format_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format_combo), "文本 (.txt)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format_combo), "CSV (.csv)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format_combo), "JSON (.json)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format_combo), "XML (.xml)");
    gtk_combo_box_set_active(GTK_COMBO_BOX(format_combo), 0);

    gtk_grid_attach(GTK_GRID(format_grid), format_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(format_grid), format_combo, 1, 0, 3, 1);

    // 显示选项
    GtkWidget *show_banner_check = gtk_check_button_new_with_label("结果中显示横幅信息");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_banner_check), TRUE);
    gtk_grid_attach(GTK_GRID(format_grid), show_banner_check, 0, 1, 4, 1);

    GtkWidget *show_closed_check = gtk_check_button_new_with_label("显示关闭端口");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_closed_check), FALSE);
    gtk_grid_attach(GTK_GRID(format_grid), show_closed_check, 0, 2, 4, 1);

    gtk_container_add(GTK_CONTAINER(format_frame), format_grid);
    gtk_box_pack_start(GTK_BOX(vbox), format_frame, FALSE, FALSE, 0);

    // 文件输出
    GtkWidget *file_frame = gtk_frame_new("文件输出");
    GtkWidget *file_grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(file_grid), 5);
    gtk_grid_set_row_spacing(GTK_GRID(file_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(file_grid), 10);

    GtkWidget *save_check = gtk_check_button_new_with_label("保存到文件");
    GtkWidget *file_entry = gtk_entry_new();
    GtkWidget *browse_button = gtk_button_new_with_label("浏览...");

    gtk_entry_set_placeholder_text(GTK_ENTRY(file_entry), "输出文件名");
    gtk_widget_set_sensitive(file_entry, FALSE);
    gtk_widget_set_sensitive(browse_button, FALSE);

    g_signal_connect(save_check, "toggled",
                     G_CALLBACK(on_save_toggled), file_entry);
    g_signal_connect(browse_button, "clicked",
                     G_CALLBACK(on_browse_clicked), file_entry);

    gtk_grid_attach(GTK_GRID(file_grid), save_check, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(file_grid), file_entry, 1, 0, 2, 1);
    gtk_grid_attach(GTK_GRID(file_grid), browse_button, 3, 0, 1, 1);

    // 自动命名
    GtkWidget *auto_name_check = gtk_check_button_new_with_label("自动生成文件名");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(auto_name_check), TRUE);
    gtk_grid_attach(GTK_GRID(file_grid), auto_name_check, 0, 1, 4, 1);

    gtk_container_add(GTK_CONTAINER(file_frame), file_grid);
    gtk_box_pack_start(GTK_BOX(vbox), file_frame, FALSE, FALSE, 0);

    // 数据库输出
    GtkWidget *db_frame = gtk_frame_new("数据库输出 (可选)");
    GtkWidget *db_grid = gtk_grid_new();
    gtk_container_set_border_width(GTK_CONTAINER(db_grid), 5);
    gtk_grid_set_row_spacing(GTK_GRID(db_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(db_grid), 10);

    GtkWidget *db_check = gtk_check_button_new_with_label("保存到数据库");
    GtkWidget *db_path_label = gtk_label_new("数据库路径:");
    GtkWidget *db_path_entry = gtk_entry_new();
    GtkWidget *db_browse_button = gtk_button_new_with_label("浏览...");

    gtk_entry_set_text(GTK_ENTRY(db_path_entry), "./scan_results.db");
    gtk_widget_set_sensitive(db_path_entry, FALSE);
    gtk_widget_set_sensitive(db_browse_button, FALSE);

    g_signal_connect(db_check, "toggled",
                     G_CALLBACK(on_db_check_toggled), db_path_entry);

    gtk_grid_attach(GTK_GRID(db_grid), db_check, 0, 0, 4, 1);
    gtk_grid_attach(GTK_GRID(db_grid), db_path_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(db_grid), db_path_entry, 1, 1, 2, 1);
    gtk_grid_attach(GTK_GRID(db_grid), db_browse_button, 3, 1, 1, 1);

    gtk_container_add(GTK_CONTAINER(db_frame), db_grid);
    gtk_box_pack_start(GTK_BOX(vbox), db_frame, FALSE, FALSE, 0);

    // 存储控件引用
    g_object_set_data(G_OBJECT(vbox), "format-combo", format_combo);
    g_object_set_data(G_OBJECT(vbox), "show-banner-check", show_banner_check);
    g_object_set_data(G_OBJECT(vbox), "show-closed-check", show_closed_check);
    g_object_set_data(G_OBJECT(vbox), "save-check", save_check);
    g_object_set_data(G_OBJECT(vbox), "file-entry", file_entry);
    g_object_set_data(G_OBJECT(vbox), "browse-button", browse_button);
    g_object_set_data(G_OBJECT(vbox), "auto-name-check", auto_name_check);
    g_object_set_data(G_OBJECT(vbox), "db-check", db_check);
    g_object_set_data(G_OBJECT(vbox), "db-path-entry", db_path_entry);

    return vbox;
}

// 预设端口选择回调
void on_preset_ports_changed(GtkComboBox *combo, gpointer data) {
    GtkWidget *custom_ports_entry = GTK_WIDGET(data);
    int active = gtk_combo_box_get_active(combo);

    switch (active) {
        case 1:  // 常用端口
            gtk_entry_set_text(GTK_ENTRY(custom_ports_entry), "1-1024");
            break;
        case 2:  // Web服务
            gtk_entry_set_text(GTK_ENTRY(custom_ports_entry), "80,443,8080,8443");
            break;
        case 3:  // 数据库
            gtk_entry_set_text(GTK_ENTRY(custom_ports_entry), "1433,1521,3306,5432,27017");
            break;
        case 4:  // Windows服务
            gtk_entry_set_text(GTK_ENTRY(custom_ports_entry), "135,139,445,3389");
            break;
        case 5:  // 全端口
            gtk_entry_set_text(GTK_ENTRY(custom_ports_entry), "1-65535");
            break;
        default:
            gtk_entry_set_text(GTK_ENTRY(custom_ports_entry), "");
            break;
    }
}

// 线程数改变回调
void on_threads_changed(GtkRange *range, gpointer data) {
    GtkWidget *label = GTK_WIDGET(data);
    int value = (int)gtk_range_get_value(range);
    char text[32];
    snprintf(text, sizeof(text), "%d", value);
    gtk_label_set_text(GTK_LABEL(label), text);
}

// 保存复选框状态变化回调
void on_save_toggled(GtkToggleButton *button, gpointer data) {
    GtkWidget *file_entry = GTK_WIDGET(data);
    GtkWidget *browse_button = g_object_get_data(G_OBJECT(button), "browse-button");
    gboolean active = gtk_toggle_button_get_active(button);

    gtk_widget_set_sensitive(file_entry, active);
    if (browse_button) {
        gtk_widget_set_sensitive(browse_button, active);
    }
}

// 数据库复选框状态变化回调
void on_db_check_toggled(GtkToggleButton *button, gpointer data) {
    GtkWidget *db_path_entry = GTK_WIDGET(data);
    GtkWidget *db_browse_button = g_object_get_data(G_OBJECT(button), "db-browse-button");
    gboolean active = gtk_toggle_button_get_active(button);

    gtk_widget_set_sensitive(db_path_entry, active);
    if (db_browse_button) {
        gtk_widget_set_sensitive(db_browse_button, active);
    }
}

// 浏览按钮回调
void on_browse_clicked(GtkButton *button, gpointer data) {
    GtkWidget *file_entry = GTK_WIDGET(data);

    GtkWidget *dialog = gtk_file_chooser_dialog_new("保存文件",
                                                    GTK_WINDOW(main_window),
                                                    GTK_FILE_CHOOSER_ACTION_SAVE,
                                                    "取消", GTK_RESPONSE_CANCEL,
                                                    "保存", GTK_RESPONSE_ACCEPT,
                                                    NULL);

    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

    // 设置默认文件名
    const char *current_text = gtk_entry_get_text(GTK_ENTRY(file_entry));
    if (current_text && strlen(current_text) > 0) {
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), current_text);
    } else {
        // 自动生成文件名
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char default_name[256];
        strftime(default_name, sizeof(default_name), "scan_%Y%m%d_%H%M%S.txt", tm_info);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), default_name);
    }

    // 设置过滤器
    GtkFileFilter *filter_txt = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_txt, "文本文件 (*.txt)");
    gtk_file_filter_add_pattern(filter_txt, "*.txt");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_txt);

    GtkFileFilter *filter_csv = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_csv, "CSV文件 (*.csv)");
    gtk_file_filter_add_pattern(filter_csv, "*.csv");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_csv);

    GtkFileFilter *filter_json = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_json, "JSON文件 (*.json)");
    gtk_file_filter_add_pattern(filter_json, "*.json");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_json);

    GtkFileFilter *filter_all = gtk_file_filter_new();
    gtk_file_filter_set_name(filter_all, "所有文件 (*)");
    gtk_file_filter_add_pattern(filter_all, "*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_all);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        gtk_entry_set_text(GTK_ENTRY(file_entry), filename);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

// 扫描按钮回调
void on_scan_clicked(GtkButton *button, gpointer data) {
    GtkWidget *notebook = GTK_WIDGET(data);

    // 获取基本设置
    GtkWidget *basic_page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 0);
    GtkWidget *target_entry = g_object_get_data(G_OBJECT(basic_page), "target-entry");
    GtkWidget *target_list_text = g_object_get_data(G_OBJECT(basic_page), "target-list-text");
    GtkWidget *port_start_entry = g_object_get_data(G_OBJECT(basic_page), "port-start-entry");
    GtkWidget *port_end_entry = g_object_get_data(G_OBJECT(basic_page), "port-end-entry");
    GtkWidget *custom_ports_entry = g_object_get_data(G_OBJECT(basic_page), "custom-ports-entry");

    // 获取高级设置
    GtkWidget *advanced_page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 1);
    GtkWidget *scan_type_combo = g_object_get_data(G_OBJECT(advanced_page), "scan-type-combo");
    GtkWidget *threads_scale = g_object_get_data(G_OBJECT(advanced_page), "threads-scale");
    GtkWidget *timeout_spin = g_object_get_data(G_OBJECT(advanced_page), "timeout-spin");
    GtkWidget *banner_check = g_object_get_data(G_OBJECT(advanced_page), "banner-check");
    GtkWidget *verbose_check = g_object_get_data(G_OBJECT(advanced_page), "verbose-check");

    // 获取输出设置
    GtkWidget *output_page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 2);
    GtkWidget *format_combo = g_object_get_data(G_OBJECT(output_page), "format-combo");
    GtkWidget *save_check = g_object_get_data(G_OBJECT(output_page), "save-check");
    GtkWidget *file_entry = g_object_get_data(G_OBJECT(output_page), "file-entry");
    GtkWidget *auto_name_check = g_object_get_data(G_OBJECT(output_page), "auto-name-check");

    // 验证输入
    const char *target = gtk_entry_get_text(GTK_ENTRY(target_entry));
    GtkTextBuffer *target_list_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(target_list_text));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(target_list_buffer, &start, &end);
    char *target_list = gtk_text_buffer_get_text(target_list_buffer, &start, &end, FALSE);

    const char *custom_ports = gtk_entry_get_text(GTK_ENTRY(custom_ports_entry));
    const char *port_start = gtk_entry_get_text(GTK_ENTRY(port_start_entry));
    const char *port_end = gtk_entry_get_text(GTK_ENTRY(port_end_entry));

    // 确定目标
    char final_target[256] = {0};
    if (strlen(target) > 0) {
        strcpy(final_target, target);
    } else if (target_list && strlen(target_list) > 0) {
        // 使用第一个目标
        char *first_line = strtok(target_list, "\n");
        if (first_line) {
            strcpy(final_target, first_line);
        }
    }

    if (strlen(final_target) == 0) {
        show_error_dialog("请输入扫描目标");
        g_free(target_list);
        return;
    }

    // 确定端口范围
    char port_range[256] = {0};
    if (custom_ports && strlen(custom_ports) > 0) {
        strcpy(port_range, custom_ports);
    } else {
        snprintf(port_range, sizeof(port_range), "%s-%s", port_start, port_end);
    }

    if (strlen(port_range) == 0) {
        show_error_dialog("请输入端口范围");
        g_free(target_list);
        return;
    }

    // 构建命令
    GString *command = g_string_new(backend_path);
    g_string_append(command, " port-scanner scan ");
    g_string_append(command, final_target);

    // 添加端口参数
    g_string_append_printf(command, " -p \"%s\"", port_range);

    // 添加线程数
    int threads = (int)gtk_range_get_value(GTK_RANGE(threads_scale));
    g_string_append_printf(command, " -t %d", threads);

    // 添加超时时间
    int timeout = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(timeout_spin));
    g_string_append_printf(command, " -T %d", timeout);

    // 添加扫描类型
    int scan_type = gtk_combo_box_get_active(GTK_COMBO_BOX(scan_type_combo));
    const char *scan_type_str = "connect";
    if (scan_type == 1) {
        scan_type_str = "syn";
    } else if (scan_type == 2) {
        scan_type_str = "udp";
    }
    g_string_append_printf(command, " -s %s", scan_type_str);

    // 添加横幅抓取
    gboolean banner = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(banner_check));
    if (banner) {
        g_string_append(command, " -b");
    }

    // 添加详细输出
    gboolean verbose = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(verbose_check));
    if (verbose) {
        g_string_append(command, " -v");
    }

    // 添加输出文件
    gboolean save = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(save_check));
    if (save) {
        const char *filename = gtk_entry_get_text(GTK_ENTRY(file_entry));
        gboolean auto_name = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(auto_name_check));

        char final_filename[256] = {0};
        if (auto_name) {
            // 自动生成文件名
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y%m%d_%H%M%S", tm_info);

            int format = gtk_combo_box_get_active(GTK_COMBO_BOX(format_combo));
            const char *ext = ".txt";
            if (format == 1) {
                ext = ".csv";
            } else if (format == 2) {
                ext = ".json";
            } else if (format == 3) {
                ext = ".xml";
            }

            // 清理目标字符串用于文件名
            char safe_target[128];
            strncpy(safe_target, final_target, sizeof(safe_target) - 1);
            for (char *p = safe_target; *p; p++) {
                if (*p == '.' || *p == ':') *p = '_';
                if (*p == '/') *p = '-';
            }

            snprintf(final_filename, sizeof(final_filename),
                     "scan_%s_%s%s", safe_target, time_str, ext);
        } else if (filename && strlen(filename) > 0) {
            strcpy(final_filename, filename);
        }

        if (strlen(final_filename) > 0) {
            // 添加输出格式
            int format = gtk_combo_box_get_active(GTK_COMBO_BOX(format_combo));
            const char *format_str = "txt";
            if (format == 1) {
                format_str = "csv";
            } else if (format == 2) {
                format_str = "json";
            } else if (format == 3) {
                format_str = "xml";
            }
            g_string_append_printf(command, " -o \"%s\" -f %s", final_filename, format_str);
        }
    }

    printf("执行命令: %s\n", command->str);

    // 显示进度对话框
    show_scan_progress_dialog(final_target, port_range, command->str);

    g_string_free(command, TRUE);
    g_free(target_list);
}

// 显示扫描进度对话框
void show_scan_progress_dialog(const char *target, const char *port_range, const char *command) {
    // 创建对话框
    progress_dialog = g_new0(ScanProgressDialog, 1);

    progress_dialog->dialog = gtk_dialog_new_with_buttons(
        "正在扫描...",
        GTK_WINDOW(main_window),
                                                          GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                          "停止扫描",
                                                          GTK_RESPONSE_CANCEL,
                                                          NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(progress_dialog->dialog));

    // 创建进度信息
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    // 目标信息
    char info_text[256];
    snprintf(info_text, sizeof(info_text), "目标: %s\n端口: %s", target, port_range);
    GtkWidget *info_label = gtk_label_new(info_text);
    gtk_widget_set_halign(info_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), info_label, FALSE, FALSE, 0);

    // 进度条
    progress_dialog->progress_bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(vbox), progress_dialog->progress_bar, FALSE, FALSE, 0);

    // 状态标签
    progress_dialog->status_label = gtk_label_new("正在初始化扫描...");
    gtk_box_pack_start(GTK_BOX(vbox), progress_dialog->status_label, FALSE, FALSE, 0);

    // 详细输出区域
    GtkWidget *details_frame = gtk_frame_new("扫描详情");
    GtkWidget *details_scrolled = gtk_scrolled_window_new(NULL, NULL);
    progress_dialog->details_text = gtk_text_view_new();

    gtk_text_view_set_editable(GTK_TEXT_VIEW(progress_dialog->details_text), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(progress_dialog->details_text), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(progress_dialog->details_text), GTK_WRAP_WORD_CHAR);

    progress_dialog->details_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(progress_dialog->details_text));

    gtk_container_add(GTK_CONTAINER(details_scrolled), progress_dialog->details_text);
    gtk_container_add(GTK_CONTAINER(details_frame), details_scrolled);
    gtk_widget_set_size_request(details_frame, 600, 200);

    gtk_box_pack_start(GTK_BOX(vbox), details_frame, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(content_area), vbox);

    // 连接信号
    g_signal_connect(progress_dialog->dialog, "response",
                     G_CALLBACK(on_progress_dialog_response), NULL);

    // 获取停止按钮
    progress_dialog->stop_button = gtk_dialog_get_widget_for_response(
        GTK_DIALOG(progress_dialog->dialog), GTK_RESPONSE_CANCEL);

    // 开始扫描
    start_scan_process(command);

    gtk_widget_show_all(progress_dialog->dialog);
}

// 开始扫描进程
void start_scan_process(const char *command) {
    // 创建管道用于读取输出
    int stdout_pipe[2];
    if (pipe(stdout_pipe) == -1) {
        perror("创建管道失败");
        return;
    }

    pid_t pid = fork();
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
    } else if (pid > 0) {
        // 父进程
        close(stdout_pipe[1]);  // 关闭写端
        progress_dialog->scan_pid = pid;
        progress_dialog->output_fd = stdout_pipe[0];
        progress_dialog->scanning = 1;

        // 添加IO监视器
        progress_dialog->watch_id = g_io_add_watch(
            g_io_channel_unix_new(stdout_pipe[0]),
                                                   G_IO_IN | G_IO_HUP,
                                                   (GIOFunc)on_scan_output,
                                                   progress_dialog);
    } else {
        perror("fork失败");
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
    }
}

// 扫描输出回调
gboolean on_scan_output(GIOChannel *channel, GIOCondition cond, gpointer data) {
    ScanProgressDialog *dialog = (ScanProgressDialog *)data;

    if (cond & G_IO_IN) {
        char buffer[4096];
        gsize bytes_read = 0;
        GError *error = NULL;

        GIOStatus status = g_io_channel_read_chars(
            channel, buffer, sizeof(buffer) - 1, &bytes_read, &error);

        if (status == G_IO_STATUS_NORMAL && bytes_read > 0) {
            buffer[bytes_read] = '\0';

            // 在详细输出区域显示
            GtkTextIter end;
            gtk_text_buffer_get_end_iter(dialog->details_buffer, &end);
            gtk_text_buffer_insert(dialog->details_buffer, &end, buffer, -1);

            // 滚动到最后
            GtkTextMark *mark = gtk_text_buffer_create_mark(dialog->details_buffer,
                                                            "scroll", &end, FALSE);
            gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(dialog->details_text),
                                         mark, 0.0, FALSE, 0.0, 0.0);
            gtk_text_buffer_delete_mark(dialog->details_buffer, mark);

            // 解析进度信息
            parse_scan_progress(buffer, dialog);
        }

        if (error) {
            g_error_free(error);
        }
    }

    if (cond & G_IO_HUP) {
        // 扫描完成
        g_io_channel_unref(channel);
        dialog->watch_id = 0;
        dialog->scanning = 0;

        // 等待子进程
        int status;
        waitpid(dialog->scan_pid, &status, 0);

        // 更新状态
        gtk_label_set_text(GTK_LABEL(dialog->status_label), "扫描完成");
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dialog->progress_bar), 1.0);

        // 更改按钮文本
        gtk_button_set_label(GTK_BUTTON(dialog->stop_button), "关闭");

        // 解析扫描结果
        parse_scan_results(dialog->details_buffer);

        return FALSE;  // 移除监视器
    }

    return TRUE;  // 继续监视
}

// 解析扫描进度
void parse_scan_progress(const char *output, ScanProgressDialog *dialog) {
    // 查找进度信息
    const char *progress_ptr = strstr(output, "进度:");
    if (progress_ptr) {
        int scanned = 0, total = 0;
        float percent = 0;

        if (sscanf(progress_ptr, "进度: %d/%d", &scanned, &total) == 2) {
            if (total > 0) {
                percent = (float)scanned / total;
                gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dialog->progress_bar), percent);

                char status[128];
                snprintf(status, sizeof(status), "已扫描: %d/%d (%.1f%%)", scanned, total, percent * 100);
                gtk_label_set_text(GTK_LABEL(dialog->status_label), status);
            }
        }
    }

    // 查找开放端口信息
    const char *open_ptr = strstr(output, "开放端口:");
    if (open_ptr) {
        int open_ports = 0;
        if (sscanf(open_ptr, "开放端口: %d", &open_ports) == 1) {
            char status[128];
            snprintf(status, sizeof(status), "发现开放端口: %d", open_ports);
            gtk_label_set_text(GTK_LABEL(dialog->status_label), status);
        }
    }
}

// 解析扫描结果
void parse_scan_results(GtkTextBuffer *buffer) {
    // 获取完整输出
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(buffer, &start, &end);
    char *output = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

    // 这里可以添加结果解析逻辑
    // 例如：提取开放端口列表，统计信息等

    printf("扫描输出:\n%s\n", output);

    g_free(output);
}

// 进度对话框响应回调
void on_progress_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    if (progress_dialog) {
        if (progress_dialog->scanning) {
            // 停止扫描
            kill(progress_dialog->scan_pid, SIGTERM);
            progress_dialog->scanning = 0;

            // 更新状态
            gtk_label_set_text(GTK_LABEL(progress_dialog->status_label), "扫描已停止");
            gtk_button_set_label(GTK_BUTTON(progress_dialog->stop_button), "关闭");
        } else {
            // 关闭对话框
            gtk_widget_destroy(progress_dialog->dialog);

            // 清理资源
            if (progress_dialog->watch_id > 0) {
                g_source_remove(progress_dialog->watch_id);
            }
            if (progress_dialog->output_fd > 0) {
                close(progress_dialog->output_fd);
            }

            g_free(progress_dialog);
            progress_dialog = NULL;
        }
    }
}

// 停止扫描回调
void on_stop_scan(GtkButton *button, gpointer data) {
    if (progress_dialog && progress_dialog->scanning) {
        // 模拟点击停止按钮
        gtk_dialog_response(GTK_DIALOG(progress_dialog->dialog), GTK_RESPONSE_CANCEL);
    }
}

// 清除设置回调
void on_clear_settings(GtkButton *button, gpointer data) {
    GtkWidget *notebook = GTK_WIDGET(data);

    // 清除基本设置
    GtkWidget *basic_page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 0);
    GtkWidget *target_entry = g_object_get_data(G_OBJECT(basic_page), "target-entry");
    GtkWidget *target_list_text = g_object_get_data(G_OBJECT(basic_page), "target-list-text");
    GtkWidget *port_start_entry = g_object_get_data(G_OBJECT(basic_page), "port-start-entry");
    GtkWidget *port_end_entry = g_object_get_data(G_OBJECT(basic_page), "port-end-entry");
    GtkWidget *custom_ports_entry = g_object_get_data(G_OBJECT(basic_page), "custom-ports-entry");
    GtkWidget *preset_combo = g_object_get_data(G_OBJECT(basic_page), "preset-combo");

    gtk_entry_set_text(GTK_ENTRY(target_entry), "");
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(target_list_text)), "");
    gtk_entry_set_text(GTK_ENTRY(port_start_entry), "1");
    gtk_entry_set_text(GTK_ENTRY(port_end_entry), "1024");
    gtk_entry_set_text(GTK_ENTRY(custom_ports_entry), "");
    gtk_combo_box_set_active(GTK_COMBO_BOX(preset_combo), 0);

    // 重置高级设置
    GtkWidget *advanced_page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 1);
    GtkWidget *scan_type_combo = g_object_get_data(G_OBJECT(advanced_page), "scan-type-combo");
    GtkWidget *threads_scale = g_object_get_data(G_OBJECT(advanced_page), "threads-scale");
    GtkWidget *threads_value = g_object_get_data(G_OBJECT(advanced_page), "threads-value");
    GtkWidget *timeout_spin = g_object_get_data(G_OBJECT(advanced_page), "timeout-spin");
    GtkWidget *retry_spin = g_object_get_data(G_OBJECT(advanced_page), "retry-spin");
    GtkWidget *banner_check = g_object_get_data(G_OBJECT(advanced_page), "banner-check");
    GtkWidget *verbose_check = g_object_get_data(G_OBJECT(advanced_page), "verbose-check");

    gtk_combo_box_set_active(GTK_COMBO_BOX(scan_type_combo), 0);
    gtk_range_set_value(GTK_RANGE(threads_scale), 50);
    gtk_label_set_text(GTK_LABEL(threads_value), "50");
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(timeout_spin), 2000);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(retry_spin), 1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(banner_check), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(verbose_check), FALSE);

    // 重置输出设置
    GtkWidget *output_page = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 2);
    GtkWidget *format_combo = g_object_get_data(G_OBJECT(output_page), "format-combo");
    GtkWidget *save_check = g_object_get_data(G_OBJECT(output_page), "save-check");
    GtkWidget *file_entry = g_object_get_data(G_OBJECT(output_page), "file-entry");
    GtkWidget *auto_name_check = g_object_get_data(G_OBJECT(output_page), "auto-name-check");

    gtk_combo_box_set_active(GTK_COMBO_BOX(format_combo), 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(save_check), FALSE);
    gtk_entry_set_text(GTK_ENTRY(file_entry), "");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(auto_name_check), TRUE);

    update_status("设置已清除");
}

// 端口扫描器帮助回调
void on_port_scanner_help(GtkButton *button, gpointer data) {
    char command[256];
    snprintf(command, sizeof(command), "%s help port-scanner", backend_path);

    FILE *fp = popen(command, "r");
    if (!fp) {
        return;
    }

    // 创建帮助对话框
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "端口扫描器帮助",
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

    gtk_widget_set_size_request(dialog, 600, 400);
    gtk_widget_show_all(dialog);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}
