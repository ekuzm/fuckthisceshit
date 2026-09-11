#include "ui.hpp"
#include "logic/logic.hpp"

#include <gtk/gtk.h>

#include <algorithm>
#include <cstring>
#include <string>

namespace power_widget {
namespace {

struct App {
	Logic logic;
	GtkWidget *window{}, *canvas{}, *log_window{}, *filter{}, *log_view{}, *menu{};
	std::string selected;
};

void update_state(App& app) {
	const auto& batteries = app.logic.state.batteries;
	if (!find_battery(app.logic.state, app.selected)) {
		app.selected = batteries.empty() ? "" : batteries.front().name;
	}
	gtk_widget_queue_draw(app.canvas);
}

void render_log(App& app) {
	if (!app.log_view) {
		return;
	}
	const auto selected = static_cast<Kind>(gtk_combo_box_get_active(GTK_COMBO_BOX(app.filter)));
	std::string text;
	for (const auto& event : app.logic.events) {
		if (selected == Kind::All || selected == event.kind) {
			text += event.line + '\n';
		}
	}
	auto* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app.log_view));
	gtk_text_buffer_set_text(buffer, text.c_str(), static_cast<gint>(text.size()));
	GtkTextIter end;
	gtk_text_buffer_get_end_iter(buffer, &end);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(app.log_view), &end, 0, FALSE, 0, 0);
}

// Pango обеспечивает корректную кириллицу и обрезку длинных строк.
void label(cairo_t* cr, const std::string& text, double y, int size, bool muted = false) {
	PangoLayout* layout = pango_cairo_create_layout(cr);
	auto* font = pango_font_description_new();
	pango_font_description_set_family(font, "Sans");
	pango_font_description_set_absolute_size(font, size * PANGO_SCALE);
	pango_layout_set_font_description(layout, font);
	pango_layout_set_width(layout, 280 * PANGO_SCALE);
	pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	const auto safe = valid_utf8(text);
	pango_layout_set_text(layout, safe.c_str(), -1);
	if (muted) {
		cairo_set_source_rgb(cr, 0.65, 0.71, 0.80);
	} else {
		cairo_set_source_rgb(cr, 0.94, 0.96, 1.0);
	}
	cairo_move_to(cr, 10, y);
	pango_cairo_show_layout(cr, layout);
	pango_font_description_free(font);
	g_object_unref(layout);
}

void draw_charge_ring(cairo_t* cr, double value) {
	constexpr double pi = 3.14159265358979323846;
	cairo_set_line_width(cr, 9);
	cairo_set_source_rgb(cr, 0.16, 0.21, 0.29);
	cairo_arc(cr, 150, 69, 44, 0, 2 * pi);
	cairo_stroke(cr);
	if (value >= 0) {
		if (value >= 50) {
			cairo_set_source_rgb(cr, 0.20, 0.83, 0.60);
		} else if (value >= 20) {
			cairo_set_source_rgb(cr, 0.98, 0.76, 0.20);
		} else {
			cairo_set_source_rgb(cr, 0.98, 0.33, 0.36);
		}
		cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
		cairo_arc(cr, 150, 69, 44, -pi / 2, -pi / 2 + 2 * pi * value / 100);
		cairo_stroke(cr);
	}
}

std::string battery_detail(const Battery* battery) {
	if (!battery) {
		return "Данные батареи недоступны";
	}
	if (battery->status != "Discharging") {
		return status_name(battery->status);
	}
	if (battery->minutes < 0) {
		return "Оставшееся время неизвестно";
	}
	const auto minutes = static_cast<long long>(std::min(battery->minutes, 1e9));
	return "Осталось ≈ " + std::to_string(minutes) + " мин";
}

gboolean draw(GtkWidget* widget, cairo_t* cr, gpointer data) {
	auto& app = *static_cast<App*>(data);
	cairo_scale(cr, gtk_widget_get_allocated_width(widget) / 300.0,
	            gtk_widget_get_allocated_height(widget) / 200.0);
	cairo_set_source_rgb(cr, 0.06, 0.09, 0.15);
	cairo_paint(cr);
	const Battery* battery = find_battery(app.logic.state, app.selected);
	const double value = battery ? battery->percent : -1;
	draw_charge_ring(cr, value);
	label(cr, value >= 0 ? percent(value) : "—", 48, 28);
	label(cr, battery ? battery->name : "Нет батареи", 83, 11, true);
	label(cr, source_name(app.logic.state.online), 122, 14);
	label(cr, battery_detail(battery), 145, 12, true);
	label(cr, "ЛКМ: переместить · ПКМ: меню", 178, 11, true);
	return TRUE;
}

void show_error(App& app, const std::string& text) {
	auto* dialog =
	    gtk_message_dialog_new(GTK_WINDOW(app.window), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
		                       GTK_BUTTONS_CLOSE, "%s", valid_utf8(text).c_str());
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

void on_save_log(GtkButton*, gpointer data) {
	auto& app = *static_cast<App*>(data);
	auto* dialog = gtk_file_chooser_dialog_new(
	    "Сохранить полный журнал", GTK_WINDOW(app.log_window), GTK_FILE_CHOOSER_ACTION_SAVE,
	    "Отмена", GTK_RESPONSE_CANCEL, "Сохранить", GTK_RESPONSE_ACCEPT, nullptr);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "power-report.txt");
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		save_log(app.logic, path);
		g_free(path);
	}
	gtk_widget_destroy(dialog);
}

void on_filter_changed(GtkComboBox*, gpointer data) {
	render_log(*static_cast<App*>(data));
}

void create_log_window(App& app) {
	app.log_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(app.log_window), "Журнал энергопитания");
	gtk_window_set_default_size(GTK_WINDOW(app.log_window), 780, 440);
	// Закрытие журнала скрывает его, виджет и мониторинг продолжают работать.
	g_signal_connect(app.log_window, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete),
	                 nullptr);
	auto* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_container_set_border_width(GTK_CONTAINER(box), 12);
	gtk_container_add(GTK_CONTAINER(app.log_window), box);
	app.filter = gtk_combo_box_text_new();
	for (const char* text :
	     {"Все события", "Подключение / отключение ЗУ", "Изменение заряда", "Переходы в сон"}) {
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.filter), text);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(app.filter), 0);
	g_signal_connect(app.filter, "changed", G_CALLBACK(on_filter_changed), &app);
	gtk_box_pack_start(GTK_BOX(box), app.filter, FALSE, FALSE, 0);
	auto* scroll = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);
	app.log_view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(app.log_view), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app.log_view), FALSE);
	gtk_text_view_set_monospace(GTK_TEXT_VIEW(app.log_view), TRUE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app.log_view), GTK_WRAP_WORD_CHAR);
	gtk_container_add(GTK_CONTAINER(scroll), app.log_view);
	auto* save = gtk_button_new_with_label("Сохранить полный журнал…");
	g_signal_connect(save, "clicked", G_CALLBACK(on_save_log), &app);
	gtk_box_pack_start(GTK_BOX(box), save, FALSE, FALSE, 0);
}

void show_log(GtkMenuItem*, gpointer data) {
	auto& app = *static_cast<App*>(data);
	if (!app.log_window) {
		create_log_window(app);
	}
	gtk_widget_show_all(app.log_window);
	render_log(app);
	gtk_window_present(GTK_WINDOW(app.log_window));
}

void power_action(GtkMenuItem* item, gpointer data) {
	auto& app = *static_cast<App*>(data);
	if (app.logic.busy) {
		return;
	}
	const char* action = static_cast<const char*>(g_object_get_data(G_OBJECT(item), "action"));
	auto* dialog = gtk_message_dialog_new(
	    GTK_WINDOW(app.window), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, "%s",
	    std::strcmp(action, "suspend") == 0 ? "Перевести компьютер в спящий режим?"
		                                    : "Перевести компьютер в гибернацию?");
	const bool accepted = gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK;
	gtk_widget_destroy(dialog);
	if (!accepted) {
		return;
	}
	request_power_action(app.logic, action);
}

void autostart(GtkCheckMenuItem* item, gpointer data) {
	auto& app = *static_cast<App*>(data);
	set_autostart(app.logic, gtk_check_menu_item_get_active(item));
}

GtkWidget* menu_item(App& app, const char* text, GCallback callback) {
	auto* item = gtk_menu_item_new_with_label(text);
	gtk_menu_shell_append(GTK_MENU_SHELL(app.menu), item);
	g_signal_connect(item, "activate", callback, &app);
	return item;
}

void on_battery_selected(GtkMenuItem* selected, gpointer p) {
	auto& current = *static_cast<App*>(p);
	current.selected = static_cast<const char*>(g_object_get_data(G_OBJECT(selected), "battery"));
	gtk_widget_queue_draw(current.canvas);
}

void add_battery_menu(App& app) {
	if (app.logic.state.batteries.size() <= 1) {
		return;
	}
	auto* choose = gtk_menu_item_new_with_label("Батарея");
	auto* submenu = gtk_menu_new();
	for (const auto& battery : app.logic.state.batteries) {
		auto* item = gtk_menu_item_new_with_label(
		    (battery.name + (battery.name == app.selected ? " ✓" : "")).c_str());
		g_object_set_data_full(G_OBJECT(item), "battery", g_strdup(battery.name.c_str()), g_free);
		g_signal_connect(item, "activate", G_CALLBACK(on_battery_selected), &app);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
	}
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(choose), submenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(app.menu), choose);
}

void on_refresh(GtkMenuItem*, gpointer data) {
	refresh(static_cast<App*>(data)->logic);
}

void on_exit(GtkMenuItem*, gpointer) {
	gtk_main_quit();
}

void popup(App& app, GdkEvent* event) {
	if (app.menu) {
		gtk_widget_destroy(app.menu);
	}
	app.menu = gtk_menu_new();
	auto* suspend = menu_item(app, "Спящий режим", G_CALLBACK(power_action));
	g_object_set_data(G_OBJECT(suspend), "action", const_cast<char*>("suspend"));
	auto* hibernate = menu_item(app, "Гибернация", G_CALLBACK(power_action));
	g_object_set_data(G_OBJECT(hibernate), "action", const_cast<char*>("hibernate"));
	gtk_widget_set_sensitive(suspend, !app.logic.busy);
	gtk_widget_set_sensitive(hibernate, !app.logic.busy);
	menu_item(app, "Показать полный журнал", G_CALLBACK(show_log));
	menu_item(app, "Обновить", G_CALLBACK(on_refresh));
	add_battery_menu(app);
	auto* automatic = gtk_check_menu_item_new_with_label("Автозапуск при входе");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(automatic), autostart_enabled());
	g_signal_connect(automatic, "toggled", G_CALLBACK(autostart), &app);
	gtk_menu_shell_append(GTK_MENU_SHELL(app.menu), automatic);
	menu_item(app, "Выход", G_CALLBACK(on_exit));
	gtk_widget_show_all(app.menu);
	gtk_menu_popup_at_pointer(GTK_MENU(app.menu), event);
}

gboolean mouse(GtkWidget*, GdkEventButton* event, gpointer data) {
	auto& app = *static_cast<App*>(data);
	if (event->type != GDK_BUTTON_PRESS) {
		return FALSE;
	}
	if (event->button == GDK_BUTTON_PRIMARY) {
		gtk_window_begin_move_drag(GTK_WINDOW(app.window), event->button,
		                           static_cast<int>(event->x_root), static_cast<int>(event->y_root),
		                           event->time);
		return TRUE;
	}
	if (event->button == GDK_BUTTON_SECONDARY) {
		popup(app, reinterpret_cast<GdkEvent*>(event));
		return TRUE;
	}
	return FALSE;
}

gboolean on_window_close(GtkWidget*, GdkEvent*, gpointer) {
	gtk_main_quit();
	return TRUE;
}

gboolean on_key_press(GtkWidget*, GdkEventKey* event, gpointer p) {
	if (event->keyval == GDK_KEY_Menu ||
	    (event->keyval == GDK_KEY_F10 && (event->state & GDK_SHIFT_MASK))) {
		popup(*static_cast<App*>(p), reinterpret_cast<GdkEvent*>(event));
		return TRUE;
	}
	return FALSE;
}

void apply_theme() {
	auto* css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(
	    css,
	    "window, menu, dialog { background-color: #101726; color: #f0f5ff; }"
	    "textview text { background-color: #101726; color: #f0f5ff; }"
	    "button, combobox button { background-image: none; background-color: #29364a; color: "
	    "#f0f5ff; }"
	    "button:hover, menuitem:hover { background-color: #374b65; }",
	    -1, nullptr);
	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
	                                          GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(css);
}

void create_main_window(App& app) {
	app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(app.window), "Монитор энергопитания · Б4");
	gtk_window_set_decorated(GTK_WINDOW(app.window), FALSE);
	gtk_window_set_resizable(GTK_WINDOW(app.window), FALSE);
	gtk_window_set_default_size(GTK_WINDOW(app.window), 300, 200);
	app.canvas = gtk_drawing_area_new();
	gtk_widget_set_size_request(app.canvas, 300, 200);
	gtk_container_add(GTK_CONTAINER(app.window), app.canvas);
	gtk_widget_add_events(app.canvas, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(app.canvas, "draw", G_CALLBACK(draw), &app);
	g_signal_connect(app.canvas, "button-press-event", G_CALLBACK(mouse), &app);
	g_signal_connect(app.window, "delete-event", G_CALLBACK(on_window_close), nullptr);
	g_signal_connect(app.window, "key-press-event", G_CALLBACK(on_key_press), &app);
}

} // namespace

int run_ui(int argc, char** argv) {
	gtk_init(&argc, &argv);
	apply_theme();
	App app;
	create_main_window(app);
	app.logic.on_state_changed = [&app] {
		update_state(app);
	};
	app.logic.on_log_changed = [&app] {
		render_log(app);
	};
	app.logic.on_error = [&app](const std::string& text) {
		show_error(app, text);
	};

	log(app.logic, Kind::System, "Монитор запущен. Вариант Б4");
	// Подписываемся до первого снимка, чтобы не потерять событие при запуске.
	start_monitors(app.logic);
	refresh(app.logic);
	gtk_widget_show_all(app.window);
	gtk_main();
	stop_monitors(app.logic);
	return 0;
}

} // namespace power_widget
