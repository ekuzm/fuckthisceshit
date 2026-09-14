#include "constants.hpp"
#include "logic/autostart.hpp"

#include "logic/state.hpp"
#include "logic/journal.hpp"
#include <gio/gio.h>
#include <cerrno>
#include <cstring>

// Возвращает путь к .desktop-файлу в пользовательском каталоге автозапуска.
static std::string autostart_path() {
	return std::string(g_get_user_config_dir()) + "/autostart/lab1-power-widget.desktop";
}

// Удаляет .desktop-файл; уже отсутствующий файл также считается отключённым автозапуском.
static void disable_autostart(Logic& app) {
	const std::string path = autostart_path();
	GError* error = nullptr;
	GFile* file = g_file_new_for_path(path.c_str());
	const bool removed = g_file_delete(file, nullptr, &error);
	g_object_unref(file);
	// Отсутствие файла — нормальный результат отключения; остальные ошибки показываем пользователю.
	if (!removed && !g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
		report_error(app, "Failed to disable autostart: " + std::string(error->message));
		g_clear_error(&error);
		return;
	}
	g_clear_error(&error);
	log(app, Kind::System, "Autostart disabled");
}

// Находит исполняемый файл и создаёт .desktop-запись для запуска при входе пользователя.
static void enable_autostart(Logic& app) {
	const std::string path = autostart_path();
	GError* error = nullptr;
	gchar* executable = g_file_read_link("/proc/self/exe", &error);
	if (!executable) {
		report_error(app, "Failed to determine application path: " + std::string(error->message));
		g_clear_error(&error);
		return;
	}
	// Экранирование Exec по Desktop Entry Specification, затем GKeyFile
	// экранирует значение для формата самого .desktop-файла.
	std::string quoted = "\"";
	for (const char ch : std::string(executable)) {
		if (ch == '%') {
			quoted += "%%";
		} else {
			if (ch == '\\' || ch == '"' || ch == '`' || ch == '$') {
				quoted += '\\';
			}
			quoted += ch;
		}
	}
	quoted += '"';
	g_free(executable);
	const std::string dir = std::string(g_get_user_config_dir()) + "/autostart";
	if (g_mkdir_with_parents(dir.c_str(), AUTOSTART_DIRECTORY_MODE) != 0) {
		report_error(app, "Failed to create autostart directory: " +
		                      std::string(std::strerror(errno)));
		return;
	}
	// GKeyFile записывает поля секции Desktop Entry и экранирует их для формата .desktop.
	GKeyFile* entry = g_key_file_new();
	g_key_file_set_string(entry, "Desktop Entry", "Type", "Application");
	g_key_file_set_string(entry, "Desktop Entry", "Name", "Power Monitor");
	g_key_file_set_string(entry, "Desktop Entry", "Exec", quoted.c_str());
	g_key_file_set_boolean(entry, "Desktop Entry", "Terminal", FALSE);
	g_key_file_set_boolean(entry, "Desktop Entry", "X-GNOME-Autostart-enabled", TRUE);
	if (!g_key_file_save_to_file(entry, path.c_str(), &error)) {
		report_error(app, "Failed to save autostart entry: " + std::string(error->message));
		g_clear_error(&error);
	} else {
		log(app, Kind::System, "Autostart enabled: " + path);
	}
	g_key_file_unref(entry);
}

// Проверяет наличие файла автозапуска приложения.
bool autostart_enabled() {
	return g_file_test(autostart_path().c_str(), G_FILE_TEST_EXISTS);
}

// Включает или выключает автозапуск в зависимости от переданного флага.
void set_autostart(Logic& app, bool enabled) {
	if (enabled) {
		enable_autostart(app);
	} else {
		disable_autostart(app);
	}
}
