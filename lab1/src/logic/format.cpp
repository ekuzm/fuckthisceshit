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
    if (online == 1) return "Работа от сети";
    if (online == 0) return "Работа от батареи";
    return "Источник питания неизвестен";
}

std::string status_name(const std::string& status) {
    if (status == "Charging") return "Заряжается";
    if (status == "Discharging") return "Разряжается";
    if (status == "Full") return "Полностью заряжена";
    if (status == "Not charging") return "Не заряжается";
    return "Состояние неизвестно";
}

std::string percent(double value) {
    return value < 0 ? "неизвестен" : std::to_string(static_cast<int>(std::round(value))) + "%";
}

} // namespace power_widget
