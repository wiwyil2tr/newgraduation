#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <gtk/gtk.h>

// 主窗口函数
GtkWidget* create_main_window(GtkApplication *app);
void update_status(const char *message);

// 模块面板函数
void create_module_panel(GtkWidget *parent);
void load_module_list(void);
void add_module_to_list(const char *name, const char *category,
                        const char *description, const char *version,
                        const char *author);
void on_module_activated(GtkTreeView *tree_view,
                         GtkTreePath *path,
                         GtkTreeViewColumn *column,
                         gpointer user_data);

// 端口扫描器函数
void create_port_scanner_tab(void);
void on_common_ports_changed(GtkComboBox *combo, gpointer data);
void on_save_toggled(GtkToggleButton *button, gpointer data);
void on_browse_clicked(GtkButton *button, gpointer data);
void on_scan_clicked(GtkButton *button, gpointer data);
void on_stop_scan(GtkButton *button, gpointer data);
void on_clear_results(GtkButton *button, gpointer data);
void on_port_scanner_help(GtkButton *button, gpointer data);

// 通用模块函数
void create_generic_module_tab(const char *module_name);
void on_generic_run(GtkButton *button, gpointer data);
void on_generic_help(GtkButton *button, gpointer data);
void execute_command_and_display(const char *command, GtkWidget *text_view);
GtkWidget* find_child_by_type(GtkWidget *parent, GType type);

// 其他回调函数
void on_new_tab(GtkMenuItem *item, gpointer data);
void on_open_port_scanner(GtkMenuItem *item, gpointer data);
void on_run_command(GtkToolButton *button, gpointer data);
void on_stop_command(GtkToolButton *button, gpointer data);
void on_clear_output(GtkToolButton *button, gpointer data);
void on_tab_switched(GtkNotebook *notebook, GtkWidget *page,
                     guint page_num, gpointer data);
void on_about(GtkMenuItem *item, gpointer data);

// 工具函数
void show_error_dialog(const char *message);

// 扫描参数结构
typedef struct {
    GtkWidget *target_entry;
    GtkWidget *port_start_entry;
    GtkWidget *port_end_entry;
    GtkWidget *custom_ports_entry;
    GtkWidget *threads_spin;
    GtkWidget *timeout_spin;
    GtkWidget *scan_type_combo;
    GtkWidget *banner_check;
    GtkWidget *format_combo;
    GtkWidget *save_check;
    GtkWidget *file_entry;
    GtkListStore *result_store;
    GtkWidget *open_ports_label;
    GtkWidget *filtered_ports_label;
    GtkWidget *closed_ports_label;
} ScanParams;

#endif // MAIN_WINDOW_H
