#include "logic/sleep.hpp"

#include "logic/state.hpp"
#include "logic/journal.hpp"
#include <gio/gio.h>

namespace power_widget {

static void on_power_action_finished(GObject* source, GAsyncResult* result, gpointer p) {
	auto& current = *static_cast<Logic*>(p);
	auto* child = G_SUBPROCESS(source);
	gchar* stderr_text = nullptr;
	GError* failure = nullptr;
	const bool completed =
	    g_subprocess_communicate_utf8_finish(child, result, nullptr, &stderr_text, &failure);
	current.busy = false;
	if (!completed || !g_subprocess_get_successful(child)) {
		std::string reason = "неизвестная ошибка";
		if (failure) {
			reason = failure->message;
		} else if (stderr_text && *stderr_text) {
			reason = stderr_text;
		}
		log(current, Kind::Sleep, "Запрос перехода отклонён: " + reason);
		report_error(current, "Не удалось изменить режим питания: " + reason);
	} else {
		log(current, Kind::Sleep, "systemctl принял запрос перехода");
	}
	g_clear_error(&failure);
	g_free(stderr_text);
	g_object_unref(child);
	refresh(current);
}

void request_power_action(Logic& app, const char* action) {
	if (app.busy) {
		return;
	}
	log(app, Kind::Sleep, "Запрошен systemctl " + std::string(action));
	GError* error = nullptr;
	// Аргументы передаются без shell. systemctl использует штатную авторизацию logind/polkit.
	auto* process =
	    g_subprocess_new(static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_SILENCE |
		                                               G_SUBPROCESS_FLAGS_STDERR_PIPE),
		                 &error, "systemctl", action, nullptr);
	if (!process) {
		log(app, Kind::Sleep, "Не удалось вызвать systemctl: " + std::string(error->message));
		report_error(app, "Не удалось вызвать systemctl: " + std::string(error->message));
		g_clear_error(&error);
		return;
	}
	app.busy = true;
	g_subprocess_communicate_utf8_async(process, nullptr, nullptr, on_power_action_finished, &app);
}

} // namespace power_widget
