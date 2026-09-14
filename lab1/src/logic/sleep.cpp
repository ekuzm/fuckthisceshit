#include "logic/sleep.hpp"

#include "logic/state.hpp"
#include "logic/journal.hpp"
#include <gio/gio.h>

// Получает результат асинхронной команды systemctl, сообщает об ошибке и обновляет показания.
static void on_power_action_finished(GObject* source, GAsyncResult* result, gpointer p) {
	Logic& current = *static_cast<Logic*>(p);
	GSubprocess* child = G_SUBPROCESS(source);
	gchar* stderr_text = nullptr;
	GError* failure = nullptr;
	// Завершаем получение вывода процесса; отдельно проверяем успешный код завершения systemctl.
	const bool completed =
	    g_subprocess_communicate_utf8_finish(child, result, nullptr, &stderr_text, &failure);
	current.busy = false;
	if (!completed || !g_subprocess_get_successful(child)) {
		std::string reason = "unknown error";
		if (failure) {
			reason = failure->message;
		} else if (stderr_text && *stderr_text) {
			reason = stderr_text;
		}
		log(current, Kind::Sleep, "Power state change request rejected: " + reason);
		report_error(current, "Failed to change power state: " + reason);
	} else {
		// Успех команды означает принятие запроса; фактический переход отслеживается через logind.
		log(current, Kind::Sleep, "systemctl accepted the power state change request");
	}
	g_clear_error(&failure);
	g_free(stderr_text);
	g_object_unref(child);
	refresh(current);
}

// Запускает systemctl suspend/hibernate без ожидания в UI; повторный запрос во время выполнения пропускает.
void request_power_action(Logic& app, const char* action) {
	if (app.busy) {
		return;
	}
	log(app, Kind::Sleep, "Requested systemctl " + std::string(action));
	GError* error = nullptr;
	// Аргументы передаются без shell. systemctl использует штатную авторизацию logind/polkit.
	GSubprocess* process =
	    g_subprocess_new(static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
		                                               G_SUBPROCESS_FLAGS_STDERR_PIPE),
		                 &error, "systemctl", action, nullptr);
	if (!process) {
		log(app, Kind::Sleep, "Failed to run systemctl: " + std::string(error->message));
		report_error(app, "Failed to run systemctl: " + std::string(error->message));
		g_clear_error(&error);
		return;
	}
	app.busy = true;
	// Ожидание не блокирует UI; результат придёт в on_power_action_finished, где освободим процесс.
	g_subprocess_communicate_utf8_async(process, nullptr, nullptr, on_power_action_finished, &app);
}
