#pragma once

#include <string>

namespace power_widget {
struct Logic;
enum class Kind;

void log(Logic& app, Kind kind, const std::string& message);
void report_error(Logic& app, const std::string& text);
void save_log(Logic& app, const std::string& path);
} // namespace power_widget
