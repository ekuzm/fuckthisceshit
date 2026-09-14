#include "logic/state.hpp"

#include "logic/journal.hpp"
#include "logic/format.hpp"
#include <utility>

// Сравнивает старый и новый снимки: записывает изменения питания, батарей и ошибок чтения.
static void log_power_changes(Logic& app, const PowerState& next) {
	if (!next.error.empty() && next.error != app.state.error) {
		log(app, Kind::System, next.error);
	}
	if (next.error.empty() && !app.state.error.empty()) {
		log(app, Kind::System, "Reading from sysfs restored");
	}
	if (!app.initialized || app.state.online != next.online) {
		log(app, Kind::Charger, source_name(next.online));
	}
	for (const Battery& battery : next.batteries) {
		const Battery* old = find_battery(app.state, battery.name);
		if (!old) {
			log(app, Kind::System, "Battery detected: " + battery.name);
		}
		// Короткое замыкание ||: если старой батареи нет, к old->percent не обращаемся.
		if (!old || old->percent != battery.percent) {
			log(app, Kind::Charge, battery.name + ": charge " + percent(battery.percent));
		}
		if (!old || old->status != battery.status) {
			log(app, Kind::Charge, battery.name + ": " + status_name(battery.status));
		}
	}
	// Обратный проход обнаруживает батареи, которые были раньше, но исчезли из нового снимка.
	for (const Battery& battery : app.state.batteries) {
		if (!find_battery(next, battery.name)) {
			log(app, Kind::System, "Battery removed: " + battery.name);
		}
	}
	if (!app.initialized && next.batteries.empty()) {
		log(app, Kind::System, "No battery detected");
	}
}

// Читает новый снимок, записывает изменения в журнал, сохраняет данные и уведомляет интерфейс.
void refresh(Logic& app) {
	PowerState next = read_power();
	log_power_changes(app, next);
	// Передаём содержимое нового снимка в состояние без копирования списков; next дальше не используем.
	app.state = std::move(next);
	app.initialized = true;
	if (app.on_state_changed) {
		app.on_state_changed();
	}
}
