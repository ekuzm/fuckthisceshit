#include "constants.hpp"
#include "ui.hpp"
#include "logic/logic.hpp"

#include <gtk/gtk.h>

#include <algorithm>
#include <cstring>
#include <string>

// Состояние интерфейса: логика, созданные виджеты и имя выбранной батареи.
struct App {
	Logic logic;
	GtkWidget *window{}, *canvas{}, *log_window{}, *filter{}, *log_view{}, *menu{};
	std::string selected;
};

// Проверяет выбранную батарею, при её исчезновении выбирает первую доступную и запрашивает перерисовку.
static void update_state(App& app) {
	const std::vector<Battery>& batteries = app.logic.state.batteries;
	if (!find_battery(app.logic.state, app.selected)) {
		if (batteries.empty()) {
			app.selected = "";
		} else {
			app.selected = batteries[0].name;
		}
	}
	// Это запрос перерисовки: GTK позже вызовет draw, а не рисует прямо здесь.
	gtk_widget_queue_draw(app.canvas);
}

// Собирает текст событий по текущему фильтру, обновляет окно журнала и прокручивает его к концу.
static void render_log(App& app) {
	if (!app.log_view) {
		return;
	}
	// Индексы списка фильтров соответствуют порядку значений Kind; системные события видны в All.
	const Kind selected = static_cast<Kind>(gtk_combo_box_get_active(GTK_COMBO_BOX(app.filter)));
	std::string text;
	for (const Event& event : app.logic.events) {
		if (selected == Kind::All || selected == event.kind) {
			text += event.line + '\n';
		}
	}
	GtkTextBuffer* buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(app.log_view));
	gtk_text_buffer_set_text(buffer, text.c_str(), static_cast<gint>(text.size()));
	GtkTextIter end;
	gtk_text_buffer_get_end_iter(buffer, &end);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(app.log_view), &end, 0, FALSE, 0, 0);
}

// Устанавливает цвет для последующих операций рисования Cairo.
static void set_color(cairo_t* cr, const RgbColor& color) {
	cairo_set_source_rgb(cr, color.red, color.green, color.blue);
}

// Pango обеспечивает корректную кириллицу и обрезку длинных строк.
// Рисует выровненную по центру подпись на заданной высоте с обычным или приглушённым цветом.
static void label(cairo_t* cr, const std::string& text, double y, int size, bool muted = false) {
	PangoLayout* layout = pango_cairo_create_layout(cr);
	PangoFontDescription* font = pango_font_description_new();
	pango_font_description_set_family(font, "Sans");
	// Pango использует собственные единицы, поэтому размеры умножаются на PANGO_SCALE.
	pango_font_description_set_absolute_size(font, size * PANGO_SCALE);
	pango_layout_set_font_description(layout, font);
	pango_layout_set_width(layout, LABEL_WIDTH * PANGO_SCALE);
	pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
	pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
	const std::string safe = valid_utf8(text);
	pango_layout_set_text(layout, safe.c_str(), NULL_TERMINATED_TEXT);
	if (muted) {
		set_color(cr, MUTED_TEXT_COLOR);
	} else {
		set_color(cr, TEXT_COLOR);
	}
	cairo_move_to(cr, LABEL_LEFT, y);
	pango_cairo_show_layout(cr, layout);
	pango_font_description_free(font);
	g_object_unref(layout);
}

// Рисует фон кольца и цветную дугу, длина и цвет которой зависят от процента заряда.
static void draw_charge_ring(cairo_t* cr, double value) {
	cairo_set_line_width(cr, RING_LINE_WIDTH);
	set_color(cr, RING_TRACK_COLOR);
	cairo_arc(cr, RING_CENTER_X, RING_CENTER_Y, RING_RADIUS, 0, FULL_CIRCLE);
	cairo_stroke(cr);
	if (value >= 0) {
		if (value >= HIGH_CHARGE_PERCENT) {
			set_color(cr, HIGH_CHARGE_COLOR);
		} else if (value >= LOW_CHARGE_PERCENT) {
			set_color(cr, MEDIUM_CHARGE_COLOR);
		} else {
			set_color(cr, LOW_CHARGE_COLOR);
		}
		cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
		// Доля заряда задаёт долю полного оборота; начальный угол расположен сверху кольца.
		cairo_arc(cr, RING_CENTER_X, RING_CENTER_Y, RING_RADIUS, RING_START_ANGLE,
		          RING_START_ANGLE + FULL_CIRCLE * value / FULL_CHARGE_PERCENT);
		cairo_stroke(cr);
	}
}

// Готовит нижнюю подпись: статус батареи, оценку времени разряда или сообщение об отсутствии данных.
static std::string battery_detail(const Battery* battery) {
	if (!battery) {
		return "Battery data unavailable";
	}
	if (battery->status != BATTERY_STATUS_DISCHARGING) {
		return status_name(battery->status);
	}
	if (battery->minutes < 0) {
		return "Remaining time unknown";
	}
	// Ограничиваем огромное значение перед преобразованием в целое, дробную часть отбрасываем.
	const long long minutes = static_cast<long long>(std::min(battery->minutes, MAX_DISPLAY_MINUTES));
	return "Time left ≈ " + std::to_string(minutes) + " min";
}

// Обработчик перерисовки GTK: рисует фон, кольцо и подписи по уже сохранённым показаниям.
static gboolean draw(GtkWidget* widget, cairo_t* cr, gpointer data) {
	// GTK возвращает переданный в g_signal_connect указатель &app через gpointer; восстанавливаем App.
	App& app = *static_cast<App*>(data);
	// Масштабируем координаты макета под фактически выделенную GTK область рисования.
	cairo_scale(cr, gtk_widget_get_allocated_width(widget) / static_cast<double>(WIDGET_WIDTH),
	            gtk_widget_get_allocated_height(widget) / static_cast<double>(WIDGET_HEIGHT));
	set_color(cr, BACKGROUND_COLOR);
	cairo_paint(cr);
	const Battery* battery = find_battery(app.logic.state, app.selected);
	double value = UNKNOWN_READING;
	std::string battery_name = "No battery";
	if (battery != nullptr) {
		value = battery->percent;
		battery_name = battery->name;
	}
	std::string charge_text = "—";
	if (value >= 0) {
		charge_text = percent(value);
	}
	draw_charge_ring(cr, value);
	label(cr, charge_text, CHARGE_LABEL_Y, CHARGE_FONT_SIZE);
	label(cr, battery_name, BATTERY_LABEL_Y, SMALL_FONT_SIZE, true);
	label(cr, source_name(app.logic.state.online), SOURCE_LABEL_Y, SOURCE_FONT_SIZE);
	label(cr, battery_detail(battery), DETAIL_LABEL_Y, DETAIL_FONT_SIZE, true);
	label(cr, "Left drag: move · Right click: menu", HINT_LABEL_Y, SMALL_FONT_SIZE, true);
	return TRUE;
}

// Показывает модальное окно с текстом ошибки и уничтожает его после закрытия.
static void show_error(App& app, const std::string& text) {
	GtkWidget* dialog =
	    gtk_message_dialog_new(GTK_WINDOW(app.window), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
		                       GTK_BUTTONS_CLOSE, "%s", valid_utf8(text).c_str());
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

// Предлагает выбрать файл и передаёт выбранный путь функции сохранения полного журнала.
static void on_save_log(GtkButton*, gpointer data) {
	App& app = *static_cast<App*>(data);
	GtkWidget* dialog = gtk_file_chooser_dialog_new(
	    "Save full log", GTK_WINDOW(app.log_window), GTK_FILE_CHOOSER_ACTION_SAVE,
	    "Cancel", GTK_RESPONSE_CANCEL, "Save", GTK_RESPONSE_ACCEPT, nullptr);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "power-report.txt");
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		gchar* path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		save_log(app.logic, path);
		g_free(path);
	}
	gtk_widget_destroy(dialog);
}

// Перестраивает отображаемый журнал после выбора категории событий.
static void on_filter_changed(GtkComboBox*, gpointer data) {
	render_log(*static_cast<App*>(data));
}

// Создаёт отдельное окно журнала с фильтром, прокруткой и кнопкой сохранения.
static void create_log_window(App& app) {
	app.log_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(app.log_window), "Power log");
	gtk_window_set_default_size(GTK_WINDOW(app.log_window), LOG_WINDOW_WIDTH, LOG_WINDOW_HEIGHT);
	// Закрытие журнала скрывает его, виджет и мониторинг продолжают работать.
	g_signal_connect(app.log_window, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete),
	                 nullptr);
	GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, LOG_BOX_SPACING);
	gtk_container_set_border_width(GTK_CONTAINER(box), LOG_BORDER_WIDTH);
	gtk_container_add(GTK_CONTAINER(app.log_window), box);
	app.filter = gtk_combo_box_text_new();
	for (const char* text :
	     {"All events", "Charger connected / disconnected", "Charge changes", "Sleep transitions"}) {
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(app.filter), text);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(app.filter), static_cast<int>(Kind::All));
	g_signal_connect(app.filter, "changed", G_CALLBACK(on_filter_changed), &app);
	gtk_box_pack_start(GTK_BOX(box), app.filter, FALSE, FALSE, NO_BOX_PADDING);
	GtkWidget* scroll = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, NO_BOX_PADDING);
	app.log_view = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(app.log_view), FALSE);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(app.log_view), FALSE);
	gtk_text_view_set_monospace(GTK_TEXT_VIEW(app.log_view), TRUE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(app.log_view), GTK_WRAP_WORD_CHAR);
	gtk_container_add(GTK_CONTAINER(scroll), app.log_view);
	GtkWidget* save = gtk_button_new_with_label("Save full log…");
	g_signal_connect(save, "clicked", G_CALLBACK(on_save_log), &app);
	gtk_box_pack_start(GTK_BOX(box), save, FALSE, FALSE, NO_BOX_PADDING);
}

// При первом вызове создаёт окно журнала, затем показывает его и обновляет текст.
static void show_log(GtkMenuItem*, gpointer data) {
	App& app = *static_cast<App*>(data);
	if (!app.log_window) {
		create_log_window(app);
	}
	gtk_widget_show_all(app.log_window);
	render_log(app);
	gtk_window_present(GTK_WINDOW(app.log_window));
}

// Уточняет действие пункта меню, запрашивает подтверждение и передаёт запрос сна или гибернации логике.
static void power_action(GtkMenuItem* item, gpointer data) {
	App& app = *static_cast<App*>(data);
	if (app.logic.busy) {
		return;
	}
	const char* action = static_cast<const char*>(g_object_get_data(G_OBJECT(item), "action"));
	const char* question = "Hibernate the computer?";
	if (std::strcmp(action, "suspend") == 0) {
		question = "Suspend the computer?";
	}
	GtkWidget* dialog = gtk_message_dialog_new(
	    GTK_WINDOW(app.window), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
	    "%s", question);
	const bool accepted = gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK;
	gtk_widget_destroy(dialog);
	if (!accepted) {
		return;
	}
	request_power_action(app.logic, action);
}

// Передаёт состояние галочки меню функции настройки автозапуска.
static void autostart(GtkCheckMenuItem* item, gpointer data) {
	App& app = *static_cast<App*>(data);
	set_autostart(app.logic, gtk_check_menu_item_get_active(item));
}

// Добавляет пункт в контекстное меню и связывает его выбор с переданным обработчиком.
static GtkWidget* menu_item(App& app, const char* text, GCallback callback) {
	GtkWidget* item = gtk_menu_item_new_with_label(text);
	gtk_menu_shell_append(GTK_MENU_SHELL(app.menu), item);
	g_signal_connect(item, "activate", callback, &app);
	return item;
}

// Сохраняет имя выбранной батареи из пункта меню и запрашивает перерисовку виджета.
static void on_battery_selected(GtkMenuItem* selected, gpointer p) {
	App& current = *static_cast<App*>(p);
	current.selected = static_cast<const char*>(g_object_get_data(G_OBJECT(selected), "battery"));
	gtk_widget_queue_draw(current.canvas);
}

// При нескольких батареях добавляет подменю выбора и отмечает текущую батарею.
static void add_battery_menu(App& app) {
	if (app.logic.state.batteries.size() <= 1) {
		return;
	}
	GtkWidget* choose = gtk_menu_item_new_with_label("Battery");
	GtkWidget* submenu = gtk_menu_new();
	for (const Battery& battery : app.logic.state.batteries) {
		std::string title = battery.name;
		if (battery.name == app.selected) {
			title += " ✓";
		}
		GtkWidget* item = gtk_menu_item_new_with_label(title.c_str());
		// Пункт хранит отдельную копию имени: g_free освободит её при уничтожении пункта меню.
		g_object_set_data_full(G_OBJECT(item), "battery", g_strdup(battery.name.c_str()), g_free);
		g_signal_connect(item, "activate", G_CALLBACK(on_battery_selected), &app);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
	}
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(choose), submenu);
	gtk_menu_shell_append(GTK_MENU_SHELL(app.menu), choose);
}

// Выполняет ручное обновление показаний по команде меню.
static void on_refresh(GtkMenuItem*, gpointer data) {
	refresh(static_cast<App*>(data)->logic);
}

// Просит главный цикл GTK завершиться; освобождение ресурсов продолжится в run_ui.
static void on_exit(GtkMenuItem*, gpointer) {
	gtk_main_quit();
}

// Заново собирает контекстное меню с актуальными состояниями и показывает его у указателя.
static void popup(App& app, GdkEvent* event) {
	if (app.menu) {
		gtk_widget_destroy(app.menu);
	}
	app.menu = gtk_menu_new();
	GtkWidget* suspend = menu_item(app, "Suspend", G_CALLBACK(power_action));
	// Один обработчик обслуживает два пункта; поле action указывает, какую команду выполнить.
	g_object_set_data(G_OBJECT(suspend), "action", const_cast<char*>("suspend"));
	GtkWidget* hibernate = menu_item(app, "Hibernate", G_CALLBACK(power_action));
	g_object_set_data(G_OBJECT(hibernate), "action", const_cast<char*>("hibernate"));
	gtk_widget_set_sensitive(suspend, !app.logic.busy);
	gtk_widget_set_sensitive(hibernate, !app.logic.busy);
	menu_item(app, "Show full log", G_CALLBACK(show_log));
	menu_item(app, "Refresh", G_CALLBACK(on_refresh));
	add_battery_menu(app);
	GtkWidget* automatic = gtk_check_menu_item_new_with_label("Start automatically at login");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(automatic), autostart_enabled());
	g_signal_connect(automatic, "toggled", G_CALLBACK(autostart), &app);
	gtk_menu_shell_append(GTK_MENU_SHELL(app.menu), automatic);
	menu_item(app, "Quit", G_CALLBACK(on_exit));
	gtk_widget_show_all(app.menu);
	gtk_menu_popup_at_pointer(GTK_MENU(app.menu), event);
}

// Начинает перетаскивание окна левой кнопкой или открывает меню правой кнопкой.
static gboolean mouse(GtkWidget*, GdkEventButton* event, gpointer data) {
	App& app = *static_cast<App*>(data);
	if (event->type != GDK_BUTTON_PRESS) {
		return FALSE;
	}
	if (event->button == GDK_BUTTON_PRIMARY) {
		// Перемещение поручаем оконной системе; x_root/y_root — координаты указателя на экране.
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

// Останавливает главный цикл при закрытии основного окна.
static gboolean on_window_close(GtkWidget*, GdkEvent*, gpointer) {
	gtk_main_quit();
	return TRUE;
}

// Открывает контекстное меню по клавише Menu или сочетанию Shift+F10.
static gboolean on_key_press(GtkWidget*, GdkEventKey* event, gpointer p) {
	if (event->keyval == GDK_KEY_Menu ||
	    (event->keyval == GDK_KEY_F10 && (event->state & GDK_SHIFT_MASK))) {
		popup(*static_cast<App*>(p), reinterpret_cast<GdkEvent*>(event));
		return TRUE;
	}
	return FALSE;
}

// Загружает CSS из констант и применяет оформление к GTK-интерфейсу приложения.
static void apply_theme() {
	GtkCssProvider* css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(
	    css,
	    GTK_THEME_CSS, NULL_TERMINATED_TEXT, nullptr);
	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
	                                          GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref(css);
}

// Создаёт окно без рамки и область рисования, подключает обработчики рисования, мыши и клавиатуры.
static void create_main_window(App& app) {
	app.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(app.window), "Power Monitor · B4");
	gtk_window_set_decorated(GTK_WINDOW(app.window), FALSE);
	gtk_window_set_resizable(GTK_WINDOW(app.window), FALSE);
	gtk_window_set_default_size(GTK_WINDOW(app.window), WIDGET_WIDTH, WIDGET_HEIGHT);
	app.canvas = gtk_drawing_area_new();
	gtk_widget_set_size_request(app.canvas, WIDGET_WIDTH, WIDGET_HEIGHT);
	gtk_container_add(GTK_CONTAINER(app.window), app.canvas);
	gtk_widget_add_events(app.canvas, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(app.canvas, "draw", G_CALLBACK(draw), &app);
	g_signal_connect(app.canvas, "button-press-event", G_CALLBACK(mouse), &app);
	g_signal_connect(app.window, "delete-event", G_CALLBACK(on_window_close), nullptr);
	g_signal_connect(app.window, "key-press-event", G_CALLBACK(on_key_press), &app);
}

// Инициализирует GTK, связывает интерфейс с логикой, запускает цикл событий и освобождает подписки при выходе.
int run_ui(int argc, char** argv) {
	gtk_init(&argc, &argv);
	apply_theme();
	App app;
	create_main_window(app);
	// Лямбды — функции обратного вызова; [&app] даёт им доступ к существующему App по ссылке.
	// App живёт до выхода из run_ui, включая весь главный цикл.
	app.logic.on_state_changed = [&app] {
		update_state(app);
	};
	app.logic.on_log_changed = [&app] {
		render_log(app);
	};
	app.logic.on_error = [&app](const std::string& text) {
		show_error(app, text);
	};

	log(app.logic, Kind::System, "Monitor started. Variant B4");
	// Подписываемся до первого снимка, чтобы не потерять событие при запуске.
	start_monitors(app.logic);
	refresh(app.logic);
	gtk_widget_show_all(app.window);
	// Главный цикл ждёт события GTK, udev и D-Bus и вызывает обработчики; выйдет после gtk_main_quit.
	gtk_main();
	stop_monitors(app.logic);
	return 0;
}
