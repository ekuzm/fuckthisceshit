#include "constants.hpp"
#include "logic/format.hpp"

#include <glib.h>
#include <cmath>

// Заменяет повреждённые последовательности UTF-8 символом замены и возвращает исправленный текст.
std::string valid_utf8(const std::string& text) {
	gchar* result = g_utf8_make_valid(text.c_str(), static_cast<gssize>(text.size()));
	// Копируем текст в std::string до освобождения буфера GLib через g_free.
	std::string copy(result);
	g_free(result);
	return copy;
}

// Преобразует признак внешнего питания в текст для интерфейса и журнала.
std::string source_name(int online) {
	if (online == POWER_SOURCE_ONLINE) {
		return "AC power";
	}
	if (online == POWER_SOURCE_OFFLINE) {
		return "Battery power";
	}
	return "Power source unknown";
}

// Преобразует статус батареи из sysfs в подпись; незнакомые статусы считает неизвестными.
std::string status_name(const std::string& status) {
	if (status == BATTERY_STATUS_CHARGING) {
		return BATTERY_STATUS_CHARGING;
	}
	if (status == BATTERY_STATUS_DISCHARGING) {
		return BATTERY_STATUS_DISCHARGING;
	}
	if (status == BATTERY_STATUS_FULL) {
		return "Fully charged";
	}
	if (status == BATTERY_STATUS_NOT_CHARGING) {
		return BATTERY_STATUS_NOT_CHARGING;
	}
	return "Status unknown";
}

// Округляет процент до целого и добавляет знак %; отрицательное значение показывает как неизвестное.
std::string percent(double value) {
	if (value < 0) {
		return UNKNOWN_VALUE_TEXT;
	}
	int rounded = static_cast<int>(std::round(value));
	return std::to_string(rounded) + "%";
}
