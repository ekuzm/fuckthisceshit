#include "logic/journal.hpp"

#include "logic/state.hpp"
#include "logic/format.hpp"
#include <gio/gio.h>

namespace power_widget {

void report_error(Logic& app, const std::string& text) {
    log(app, Kind::System, text);
    if (app.on_error) app.on_error(text);
}

static std::string stamp() {
    GDateTime* now = g_date_time_new_now_local();
    gchar* result = g_date_time_format(now, "%Y-%m-%d %H:%M:%S %z");
    std::string copy(result);
    g_free(result);
    g_date_time_unref(now);
    return copy;
}

static const char* kind_name(Kind kind) {
    switch (kind) {
    case Kind::Charger: return "Питание";
    case Kind::Charge: return "Заряд";
    case Kind::Sleep: return "Сон";
    default: return "Система";
    }
}

void log(Logic& app, Kind kind, const std::string& message) {
    app.events.push_back({kind, stamp() + " [" + kind_name(kind) + "] " + valid_utf8(message)});
    if (app.on_log_changed) app.on_log_changed();
}

void save_log(Logic& app, const std::string& path) {
    std::string text = "Лабораторная работа №1. Вариант Б4\nЖурнал энергопитания\n\n";
    for (const auto& event : app.events) text += event.line + '\n';
    GError* error = nullptr;
    if (!g_file_set_contents(path.c_str(), text.c_str(), static_cast<gssize>(text.size()), &error)) {
        report_error(app, "Не удалось сохранить журнал: " + std::string(error->message));
        g_clear_error(&error);
    } else log(app, Kind::System, "Журнал сохранён: " + std::string(path));
}

} // namespace power_widget
