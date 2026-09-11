#include "logic/format.hpp"

#include <glib.h>
#include <cmath>

namespace power_widget {

std::string valid_utf8(const std::string& text) {
	gchar* result = g_utf8_make_valid(text.c_str(), static_cast<gssize>(text.size()));
	std::string copy(result);
	g_free(result);
	return copy;
}

std::string source_name(int online) {
	if (online == 1) {
		return "AC power";
	}
	if (online == 0) {
		return "Battery power";
	}
	return "Power source unknown";
}

std::string status_name(const std::string& status) {
	if (status == "Charging") {
		return "Charging";
	}
	if (status == "Discharging") {
		return "Discharging";
	}
	if (status == "Full") {
		return "Fully charged";
	}
	if (status == "Not charging") {
		return "Not charging";
	}
	return "Status unknown";
}

std::string percent(double value) {
	return value < 0 ? "unknown" : std::to_string(static_cast<int>(std::round(value))) + "%";
}

} // namespace power_widget
