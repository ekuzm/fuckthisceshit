#pragma once

struct Logic;

// Проверяет наличие файла автозапуска приложения.
bool autostart_enabled();
// Включает или выключает автозапуск в зависимости от переданного флага.
void set_autostart(Logic& app, bool enabled);
