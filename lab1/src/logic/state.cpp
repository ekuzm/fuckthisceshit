#include "logic/state.hpp"

#include "logic/journal.hpp"
#include "logic/format.hpp"
#include <utility>

namespace power_widget {

static void log_power_changes(Logic& app, const PowerState& next) {
	if (!next.error.empty() && next.error != app.state.error) {
		log(app, Kind::System, next.error);
	}
	if (next.error.empty() && !app.state.error.empty()) {
		log(app, Kind::System, "Чтение sysfs восстановлено");
	}
	if (!app.initialized || app.state.online != next.online) {
		log(app, Kind::Charger, source_name(next.online));
	}
	for (const auto& battery : next.batteries) {
		const Battery* old = find_battery(app.state, battery.name);
		if (!old) {
			log(app, Kind::System, "Обнаружена батарея " + battery.name);
		}
		if (!old || old->percent != battery.percent) {
			log(app, Kind::Charge, battery.name + ": заряд " + percent(battery.percent));
		}
		if (!old || old->status != battery.status) {
			log(app, Kind::Charge, battery.name + ": " + status_name(battery.status));
		}
	}
	for (const auto& battery : app.state.batteries) {
		if (!find_battery(next, battery.name)) {
			log(app, Kind::System, "Батарея удалена: " + battery.name);
		}
	}
	if (!app.initialized && next.batteries.empty()) {
		log(app, Kind::System, "Батарея не обнаружена");
	}
}

void refresh(Logic& app) {
	auto next = read_power();
	log_power_changes(app, next);
	app.state = std::move(next);
	app.initialized = true;
	if (app.on_state_changed) {
		app.on_state_changed();
	}
}

} // namespace power_widget
