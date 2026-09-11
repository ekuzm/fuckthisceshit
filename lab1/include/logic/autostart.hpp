#pragma once

namespace power_widget {
struct Logic;

bool autostart_enabled();
void set_autostart(Logic& app, bool enabled);
} // namespace power_widget
