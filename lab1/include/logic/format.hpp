#pragma once

#include <string>

namespace power_widget {
std::string valid_utf8(const std::string& text);
std::string source_name(int online);
std::string status_name(const std::string& status);
std::string percent(double value);
} // namespace power_widget
